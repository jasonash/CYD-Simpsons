// Bench experiment (2026-09-05): the DAC plays silence while the SD, the
// display SPI and the CPU are stressed one at a time, so the ear can tell
// which one couples into the audio. LED colour shows the active stage.
#pragma once

class TFT_eSPI;

namespace probe {

// Blocks for about 4 x stageMs. Stages: LED off = idle silence, red = SD
// reads, green = display pushes, blue = CPU spin. Restores the LED to green.
void run(TFT_eSPI* tft, const char* sdFile, unsigned stageMs = 12000);

}  // namespace probe
