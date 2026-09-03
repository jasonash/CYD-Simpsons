// CYD-Simpsons firmware entry point.
//
// Phase 0 player spike: mount the card, play one AVI in a loop, print
// timing stats over serial. Everything else (channels, effects, web UI)
// comes later and hangs off the player core.
//
// The bring-up sketch lives in bringup.cpp and is built by the *_bringup envs.

#ifndef CYD_BRINGUP

#include <Arduino.h>
#include <TFT_eSPI.h>

#include "boards/board.h"
#include "player/player.h"
#include "player/sd_card.h"

static TFT_eSPI tft;

// Spike playlist: first one that opens wins. A real episode if present,
// else the synthetic clip.
static const char* kSpikeFiles[] = {"/sdcard/1.avi", "/sdcard/test_60s.avi"};
static const char* kSpikeFile = nullptr;

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

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("\nCYD-Simpsons player spike");

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

    player::begin(&tft);
    for (const char* f : kSpikeFiles) {
        FILE* fp = fopen(f, "rb");
        if (fp) { fclose(fp); kSpikeFile = f; break; }
    }
    if (!kSpikeFile) {
        showMessage("No AVI found", "Put 1.avi or test_60s.avi on the card");
        setLed(true, false, false);
        return;
    }
    Serial.printf("Playing %s\n", kSpikeFile);
    setLed(false, true, false);
}

void loop() {
    if (!sdcard::isMounted() || !kSpikeFile) {
        delay(1000);
        return;
    }
    if (!player::play(kSpikeFile, 100)) {
        showMessage("Cannot play", kSpikeFile);
        setLed(true, false, false);
        delay(5000);
        return;
    }
    // Loop forever: the exit criterion is a full episode, so run it back.
    delay(500);
}

#endif  // CYD_BRINGUP
