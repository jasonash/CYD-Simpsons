// Board profile selector.
//
// Every pin and hardware quirk the application cares about is defined in one
// board header. platformio.ini picks the board by defining CYD_BOARD_<name>.
// Nothing outside src/boards/ should hard-code a GPIO number.
#pragma once

#if defined(CYD_BOARD_2432S028R)
#include "cyd_2432s028r.h"
#else
#error "No board profile selected. Define CYD_BOARD_<name> in platformio.ini."
#endif
