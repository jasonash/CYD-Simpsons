#include "static_fx.h"

#include <Arduino.h>
#include <TFT_eSPI.h>

#include "../boards/board.h"
#include "../player/audio_out.h"

namespace fx {

static const int kStripRows = 16;
static const uint32_t kNoiseRate = 16000;
static const int kNoiseAmp = 24;   // +/- around the DAC midpoint; the amp is loud

static uint32_t s_rng = 0x2545F491;

static inline uint32_t xorshift() {
    uint32_t x = s_rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s_rng = x;
    return x;
}

// 16 grey levels in native RGB565; pushed with swapBytes(true) like the
// player's decoded pixels.
static uint16_t s_grey[16];

static void buildGreys() {
    for (int i = 0; i < 16; i++) {
        uint8_t v = (uint8_t)(i * 17);
        uint16_t c = ((v >> 3) << 11) | ((v >> 2) << 5) | (v >> 3);
        s_grey[i] = c;
    }
}

static void drawNoiseFrame(TFT_eSPI* tft, uint16_t* strip) {
    for (int y = 0; y < DISPLAY_H; y += kStripRows) {
        uint32_t n = (uint32_t)DISPLAY_W * kStripRows;
        for (uint32_t i = 0; i < n; i += 8) {
            uint32_t r = xorshift();
            for (int k = 0; k < 8; k++) {
                strip[i + k] = s_grey[r & 15];
                r >>= 4;
            }
        }
        tft->pushImage(0, y, DISPLAY_W, kStripRows, strip);
    }
}

void tvStatic(TFT_eSPI* tft, uint32_t ms) {
    buildGreys();
    uint16_t* strip = (uint16_t*)malloc(DISPLAY_W * kStripRows * sizeof(uint16_t));
    static uint8_t noise[1024];
    if (!strip) return;

    s_rng ^= esp_random();
    bool swap = tft->getSwapBytes();
    tft->setSwapBytes(true);

    bool audioOk = audio::begin(kNoiseRate);
    uint32_t t0 = millis();
    while (millis() - t0 < ms) {
        drawNoiseFrame(tft, strip);
        if (audioOk) {
            // 64 ms of noise per frame; write() blocks once the DMA ring is
            // full, which paces the picture to roughly 15 frames a second.
            for (int i = 0; i < (int)sizeof(noise); i += 4) {
                uint32_t r = xorshift();
                for (int k = 0; k < 4; k++) {
                    noise[i + k] = (uint8_t)(128 + (int)(r & 0xFF) * (2 * kNoiseAmp) / 256 - kNoiseAmp);
                    r >>= 8;
                }
            }
            audio::write(noise, sizeof(noise));
        }
    }
    if (audioOk) audio::end();

    tft->setSwapBytes(swap);
    tft->fillScreen(TFT_BLACK);
    free(strip);
}

}  // namespace fx
