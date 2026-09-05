#include "input.h"

#include <Arduino.h>
#include <XPT2046_Bitbang.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "../boards/board.h"
#include "../player/audio_out.h"
#include "../player/player.h"
#include "settings.h"

namespace input {

static const uint32_t kPollMs = 20;
static const uint32_t kDebounceMs = 40;     // must see the panel pressed this long
static const uint32_t kHoldMs = 1000;       // press this long for HOLD
// Pressure below this is a brush or panel noise. Real presses on the CYD read
// 2200-2400 in the library's units; a stray event during the static read 192.
static const uint16_t kMinZ = 500;

static XPT2046_Bitbang s_touch(PIN_TOUCH_MOSI, PIN_TOUCH_MISO, PIN_TOUCH_CLK, PIN_TOUCH_CS);
static QueueHandle_t s_events = nullptr;
static volatile bool s_pressed = false;
static bool s_started = false;
static bool s_pollPanel = true;

static void post(Event ev) {
    int v = (int)ev;
    xQueueSend(s_events, &v, 0);
}

static uint16_t s_lastZ = 0;
static uint16_t s_rawX = 0, s_rawY = 0;
static volatile int s_pointX = -1, s_pointY = -1;
static volatile bool s_flipped = false;

static bool panelPressed() {
    // The bit-banged read costs ~1 ms.
    TouchPoint p = s_touch.getTouch();
    s_lastZ = p.zRaw;
    s_rawX = p.xRaw;
    s_rawY = p.yRaw;
    return p.zRaw >= kMinZ;
}

// Raw ADC to screen pixels for TFT_ROTATION (checked in bring-up: dots
// track the finger). Flip mirrors both axes.
static void mapPoint() {
    int x = map(s_rawX, TOUCH_RAW_MIN, TOUCH_RAW_MAX, 0, DISPLAY_W);
    int y = map(s_rawY, TOUCH_RAW_MIN, TOUCH_RAW_MAX, 0, DISPLAY_H);
    x = constrain(x, 0, DISPLAY_W - 1);
    y = constrain(y, 0, DISPLAY_H - 1);
    if (s_flipped) { x = DISPLAY_W - 1 - x; y = DISPLAY_H - 1 - y; }
    s_pointX = x;
    s_pointY = y;
}

static void inputTask(void*) {
    bool down = false;          // debounced state
    uint32_t downSince = 0;
    uint32_t rawSince = 0;      // when the raw reading last changed
    bool rawLast = false;
    bool holdFired = false;

    for (;;) {
        bool raw = s_pollPanel ? panelPressed() : false;
        uint32_t now = millis();
        if (raw != rawLast) {
            rawLast = raw;
            rawSince = now;
        }
        if (raw != down && now - rawSince >= kDebounceMs) {
            down = raw;
            s_pressed = down;
            Serial.printf("[input] %s z=%u at %d,%d\n", down ? "down" : "up", (unsigned)s_lastZ,
                          (int)s_pointX, (int)s_pointY);
            if (down) {
                downSince = now;
                holdFired = false;
                mapPoint();
            } else if (!holdFired) {
                post(TAP);
            }
        }
        if (down && !holdFired && now - downSince >= kHoldMs) {
            holdFired = true;
            post(HOLD);
        }

        // Serial shortcuts for bench work: n = tap, m = hold, 1-9 = volume
        // 10-90%, 0 = 100%, t = toggle panel polling (audio noise hunt).
        while (Serial.available()) {
            int c = Serial.read();
            if (c == 'n') post(TAP);
            else if (c == 'm') post(HOLD);
            else if (c == 'x') post(PROBE);
            else if (c >= '0' && c <= '9') {
                audio::setVolume(c == '0' ? 100 : (c - '0') * 10);
                Serial.printf("[audio] volume %u%%\n", audio::volume());
            } else if (c == 't') {
                s_pollPanel = !s_pollPanel;
                Serial.printf("[input] panel polling %s\n", s_pollPanel ? "on" : "off");
            } else if (c == 'i') {
                settings::values().invert = !settings::values().invert;
                settings::save();
                Serial.printf("[settings] invert %d (takes effect on next episode)\n", (int)settings::values().invert);
            } else if (c == 'b') {
                player::setBusyMs(player::busyMs() ? 0 : 30);
                Serial.printf("[player] busy loop %u ms\n", (unsigned)player::busyMs());
            } else if (c == 'c') {
                audio::setIsrCore(audio::isrCore() == 0 ? -1 : 0);
                Serial.printf("[audio] i2s isr core for next play: %d\n", audio::isrCore());
            }
        }

        vTaskDelay(pdMS_TO_TICKS(kPollMs));
    }
}

void begin() {
    if (s_started) return;
    s_events = xQueueCreate(8, sizeof(int));
    s_touch.begin();
    xTaskCreatePinnedToCore(inputTask, "input", 3072, nullptr, 1, nullptr, 0);
    s_started = true;
}

Event poll() {
    int v;
    if (s_events && xQueueReceive(s_events, &v, 0) == pdTRUE) return (Event)v;
    return NONE;
}

void flush() {
    if (s_events) xQueueReset(s_events);
}

bool pressed() { return s_pressed; }

bool lastPoint(int* x, int* y) {
    if (s_pointX < 0) return false;
    *x = s_pointX;
    *y = s_pointY;
    return true;
}

void setFlipped(bool flipped) { s_flipped = flipped; }

}  // namespace input
