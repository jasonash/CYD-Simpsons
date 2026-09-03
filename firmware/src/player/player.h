// Player core (Phase 0 spike): read interleaved AVI chunks from SD, decode
// MJPEG frames straight to the display in MCU strips, feed PCM to the DAC,
// and keep video paced against the audio clock. Everything runs in the
// calling task for now so the measurements show where the time goes before
// any core-splitting is designed.
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

// Play a file to the end. Blocks. Returns false if it could not be opened.
// Prints a stats line to Serial every `reportEveryFrames` frames.
bool play(const char* path, uint32_t reportEveryFrames = 100);

const Stats& stats();

}  // namespace player
