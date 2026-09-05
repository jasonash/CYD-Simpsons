#include "settings.h"

#include <Arduino.h>
#include <Preferences.h>
#include <TFT_eSPI.h>

#include "../boards/board.h"
#include "../player/audio_out.h"

namespace settings {

static Values s_v;
static const char* kNs = "cyd";
static const int kBlChannel = 7;      // ledc channel for the backlight
static const uint32_t kBlHz = 20000;  // above hearing, in case it couples into audio
static bool s_blReady = false;

void load() {
    Preferences p;
    if (p.begin(kNs, true)) {
        s_v.volume = p.getUChar("vol", s_v.volume);
        s_v.brightness = p.getUChar("bri", s_v.brightness);
        s_v.invert = p.getBool("inv", s_v.invert);
        s_v.flip = p.getBool("flip", s_v.flip);
        s_v.i2sAmp = p.getBool("i2s", s_v.i2sAmp);
        p.end();
    }
    if (s_v.volume > 100) s_v.volume = 100;
    if (s_v.brightness > 100) s_v.brightness = 100;
    if (s_v.brightness < 5) s_v.brightness = 5;
    Serial.printf("[settings] volume %u brightness %u invert %d flip %d i2s %d\n",
                  s_v.volume, s_v.brightness, (int)s_v.invert, (int)s_v.flip, (int)s_v.i2sAmp);
}

void save() {
    Preferences p;
    if (!p.begin(kNs, false)) return;
    p.putUChar("vol", s_v.volume);
    p.putUChar("bri", s_v.brightness);
    p.putBool("inv", s_v.invert);
    p.putBool("flip", s_v.flip);
    p.putBool("i2s", s_v.i2sAmp);
    p.end();
}

Values& values() { return s_v; }

uint8_t rotation() { return s_v.flip ? (TFT_ROTATION + 2) % 4 : TFT_ROTATION; }

void applyBrightness() {
    if (!s_blReady) {
        ledcSetup(kBlChannel, kBlHz, 8);
        ledcAttachPin(PIN_TFT_BL, kBlChannel);
        s_blReady = true;
    }
    // Perceptual-ish curve: square the fraction so the low end is usable.
    uint32_t f = s_v.brightness;
    uint32_t duty = (f * f * 255) / 10000;
    if (duty < 3) duty = 3;
    ledcWrite(kBlChannel, duty);
}

void applyVolume() {
    audio::setVolume(s_v.volume);
    audio::setBackend(s_v.i2sAmp ? audio::BACKEND_I2S : audio::BACKEND_DAC);
}

void apply(TFT_eSPI* tft) {
    tft->invertDisplay(s_v.invert);
    tft->setRotation(rotation());
    applyBrightness();
    applyVolume();
}

}  // namespace settings
