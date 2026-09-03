#include "sd_card.h"

#include <Arduino.h>

#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"

#include "../boards/board.h"

namespace sdcard {

static const char* kMount = "/sdcard";
static sdmmc_card_t* s_card = nullptr;
static uint32_t s_busKHz = 0;

// Bring a card back to a known state without a power cycle. If the ESP32
// reset in the middle of a multi-block read, the card is still streaming
// data and will answer every command with garbage until it gets CMD12
// (STOP_TRANSMISSION). Bit-bang that at ~100 kHz before the driver takes
// over: 80 idle clocks with CS high, CMD12, drain, 80 more idle clocks.
static void recoverCard() {
    pinMode(PIN_SD_CS, OUTPUT);
    pinMode(PIN_SD_SCK, OUTPUT);
    pinMode(PIN_SD_MOSI, OUTPUT);
    pinMode(PIN_SD_MISO, INPUT_PULLUP);

    auto clockByte = [](uint8_t v) {
        for (int i = 7; i >= 0; i--) {
            digitalWrite(PIN_SD_MOSI, (v >> i) & 1);
            digitalWrite(PIN_SD_SCK, HIGH);
            delayMicroseconds(4);
            digitalWrite(PIN_SD_SCK, LOW);
            delayMicroseconds(4);
        }
    };

    auto readByte = [&]() -> uint8_t {
        uint8_t v = 0;
        for (int i = 0; i < 8; i++) {
            digitalWrite(PIN_SD_MOSI, HIGH);
            digitalWrite(PIN_SD_SCK, HIGH);
            delayMicroseconds(4);
            v = (v << 1) | (digitalRead(PIN_SD_MISO) ? 1 : 0);
            digitalWrite(PIN_SD_SCK, LOW);
            delayMicroseconds(4);
        }
        return v;
    };
    // Send a command and return the first non-0xFF response byte (R1), or
    // 0xFF if the card never answered within 32 bytes.
    auto command = [&](uint8_t idx, uint32_t arg, uint8_t crc) -> uint8_t {
        clockByte(0xFF);
        clockByte(0x40 | idx);
        clockByte(arg >> 24); clockByte(arg >> 16); clockByte(arg >> 8); clockByte(arg);
        clockByte(crc);
        uint8_t r = 0xFF;
        for (int i = 0; i < 32 && r == 0xFF; i++) r = readByte();
        return r;
    };

    digitalWrite(PIN_SD_CS, HIGH);
    digitalWrite(PIN_SD_SCK, LOW);
    for (int i = 0; i < 10; i++) clockByte(0xFF);

    digitalWrite(PIN_SD_CS, LOW);
    // If the card is streaming a multi-block read, it needs CMD12. Drain a
    // couple of blocks' worth of clocks so any in-flight data finishes,
    // send CMD12 twice, then clock through the R1b busy period.
    for (int i = 0; i < 1100; i++) clockByte(0xFF);
    uint8_t r12a = command(12, 0, 0x61);
    for (int i = 0; i < 64; i++) clockByte(0xFF);
    uint8_t r12b = command(12, 0, 0x61);
    for (int i = 0; i < 64; i++) clockByte(0xFF);
    digitalWrite(PIN_SD_CS, HIGH);
    for (int i = 0; i < 10; i++) clockByte(0xFF);

    // Now GO_IDLE_STATE. A healthy card answers 0x01.
    digitalWrite(PIN_SD_CS, LOW);
    uint8_t r0 = command(0, 0, 0x95);
    for (int i = 0; i < 8; i++) clockByte(0xFF);
    digitalWrite(PIN_SD_CS, HIGH);
    for (int i = 0; i < 10; i++) clockByte(0xFF);
    log_i("sd recover: CMD12 -> 0x%02x 0x%02x, CMD0 -> 0x%02x (want 0x01)", r12a, r12b, r0);

    // Release the pins so the SPI peripheral can claim them.
    pinMode(PIN_SD_SCK, INPUT);
    pinMode(PIN_SD_MOSI, INPUT);
    pinMode(PIN_SD_CS, INPUT);
}

bool mount() {
    if (s_card) return true;
    recoverCard();

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = VSPI_HOST;
    host.max_freq_khz = SD_SPI_HZ / 1000;

    spi_bus_config_t bus = {};
    bus.mosi_io_num = PIN_SD_MOSI;
    bus.miso_io_num = PIN_SD_MISO;
    bus.sclk_io_num = PIN_SD_SCK;
    bus.quadwp_io_num = -1;
    bus.quadhd_io_num = -1;
    bus.max_transfer_sz = 16384;
    esp_err_t err = spi_bus_initialize(VSPI_HOST, &bus, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        log_e("spi_bus_initialize failed: 0x%x", err);
        return false;
    }

    sdspi_device_config_t slot = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot.gpio_cs = (gpio_num_t)PIN_SD_CS;
    slot.host_id = VSPI_HOST;

    esp_vfs_fat_sdmmc_mount_config_t cfg = {};
    cfg.format_if_mount_failed = false;
    cfg.max_files = 4;
    cfg.allocation_unit_size = 16 * 1024;

    // A soft reset does not power-cycle the card. If it was mid-transfer
    // when the ESP32 rebooted, the first init attempt can fail with
    // ESP_ERR_INVALID_CRC or a timeout. Retry a few times; the driver sends
    // CMD0 each attempt, which brings the card back to idle.
    for (int attempt = 1; attempt <= 5; attempt++) {
        err = esp_vfs_fat_sdspi_mount(kMount, &host, &slot, &cfg, &s_card);
        if (err == ESP_OK) break;
        log_w("sdspi mount attempt %d failed: 0x%x", attempt, err);
        s_card = nullptr;
        delay(100 * attempt);
    }
    if (err != ESP_OK) {
        log_e("esp_vfs_fat_sdspi_mount failed: 0x%x", err);
        spi_bus_free(VSPI_HOST);
        return false;
    }

    // The sdspi driver stops at 20 MHz over SPI because it never enables the
    // card's high-speed mode. The bus and card both cope with 40 MHz on the
    // CYD (4096/4096 clean sectors in the bench), so push it.
    s_busKHz = s_card->max_freq_khz;
    uint32_t want = SD_SPI_HZ / 1000;
    if (want > s_busKHz) {
        if (sdspi_host_set_card_clk((sdspi_dev_handle_t)s_card->host.slot, want) == ESP_OK) {
            s_busKHz = want;
        } else {
            log_w("could not raise SD clock to %u kHz, staying at %u", want, s_busKHz);
        }
    }
    return true;
}

void unmount() {
    if (!s_card) return;
    esp_vfs_fat_sdcard_unmount(kMount, s_card);
    spi_bus_free(VSPI_HOST);
    s_card = nullptr;
    s_busKHz = 0;
}

bool isMounted() { return s_card != nullptr; }

uint32_t capacityMB() {
    if (!s_card) return 0;
    return (uint32_t)(((uint64_t)s_card->csd.capacity * s_card->csd.sector_size) / (1024 * 1024));
}

uint32_t busKHz() { return s_busKHz; }

const char* mountPoint() { return kMount; }

}  // namespace sdcard
