// User settings persisted in NVS: volume, backlight brightness, panel
// colour inversion (the two ILI9341 variants), and screen flip (the board
// is mounted upside down in the case). One build serves every board.
#pragma once

#include <stdint.h>

class TFT_eSPI;

namespace settings {

struct Values {
    uint8_t volume = 100;      // percent, software scale before the DAC
    uint8_t brightness = 100;  // percent, backlight PWM
    bool invert = false;       // panel needs the inversion bit
    bool flip = false;         // rotate 180 degrees (case mount)
};

// Load from NVS (defaults if nothing stored).
void load();
void save();
Values& values();

// Push every value to the hardware: audio gain, backlight, panel.
void apply(TFT_eSPI* tft);
void applyBrightness();
void applyVolume();

// TFT_eSPI rotation for the current flip setting.
uint8_t rotation();

}  // namespace settings
