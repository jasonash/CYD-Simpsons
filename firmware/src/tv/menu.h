// On-screen settings menu. Replaces playback while open (nothing runs per
// frame), driven by touch: tap the arrows to change a value, tap a toggle
// row to flip it, tap EXIT or wait for the timeout to leave. Saves on exit.
#pragma once

class TFT_eSPI;

namespace menu {

// Blocks until the menu closes.
void run(TFT_eSPI* tft);

}  // namespace menu
