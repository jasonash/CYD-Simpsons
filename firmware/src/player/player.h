// Player core: read interleaved AVI chunks from SD, decode MJPEG frames
// straight to the display in MCU strips, feed PCM to the DAC, and keep
// video paced against the audio clock. SD reads and audio writes run in a
// reader task on core 0; decode runs in the caller of play() (core 1).
#pragma once

#include <stdint.h>

class TFT_eSPI;

namespace player {

struct Stats {
    uint32_t framesShown = 0;
    uint32_t framesDropped = 0;
    uint32_t framesBad = 0;         // decode errors / oversize
    uint32_t audioChunks = 0;
    uint32_t audioUnderruns = 0;
    uint32_t elapsedMs = 0;         // wall time since play() started
    // Per-window (reset every report):
    uint32_t winFrames = 0;
    uint32_t winDecodeUs = 0;       // JPEG decode + blit
    uint32_t winDecodeMaxUs = 0;
    uint32_t winReadUs = 0;         // SD reads (video + audio)
    uint32_t winAudioWaitUs = 0;    // time blocked inside i2s_write
    uint32_t winSyncWaitUs = 0;     // time deliberately waiting for the clock
    uint32_t winStartMs = 0;
    int32_t avDriftMs = 0;          // video time minus audio clock at last frame
    uint32_t maxFrameBytes = 0;
};

// Attach the display. Must be initialised and rotated by the caller.
void begin(TFT_eSPI* tft);

// Polled once per frame from the decode loop; return true to stop early.
typedef bool (*StopFn)();

// Play a file to the end, or until `stop` returns true. Blocks. Returns
// false if the file could not be opened. Prints a stats line to Serial
// every `reportEveryFrames` frames (0 = only at the end).
bool play(const char* path, uint32_t reportEveryFrames = 100, StopFn stop = nullptr);

// True if the last play() ended because `stop` asked for it.
bool wasStopped();

const Stats& stats();

}  // namespace player
