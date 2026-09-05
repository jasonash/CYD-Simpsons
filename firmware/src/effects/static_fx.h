// TV static: the channel-change transition. Generated noise on the panel
// and a white-noise burst on the speaker, no stored assets.
#pragma once

#include <stdint.h>

class TFT_eSPI;

namespace fx {

// Blocks for about `ms`. Takes over the display and the audio driver; the
// audio driver is left stopped, so call before the next player::play().
void tvStatic(TFT_eSPI* tft, uint32_t ms);

}  // namespace fx
