// Audio output: 8-bit unsigned mono PCM to the internal DAC on GPIO26 via
// I2S built-in DAC mode with DMA. The I2S DMA queue is what paces playback,
// and the count of completed DMA buffers is the master clock the video
// syncs to.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace audio {

// Start the I2S/DAC driver at the given sample rate. Returns false on error.
// useApll selects the audio PLL as clock source. Keep it true: with the
// default PLL_D2 source the legacy driver cannot hit 8 or 16 kHz in DAC
// mode and runs ~5.4x fast (measured 2026-09-03); 22050 Hz and up are fine
// either way. The APLL is within 1% at every rate tested.
bool begin(uint32_t sampleRate, bool useApll = true);

// Diagnostic sweep: for several rates and both clock sources, start the
// driver, calibrate, print the measured rate, stop. ~1 s per combination.
void selfTest();
void end();
bool isRunning();

// Push u8 mono samples. Blocks when the DMA queue is full, which is the
// intended behaviour: it throttles the file reader to real time.
// Returns the number of samples accepted. Safe to call from a different
// task/core than the clock readers below.
size_t write(const uint8_t* samples, size_t count);

// Playback clock: position of the DAC in the stream of samples handed to
// write(), in samples. Derived from DMA descriptor completions (the DMA runs
// continuously, so completions are wall time at the APLL rate) minus the
// ring of silence begin() primes. Monotonic; resets on begin().
uint64_t samplesPlayed();

// DMA descriptors that completed before new data was written (audible gaps).
uint32_t underruns();

// Same clock in milliseconds.
uint32_t clockMs();

// Samples queued but not yet played (DMA occupancy estimate).
uint32_t queuedSamples();

// Queue capacity in samples.
uint32_t queueCapacity();

// Silence the DAC and drop queued audio (used on stop / rewind).
void flush();

// Diagnostic: push `samples` of silence with blocking writes and time how
// long they take to drain. Returns the measured output rate in Hz, which
// should equal the configured rate. Also reports TX_DONE event count so
// the clock bookkeeping can be checked. Takes ~samples/rate seconds.
uint32_t calibrate(uint32_t samples);

}  // namespace audio
