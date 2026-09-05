#include "input.h"

#include <Arduino.h>
#include <XPT2046_Bitbang.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "../boards/board.h"

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

static void post(Event ev) {
    int v = (int)ev;
    xQueueSend(s_events, &v, 0);
}

static uint16_t s_lastZ = 0;

static bool panelPressed() {
    // The bit-banged read costs ~1 ms; the pressure reading alone is enough
    // for tap/hold, coordinates are not needed yet.
    TouchPoint p = s_touch.getTouch();
    s_lastZ = p.zRaw;
    return p.zRaw >= kMinZ;
}

static void inputTask(void*) {
    bool down = false;          // debounced state
    uint32_t downSince = 0;
    uint32_t rawSince = 0;      // when the raw reading last changed
    bool rawLast = false;
    bool holdFired = false;

    for (;;) {
        bool raw = panelPressed();
        uint32_t now = millis();
        if (raw != rawLast) {
            rawLast = raw;
            rawSince = now;
        }
        if (raw != down && now - rawSince >= kDebounceMs) {
            down = raw;
            s_pressed = down;
            Serial.printf("[input] %s z=%u\n", down ? "down" : "up", (unsigned)s_lastZ);
            if (down) {
                downSince = now;
                holdFired = false;
            } else if (!holdFired) {
                post(TAP);
            }
        }
        if (down && !holdFired && now - downSince >= kHoldMs) {
            holdFired = true;
            post(HOLD);
        }

        // Serial shortcuts for bench work: n = tap, m = hold.
        while (Serial.available()) {
            int c = Serial.read();
            if (c == 'n') post(TAP);
            else if (c == 'm') post(HOLD);
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

}  // namespace input
