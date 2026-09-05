// Audio output: mono PCM (8-bit unsigned or 16-bit signed) through the I2S
// peripheral with DMA, to either the internal DAC on GPIO26 (built-in DAC
// mode) or an external I2S amplifier (MAX98357A on the header pins). The
// I2S DMA queue is what paces playback, and the count of completed DMA
// buffers is the master clock the video syncs to.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace audio {

// Start the I2S/DAC driver at the given sample rate. Returns false on error.
// useApll selects the audio PLL as clock source. Keep it true: with the
// default PLL_D2 source the legacy driver cannot hit 8 or 16 kHz in DAC
// mode and runs ~5.4x fast (measured 2026-09-03); 22050 Hz and up are fine
// either way. The APLL is within 1% at every rate tested.
bool begin(uint32_t sampleRate, uint8_t bits = 8, bool useApll = true);

enum Backend { BACKEND_DAC = 0, BACKEND_I2S = 1 };

// Choose the output for the next begin(): internal DAC + onboard amp, or
// the external I2S amp. Persisted by settings.
void setBackend(Backend b);
Backend backend();

// The I2S interrupt is serviced on the core that installs the driver. Pin
// it to a core (0 or 1) for the next begin(); -1 = whichever core calls
// begin(). Experiment 2026-09-05: clicks when core 1 is saturated.
void setIsrCore(int core);
int isrCore();

// Diagnostic sweep: for several rates and both clock sources, start the
// driver, calibrate, print the measured rate, stop. ~1 s per combination.
void selfTest();
void end();
bool isRunning();

// Master volume, 0-100 percent of full scale. Applied in write() as a
// linear scale of the sample around the DAC midpoint (the onboard amp has
// no gain control). Persists across begin()/end(); default 25.
void setVolume(uint8_t percent);
uint8_t volume();

// Push mono samples in the format given to begin() (u8 or s16, `count` is
// samples, not bytes). Blocks when the DMA queue is full, which is the
// intended behaviour: it throttles the file reader to real time.
// Returns the number of samples accepted. Safe to call from a different
// task/core than the clock readers below.
size_t write(const uint8_t* samples, size_t count);
size_t write16(const int16_t* samples, size_t count);

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
