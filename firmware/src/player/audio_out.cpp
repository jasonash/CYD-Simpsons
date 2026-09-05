#include "audio_out.h"

#include <Arduino.h>
#include <string.h>

#include "driver/dac.h"
#include "driver/i2s.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "../boards/board.h"

namespace audio {

static const i2s_port_t kPort = I2S_NUM_0;
static const int kDmaBufCount = 8;
static const int kDmaBufFrames = 256;   // stereo frames per DMA buffer

static bool s_running = false;
static uint32_t s_rate = 0;
static QueueHandle_t s_events = nullptr;
static uint64_t s_written = 0;        // mono samples handed to i2s_write
static uint64_t s_descDone = 0;       // samples worth of DMA descriptors completed
static uint32_t s_underruns = 0;
static const uint8_t kDefaultVolume = 25;   // the CYD amp is loud (2026-09-05)
static uint8_t s_volume = kDefaultVolume;
static int s_gain = kDefaultVolume * 256 / 100;   // 8.8 fixed point

// Staging buffer: u8 mono -> 16-bit stereo, both channels identical.
static uint16_t s_stage[kDmaBufFrames * 2];

static void drainEvents();
static int s_isrCore = -1;

void setIsrCore(int core) { s_isrCore = core; }
int isrCore() { return s_isrCore; }

struct InstallArgs {
    i2s_config_t* cfg;
    esp_err_t err;
    TaskHandle_t caller;
};

static void installTask(void* p) {
    InstallArgs* a = (InstallArgs*)p;
    a->err = i2s_driver_install(kPort, a->cfg, kDmaBufCount * 4, &s_events);
    xTaskNotifyGive(a->caller);
    vTaskDelete(nullptr);
}

// Install the driver from a task pinned to the requested core so the ISR
// lands there; otherwise install inline.
static esp_err_t installOn(int core, i2s_config_t* cfg) {
    if (core < 0 || core == xPortGetCoreID()) {
        return i2s_driver_install(kPort, cfg, kDmaBufCount * 4, &s_events);
    }
    InstallArgs a = {cfg, ESP_FAIL, xTaskGetCurrentTaskHandle()};
    if (xTaskCreatePinnedToCore(installTask, "i2s_inst", 3072, &a, 5, nullptr, core) != pdPASS) {
        return ESP_FAIL;
    }
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    return a.err;
}

bool begin(uint32_t sampleRate, bool useApll) {
    if (s_running) end();

    i2s_config_t cfg = {};
    cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN);
    cfg.sample_rate = sampleRate;
    cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    cfg.communication_format = I2S_COMM_FORMAT_STAND_MSB;
    cfg.intr_alloc_flags = 0;
    cfg.dma_buf_count = kDmaBufCount;
    cfg.dma_buf_len = kDmaBufFrames;
    cfg.use_apll = useApll;
    cfg.tx_desc_auto_clear = true;   // output zeros on underrun, not a repeat

    esp_err_t err = installOn(s_isrCore, &cfg);
    if (err != ESP_OK) {
        log_e("i2s_driver_install failed 0x%x", err);
        return false;
    }
    // DAC channel 2 is GPIO26 (the CYD amp input); "left" in the driver's
    // naming. Do NOT call i2s_set_pin(port, NULL) here: it enables both DAC
    // channels, and channel 1 is GPIO25, the touch panel's bit-banged clock.
    // With DAC1 driving that pad the XPT2046 reads as permanently pressed
    // (z=4095) whenever audio is running (found 2026-09-05).
    i2s_set_dac_mode(I2S_DAC_CHANNEL_LEFT_EN);
    dac_output_disable(DAC_CHANNEL_1);
    i2s_zero_dma_buffer(kPort);

    s_rate = sampleRate;
    s_written = s_descDone = 0;
    s_underruns = 0;
    s_running = true;

    // Fill the whole DMA ring with silence. From here on every write lands
    // in the descriptor that just finished playing, so a sample written at
    // time T plays one ring-depth later, and the clock arithmetic in
    // samplesPlayed() holds. Also drain whatever events the install produced.
    static uint8_t silence[kDmaBufFrames];
    memset(silence, 128, sizeof(silence));
    for (int i = 0; i < kDmaBufCount; i++) write(silence, kDmaBufFrames);
    s_written = 0;
    drainEvents();
    s_descDone = 0;
    s_underruns = 0;
    return true;
}

void end() {
    if (!s_running) return;
    i2s_driver_uninstall(kPort);
    s_events = nullptr;
    s_running = false;
    dacWrite(PIN_AUDIO_DAC, 128);
}

bool isRunning() { return s_running; }

// The legacy driver posts TX_DONE when a descriptor finishes and the app has
// been keeping up, and TX_Q_OVF when it finishes but the previous free
// descriptor was never consumed (underrun). Either way one descriptor of
// wall time passed, so both advance the clock.
// Only ever called from the task that owns the clock (the decoder); write()
// may run on another core and must not touch the event queue.
static void drainEvents() {
    if (!s_events) return;
    i2s_event_t ev;
    while (xQueueReceive(s_events, &ev, 0) == pdTRUE) {
        if (ev.type == I2S_EVENT_TX_DONE) {
            s_descDone += kDmaBufFrames;
        } else if (ev.type == I2S_EVENT_TX_Q_OVF) {
            s_descDone += kDmaBufFrames;
            s_underruns++;
        }
    }
}

void setVolume(uint8_t percent) {
    if (percent > 100) percent = 100;
    s_volume = percent;
    s_gain = percent * 256 / 100;
}

uint8_t volume() { return s_volume; }

size_t write(const uint8_t* samples, size_t count) {
    if (!s_running) return 0;
    size_t done = 0;
    const int gain = s_gain;
    while (done < count) {
        size_t n = count - done;
        if (n > (size_t)kDmaBufFrames) n = kDmaBufFrames;
        for (size_t i = 0; i < n; i++) {
            // Scale around the midpoint into the top byte of the 16-bit
            // slot (the DAC uses the top 8 bits), keeping the fractional
            // bits so low volumes are not needlessly coarse.
            int v = 128 * 256 + ((int)samples[done + i] - 128) * gain;
            s_stage[i * 2] = (uint16_t)v;
            s_stage[i * 2 + 1] = (uint16_t)v;
        }
        size_t wrote = 0;
        i2s_write(kPort, s_stage, n * 4, &wrote, portMAX_DELAY);
        size_t frames = wrote / 4;
        s_written += frames;
        done += frames;
        if (frames < n) break;
    }
    return done;
}

uint64_t samplesPlayed() {
    drainEvents();
    uint64_t cap = queueCapacity();
    return s_descDone > cap ? s_descDone - cap : 0;
}

uint32_t underruns() {
    drainEvents();
    return s_underruns;
}

uint32_t clockMs() {
    if (!s_rate) return 0;
    return (uint32_t)(samplesPlayed() * 1000ULL / s_rate);
}

uint32_t queuedSamples() {
    uint64_t played = samplesPlayed();
    return s_written > played ? (uint32_t)(s_written - played) : 0;
}

uint32_t queueCapacity() { return kDmaBufCount * kDmaBufFrames; }

uint32_t calibrate(uint32_t samples) {
    if (!s_running) return 0;
    flush();
    uint64_t played0 = samplesPlayed();
    // Fill the queue first so the timed region measures pure drain rate.
    static uint8_t silence[kDmaBufFrames];
    memset(silence, 128, sizeof(silence));
    uint32_t cap = queueCapacity();
    for (uint32_t i = 0; i < cap; i += kDmaBufFrames) write(silence, kDmaBufFrames);
    uint32_t t0 = millis();
    for (uint32_t i = 0; i < samples; i += kDmaBufFrames) write(silence, kDmaBufFrames);
    uint32_t dt = millis() - t0;
    uint64_t played = samplesPlayed() - played0;
    uint32_t hz = dt ? (uint32_t)((uint64_t)samples * 1000 / dt) : 0;
    Serial.printf("[audio] calibrate: %u samples drained in %u ms = %u Hz (configured %u); clock saw %llu samples\n",
                  (unsigned)samples, (unsigned)dt, (unsigned)hz, (unsigned)s_rate, played);
    return hz;
}

void selfTest() {
    const uint32_t rates[] = {8000, 16000, 22050, 32000, 44100};
    for (int apll = 0; apll < 2; apll++) {
        for (uint32_t r : rates) {
            if (!begin(r, apll != 0)) continue;
            uint32_t hz = calibrate(r);   // one second of samples
            Serial.printf("[audio] selftest rate=%u apll=%d -> measured %u Hz (x%.2f)\n",
                          (unsigned)r, apll, (unsigned)hz, r ? hz / (float)r : 0.0f);
            end();
        }
    }
}

void flush() {
    if (!s_running) return;
    i2s_zero_dma_buffer(kPort);
    drainEvents();
}

}  // namespace audio
