// microSD access for the player: ESP-IDF sdspi host driver on VSPI.
//
// Chosen over the Arduino SD library after the 2026-09-03 benchmarks: the
// Arduino driver clamps SPI to 25 MHz and polls the data token byte by byte
// (1.2 MB/s), while sdspi uses DMA multi-block reads and, once forced to
// 40 MHz, reaches 2.3 MB/s on the same card. DMA also leaves the CPU free
// for JPEG decode, which is the actual bottleneck.
//
// Must be the first thing to touch the card after power-on. The Arduino SD
// library leaves the card with CRC checking disabled and this driver will
// then fail to initialise it (ESP_ERR_INVALID_CRC).
#pragma once

#include <stdint.h>

namespace sdcard {

// Mount the card at /sdcard. Returns true on success. Safe to call once.
bool mount();

// Unmount and release the SPI bus.
void unmount();

bool isMounted();

// Card capacity in MB, 0 if not mounted.
uint32_t capacityMB();

// Effective SPI clock in kHz after mount (40000 if the high-speed force
// succeeded, else whatever the driver negotiated).
uint32_t busKHz();

// Mount point prefix for POSIX paths, e.g. "/sdcard".
const char* mountPoint();

}  // namespace sdcard
