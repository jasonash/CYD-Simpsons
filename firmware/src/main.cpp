// CYD-Simpsons firmware entry point.
//
// Phase 1: a single channel. Scan the card for episodes, play them from a
// shuffle bag back to back, and change episode on a tap with a burst of
// static. The bring-up sketch lives in bringup.cpp and is built by the
// *_bringup envs.

#ifndef CYD_BRINGUP

#include <Arduino.h>
#include <TFT_eSPI.h>

#include "boards/board.h"
#include "effects/static_fx.h"
#include "player/player.h"
#include "player/sd_card.h"
#include "tv/input.h"
#include "tv/library.h"
#include "tv/noise_probe.h"

static TFT_eSPI tft;

// Episodes live in /episodes; a card with AVIs in the root still works.
static const char* kEpisodeDirs[] = {"/sdcard/episodes", "/sdcard"};
static const uint32_t kStaticMs = 400;

static bool s_ready = false;
static input::Event s_pending = input::NONE;   // event that stopped playback

static void setLed(bool r, bool g, bool b) {
    digitalWrite(PIN_LED_R, r ? LED_ON : LED_OFF);
    digitalWrite(PIN_LED_G, g ? LED_ON : LED_OFF);
    digitalWrite(PIN_LED_B, b ? LED_ON : LED_OFF);
}

static void showMessage(const char* line1, const char* line2) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString(line1, 8, 100, 4);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(line2, 8, 130, 2);
}

// Called by the player once per frame. A tap ends the episode; a hold will
// open the settings menu once it exists and is treated as a tap until then.
static bool stopOnInput() {
    input::Event ev = input::poll();
    if (ev == input::NONE) return false;
    s_pending = ev;
    return true;
}

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("\nCYD-Simpsons");

    pinMode(PIN_LED_R, OUTPUT);
    pinMode(PIN_LED_G, OUTPUT);
    pinMode(PIN_LED_B, OUTPUT);
    setLed(true, false, false);

    tft.init();
    tft.setRotation(TFT_ROTATION);
    tft.fillScreen(TFT_BLACK);

    Serial.printf("Heap: %u free, %u largest block\n",
                  (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());

    // The card must be initialised by the IDF driver before anything else
    // touches the SPI bus (see sd_card.h).
    if (!sdcard::mount()) {
        showMessage("No SD card", "Insert a FAT32 card and reset");
        setLed(false, false, true);
        return;
    }
    Serial.printf("SD: %u MB at %u kHz\n", (unsigned)sdcard::capacityMB(),
                  (unsigned)sdcard::busKHz());

    for (const char* dir : kEpisodeDirs) {
        if (library::scan(dir) > 0) break;
    }
    if (library::count() == 0) {
        showMessage("No episodes", "Put .avi files in /episodes on the card");
        setLed(true, false, false);
        return;
    }

    player::begin(&tft);
    input::begin();
    setLed(false, true, false);
    s_ready = true;
}

void loop() {
    if (!s_ready) {
        delay(1000);
        return;
    }
    const char* path = library::next();
    Serial.printf("Playing %s\n", library::currentName());
    if (!player::play(path, 100, stopOnInput)) {
        showMessage("Cannot play", library::currentName());
        setLed(true, false, false);
        delay(3000);
        setLed(false, true, false);
        return;
    }
    if (player::wasStopped()) {
        if (s_pending == input::PROBE) {
            s_pending = input::NONE;
            probe::run(&tft, library::current());
            input::flush();
            return;
        }
        Serial.printf("Input: %s, changing episode\n", s_pending == input::HOLD ? "hold" : "tap");
        s_pending = input::NONE;
        fx::tvStatic(&tft, kStaticMs);
        input::flush();
    }
}

#endif  // CYD_BRINGUP
