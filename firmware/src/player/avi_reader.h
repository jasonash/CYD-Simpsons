// Minimal AVI (RIFF) reader for linear playback of MJPEG + PCM files.
//
// Parses the 'hdrl' list for stream layout, then walks the 'movi' list one
// chunk at a time. Handles ffmpeg's output: '00dc' video, '01wb' audio,
// 'JUNK' padding, OpenDML 'ix##' index chunks inside movi, 'rec ' sub-lists,
// and 'AVIX' continuation RIFFs for files over 1 GB. No seeking beyond a
// sequential read; that comes with the seek table in a later phase.
//
// Reads go through a 16 KB buffer over a POSIX fd so the SD driver sees
// large multi-sector requests.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace avi {

enum ChunkType : uint8_t {
    CHUNK_NONE = 0,   // end of file
    CHUNK_VIDEO,
    CHUNK_AUDIO,
    CHUNK_OTHER,      // skipped internally, never returned
};

struct Info {
    uint32_t usPerFrame = 0;     // from avih
    uint32_t totalFrames = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t fpsNum = 0;         // strh rate/scale of the video stream
    uint32_t fpsDen = 1;
    char videoFourcc[5] = {0};   // e.g. "MJPG"
    bool hasAudio = false;
    uint16_t audioFormat = 0;    // 1 = PCM
    uint16_t audioChannels = 0;
    uint32_t audioRate = 0;
    uint16_t audioBits = 0;
    uint32_t moviOffset = 0;
    uint32_t moviSize = 0;
    uint32_t fileSize = 0;
    int videoStream = -1;
    int audioStream = -1;
};

class Reader {
public:
    Reader();
    ~Reader();

    // Open and parse headers. Returns false with lastError() set on failure.
    bool open(const char* path);
    void close();
    bool isOpen() const { return fd_ >= 0; }

    const Info& info() const { return info_; }
    const char* lastError() const { return err_; }

    // Advance to the next video or audio chunk. Returns the chunk type and
    // its payload size. Call readChunk() or skipChunk() before calling again.
    ChunkType nextChunk(uint32_t* size);

    // Read up to cap bytes of the current chunk payload into dst. Returns
    // the amount copied (0 once the chunk is exhausted), -1 on read error.
    // Call repeatedly to consume a chunk larger than cap.
    int readChunk(uint8_t* dst, uint32_t cap);

    // Skip the current chunk payload without reading it.
    bool skipChunk();

    // Rewind to the first chunk of the movi list.
    bool rewind();

    // Byte position of the next unread chunk header, for progress display.
    uint32_t position() const { return pos_ + (uint32_t)bufPos_; }

private:
    bool fill();
    bool readBytes(void* dst, uint32_t n);
    bool skipBytes(uint32_t n);
    bool seekTo(uint32_t off);
    bool parseHeaders();
    bool parseStreamList(uint32_t end);
    void setError(const char* msg);

    int fd_ = -1;
    Info info_;
    const char* err_ = "";

    // Buffered reader state.
    uint8_t* buf_ = nullptr;
    size_t bufCap_ = 0;
    size_t bufLen_ = 0;
    size_t bufPos_ = 0;
    uint32_t pos_ = 0;   // absolute file offset of buf_[0]

    // Current chunk state.
    uint32_t chunkRemain_ = 0;   // payload bytes not yet consumed
    uint32_t chunkPad_ = 0;      // 1 if payload size is odd

    // Region of the current movi list we are walking.
    uint32_t moviEnd_ = 0;

    int nextStream_ = 0;   // strl lists seen so far while parsing hdrl
};

}  // namespace avi
