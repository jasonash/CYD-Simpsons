#include "menu.h"

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <math.h>

#include "../boards/board.h"
#include "../player/audio_out.h"
#include "input.h"
#include "settings.h"

namespace menu {

static const uint32_t kIdleExitMs = 12000;
static const int kRows = 6;
static const int kRowH = 34;
static const int kTop = 28;
static const int kArrowW = 48;
static const uint16_t kBg = TFT_BLACK;
static const uint16_t kFg = 0x07E0;       // classic OSD green
static const uint16_t kDim = 0x03E0;

enum Row { VOLUME, BRIGHTNESS, COLOURS, SCREEN, AUDIO_OUT, EXIT };

static void drawRow(TFT_eSPI* tft, int row) {
    settings::Values& v = settings::values();
    int y = kTop + row * kRowH;
    tft->fillRect(0, y, DISPLAY_W, kRowH, kBg);
    tft->setTextColor(kFg, kBg);
    tft->setTextDatum(ML_DATUM);
    char val[24] = "";
    bool arrows = false;
    const char* label = "";
    switch (row) {
        case VOLUME:     label = "VOLUME";     snprintf(val, sizeof(val), "%u", v.volume); arrows = true; break;
        case BRIGHTNESS: label = "BRIGHTNESS"; snprintf(val, sizeof(val), "%u", v.brightness); arrows = true; break;
        case COLOURS:    label = "COLOURS";    strlcpy(val, v.invert ? "INVERTED" : "NORMAL", sizeof(val)); break;
        case SCREEN:     label = "SCREEN";     strlcpy(val, v.flip ? "FLIPPED" : "NORMAL", sizeof(val)); break;
        case AUDIO_OUT:     label = "AUDIO OUT";  strlcpy(val, v.i2sAmp ? "I2S AMP" : "ONBOARD", sizeof(val)); break;
        case EXIT:       label = "EXIT";       break;
    }
    tft->drawString(label, 16, y + kRowH / 2, 4);
    if (arrows) {
        // Bar between the arrows, value on top.
        int x0 = 150 + kArrowW, x1 = DISPLAY_W - kArrowW - 8;
        int pct = row == VOLUME ? v.volume : v.brightness;
        tft->drawRect(x0, y + 9, x1 - x0, 16, kDim);
        tft->fillRect(x0 + 2, y + 11, ((x1 - x0 - 4) * pct) / 100, 12, kFg);
        tft->setTextDatum(MC_DATUM);
        tft->drawString("<", 150 + kArrowW / 2, y + kRowH / 2, 4);
        tft->drawString(">", DISPLAY_W - kArrowW / 2 - 8, y + kRowH / 2, 4);
    } else if (val[0]) {
        tft->setTextDatum(MR_DATUM);
        tft->drawString(val, DISPLAY_W - 16, y + kRowH / 2, 4);
    }
}

static void drawAll(TFT_eSPI* tft) {
    tft->fillScreen(kBg);
    tft->setTextColor(kFg, kBg);
    tft->setTextDatum(MC_DATUM);
    tft->drawString("SETTINGS", DISPLAY_W / 2, 14, 2);
    for (int r = 0; r < kRows; r++) drawRow(tft, r);
}

// Short beep at the current volume so the level can be judged with no
// episode playing.
static void beep() {
    static uint8_t buf[1600];   // 100 ms at 16 kHz
    for (int i = 0; i < (int)sizeof(buf); i++) {
        buf[i] = (uint8_t)(128 + 100 * sinf(2 * 3.14159f * 440 * i / 16000.0f));
    }
    if (!audio::begin(16000)) return;
    audio::write(buf, sizeof(buf));
    delay(240);   // ring depth + tone
    audio::end();
}

static int step(int v, int delta, int lo, int hi) {
    v += delta;
    if (v < lo) v = lo;
    if (v > hi) v = hi;
    return v;
}

void run(TFT_eSPI* tft) {
    settings::Values& v = settings::values();
    input::flush();
    drawAll(tft);
    uint32_t last = millis();
    bool open = true;
    while (open) {
        input::Event ev = input::poll();
        if (ev == input::NONE) {
            if (millis() - last > kIdleExitMs) break;
            delay(10);
            continue;
        }
        last = millis();
        if (ev != input::TAP && ev != input::HOLD) continue;
        int x, y;
        if (!input::lastPoint(&x, &y)) continue;
        int row = (y - kTop) / kRowH;
        if (y < kTop || row >= kRows) continue;
        int delta = 0;
        if (x >= 150 && x < 150 + kArrowW) delta = -10;
        else if (x >= DISPLAY_W - kArrowW - 8) delta = 10;
        switch (row) {
            case VOLUME:
                if (delta) { v.volume = step(v.volume, delta, 0, 100); settings::applyVolume(); drawRow(tft, row); beep(); }
                break;
            case BRIGHTNESS:
                if (delta) { v.brightness = step(v.brightness, delta, 10, 100); settings::applyBrightness(); drawRow(tft, row); }
                break;
            case COLOURS:
                v.invert = !v.invert;
                tft->invertDisplay(v.invert);
                drawRow(tft, row);
                break;
            case SCREEN:
                v.flip = !v.flip;
                tft->setRotation(settings::rotation());
                input::setFlipped(v.flip);
                drawAll(tft);
                break;
            case AUDIO_OUT:
                v.i2sAmp = !v.i2sAmp;
                settings::applyVolume();   // also sets the backend
                drawRow(tft, row);
                beep();
                break;
            case EXIT:
                open = false;
                break;
        }
    }
    settings::save();
    Serial.printf("[menu] saved: volume %u brightness %u invert %d flip %d\n",
                  v.volume, v.brightness, (int)v.invert, (int)v.flip);
    tft->fillScreen(TFT_BLACK);
    input::flush();
}

}  // namespace menu
