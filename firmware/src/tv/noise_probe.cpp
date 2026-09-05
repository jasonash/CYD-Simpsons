#include "noise_probe.h"

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <stdio.h>

#include "../boards/board.h"
#include "../player/audio_out.h"

namespace probe {

static void led(bool r, bool g, bool b) {
    digitalWrite(PIN_LED_R, r ? LED_ON : LED_OFF);
    digitalWrite(PIN_LED_G, g ? LED_ON : LED_OFF);
    digitalWrite(PIN_LED_B, b ? LED_ON : LED_OFF);
}

void run(TFT_eSPI* tft, const char* sdFile, unsigned stageMs) {
    static uint8_t silence[256];
    memset(silence, 128, sizeof(silence));
    const size_t kChunk = 16 * 1024;
    uint8_t* buf = (uint8_t*)malloc(kChunk);
    uint16_t* strip = (uint16_t*)malloc(DISPLAY_W * 16 * sizeof(uint16_t));
    if (!buf || !strip) { free(buf); free(strip); return; }
    for (int i = 0; i < DISPLAY_W * 16; i++) strip[i] = (i & 1) ? 0xFFFF : 0x0000;

    tft->fillScreen(TFT_BLACK);
    tft->setTextColor(TFT_YELLOW, TFT_BLACK);
    tft->drawString("Noise probe", 8, 100, 4);

    audio::begin(16000);
    FILE* fp = fopen(sdFile, "rb");
    Serial.printf("[probe] start, sd file %s %s\n", sdFile, fp ? "open" : "MISSING");

    for (int stage = 0; stage < 4; stage++) {
        const char* name[] = {"idle (LED off)", "SD reads (red)", "display pushes (green)", "CPU spin (blue)"};
        led(stage == 1, stage == 2, stage == 3);
        Serial.printf("[probe] stage %d: %s\n", stage, name[stage]);
        uint32_t t0 = millis();
        uint32_t bytes = 0, strips = 0;
        int y = 0;
        while (millis() - t0 < stageMs) {
            // Keep the DAC fed with silence; this call paces at 16 ms once
            // the DMA ring is full.
            audio::write(silence, sizeof(silence));
            uint32_t s0 = millis();
            switch (stage) {
                case 1:
                    if (fp) {
                        size_t n = fread(buf, 1, kChunk, fp);
                        if (n < kChunk) rewind(fp);
                        bytes += n;
                    }
                    break;
                case 2:
                    for (int k = 0; k < 8; k++) {
                        tft->pushImage(0, y, DISPLAY_W, 16, strip);
                        y = (y + 16) % DISPLAY_H;
                        strips++;
                    }
                    break;
                case 3:
                    while (millis() - s0 < 14) { /* spin */ }
                    break;
                default:
                    break;
            }
        }
        Serial.printf("[probe]   done: %u KB read, %u strips\n", (unsigned)(bytes / 1024), (unsigned)strips);
    }
    if (fp) fclose(fp);
    audio::end();
    free(buf);
    free(strip);
    tft->fillScreen(TFT_BLACK);
    led(false, true, false);
    Serial.println("[probe] end");
}

}  // namespace probe
