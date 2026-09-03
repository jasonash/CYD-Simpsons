#include "avi_reader.h"

#include <Arduino.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

namespace avi {

static const size_t kBufSize = 16 * 1024;

static inline uint32_t rd32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static inline uint16_t rd16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
static inline bool fcc(const uint8_t* p, const char* s) {
    return p[0] == s[0] && p[1] == s[1] && p[2] == s[2] && p[3] == s[3];
}

Reader::Reader() {}
Reader::~Reader() { close(); }

void Reader::setError(const char* msg) {
    err_ = msg;
    log_e("avi: %s", msg);
}

bool Reader::open(const char* path) {
    close();
    fd_ = ::open(path, O_RDONLY);
    if (fd_ < 0) {
        setError("open failed");
        return false;
    }
    struct stat st;
    if (fstat(fd_, &st) == 0) info_.fileSize = (uint32_t)st.st_size;
    info_ = Info();
    info_.fileSize = (uint32_t)st.st_size;
    nextStream_ = 0;

    buf_ = (uint8_t*)malloc(kBufSize);
    if (!buf_) {
        setError("no memory for read buffer");
        close();
        return false;
    }
    bufCap_ = kBufSize;
    bufLen_ = bufPos_ = 0;
    pos_ = 0;

    if (!parseHeaders()) {
        close();
        return false;
    }
    return rewind();
}

void Reader::close() {
    if (fd_ >= 0) ::close(fd_);
    fd_ = -1;
    free(buf_);
    buf_ = nullptr;
    bufCap_ = bufLen_ = bufPos_ = 0;
    pos_ = 0;
    chunkRemain_ = chunkPad_ = 0;
    moviEnd_ = 0;
}

// ---------------------------------------------------------------------------
// Buffered byte access
// ---------------------------------------------------------------------------

bool Reader::fill() {
    pos_ += (uint32_t)bufLen_;
    bufPos_ = 0;
    int n = ::read(fd_, buf_, bufCap_);
    if (n <= 0) {
        bufLen_ = 0;
        return false;
    }
    bufLen_ = (size_t)n;
    return true;
}

bool Reader::readBytes(void* dst, uint32_t n) {
    uint8_t* out = (uint8_t*)dst;
    while (n > 0) {
        if (bufPos_ >= bufLen_) {
            // Large remaining reads go straight to the fd, bypassing the buffer.
            if (n >= bufCap_) {
                pos_ += (uint32_t)bufLen_;
                bufLen_ = bufPos_ = 0;
                int got = ::read(fd_, out, n);
                if (got <= 0) return false;
                pos_ += (uint32_t)got;
                out += got;
                n -= (uint32_t)got;
                continue;
            }
            if (!fill()) return false;
        }
        size_t avail = bufLen_ - bufPos_;
        size_t take = avail < n ? avail : n;
        memcpy(out, buf_ + bufPos_, take);
        bufPos_ += take;
        out += take;
        n -= (uint32_t)take;
    }
    return true;
}

bool Reader::skipBytes(uint32_t n) {
    size_t avail = bufLen_ - bufPos_;
    if (n <= avail) {
        bufPos_ += n;
        return true;
    }
    return seekTo(pos_ + (uint32_t)bufPos_ + n);
}

bool Reader::seekTo(uint32_t off) {
    if (off >= pos_ && off < pos_ + bufLen_) {
        bufPos_ = off - pos_;
        return true;
    }
    if (lseek(fd_, off, SEEK_SET) < 0) return false;
    pos_ = off;
    bufLen_ = bufPos_ = 0;
    return true;
}

// ---------------------------------------------------------------------------
// Header parsing
// ---------------------------------------------------------------------------

bool Reader::parseStreamList(uint32_t end) {
    // Inside LIST 'strl': strh then strf (then optional strd/strn/indx).
    int streamIndex = nextStream_++;

    uint8_t h[8];
    bool isVideo = false, isAudio = false;
    while (pos_ + bufPos_ + 8 <= end) {
        if (!readBytes(h, 8)) return false;
        uint32_t size = rd32(h + 4);
        uint32_t padded = size + (size & 1);
        if (fcc(h, "strh")) {
            uint8_t sh[56];
            uint32_t take = size < sizeof(sh) ? size : sizeof(sh);
            if (!readBytes(sh, take)) return false;
            if (!skipBytes(padded - take)) return false;
            if (fcc(sh, "vids")) {
                isVideo = true;
                memcpy(info_.videoFourcc, sh + 4, 4);
                info_.videoFourcc[4] = 0;
                uint32_t scale = rd32(sh + 20);
                uint32_t rate = rd32(sh + 24);
                if (scale && rate) {
                    info_.fpsNum = rate;
                    info_.fpsDen = scale;
                }
                info_.videoStream = streamIndex;
            } else if (fcc(sh, "auds")) {
                isAudio = true;
                info_.audioStream = streamIndex;
            }
        } else if (fcc(h, "strf")) {
            uint8_t sf[40];
            uint32_t take = size < sizeof(sf) ? size : sizeof(sf);
            if (!readBytes(sf, take)) return false;
            if (!skipBytes(padded - take)) return false;
            if (isVideo && take >= 12) {
                // BITMAPINFOHEADER: biSize, biWidth, biHeight, ...
                if (!info_.width) info_.width = rd32(sf + 4);
                if (!info_.height) {
                    int32_t hgt = (int32_t)rd32(sf + 8);
                    info_.height = (uint32_t)(hgt < 0 ? -hgt : hgt);
                }
            } else if (isAudio && take >= 16) {
                // WAVEFORMATEX
                info_.hasAudio = true;
                info_.audioFormat = rd16(sf + 0);
                info_.audioChannels = rd16(sf + 2);
                info_.audioRate = rd32(sf + 4);
                info_.audioBits = rd16(sf + 14);
            }
        } else {
            if (!skipBytes(padded)) return false;
        }
    }
    return true;
}

bool Reader::parseHeaders() {
    uint8_t h[12];
    if (!readBytes(h, 12)) { setError("short file"); return false; }
    if (!fcc(h, "RIFF") || !fcc(h + 8, "AVI ")) { setError("not a RIFF AVI"); return false; }

    // Walk top-level chunks until we find LIST 'movi'.
    bool haveMovi = false;
    while (!haveMovi) {
        if (!readBytes(h, 8)) { setError("no movi list"); return false; }
        uint32_t size = rd32(h + 4);
        uint32_t padded = size + (size & 1);
        uint32_t chunkStart = pos_ + (uint32_t)bufPos_;  // start of payload

        if (fcc(h, "LIST")) {
            uint8_t lt[4];
            if (!readBytes(lt, 4)) { setError("truncated LIST"); return false; }
            uint32_t listEnd = chunkStart + padded;
            if (fcc(lt, "hdrl")) {
                // Iterate children of hdrl.
                while (pos_ + bufPos_ + 8 <= listEnd) {
                    uint8_t c[8];
                    if (!readBytes(c, 8)) { setError("truncated hdrl"); return false; }
                    uint32_t cs = rd32(c + 4);
                    uint32_t cpad = cs + (cs & 1);
                    uint32_t cstart = pos_ + (uint32_t)bufPos_;
                    if (fcc(c, "avih")) {
                        uint8_t a[56];
                        uint32_t take = cs < sizeof(a) ? cs : sizeof(a);
                        if (!readBytes(a, take)) return false;
                        if (!skipBytes(cpad - take)) return false;
                        info_.usPerFrame = rd32(a + 0);
                        info_.totalFrames = rd32(a + 16);
                        info_.width = rd32(a + 32);
                        info_.height = rd32(a + 36);
                    } else if (fcc(c, "LIST")) {
                        uint8_t clt[4];
                        if (!readBytes(clt, 4)) return false;
                        if (fcc(clt, "strl")) {
                            if (!parseStreamList(cstart + cpad)) return false;
                        }
                        if (!seekTo(cstart + cpad)) return false;
                    } else {
                        if (!skipBytes(cpad)) return false;
                    }
                }
                if (!seekTo(listEnd)) { setError("seek past hdrl"); return false; }
            } else if (fcc(lt, "movi")) {
                info_.moviOffset = chunkStart + 4;  // first chunk after 'movi'
                info_.moviSize = size - 4;
                moviEnd_ = chunkStart + padded;
                haveMovi = true;
            } else {
                if (!seekTo(listEnd)) { setError("seek past LIST"); return false; }
            }
        } else {
            if (!skipBytes(padded)) { setError("skip chunk"); return false; }
        }
    }

    if (info_.videoStream < 0) { setError("no video stream"); return false; }
    if (!info_.usPerFrame && info_.fpsNum) {
        info_.usPerFrame = (uint32_t)((1000000ULL * info_.fpsDen) / info_.fpsNum);
    }
    return true;
}

bool Reader::rewind() {
    chunkRemain_ = chunkPad_ = 0;
    return seekTo(info_.moviOffset);
}

// ---------------------------------------------------------------------------
// Chunk iteration
// ---------------------------------------------------------------------------

ChunkType Reader::nextChunk(uint32_t* size) {
    if (chunkRemain_ || chunkPad_) {
        if (!skipBytes(chunkRemain_ + chunkPad_)) return CHUNK_NONE;
        chunkRemain_ = chunkPad_ = 0;
    }

    uint8_t h[8];
    for (;;) {
        uint32_t here = pos_ + (uint32_t)bufPos_;
        if (here + 8 > moviEnd_) {
            // End of this movi list. ffmpeg writes 'idx1' next, then for big
            // files a 'RIFF AVIX' with another LIST movi. Look for it.
            if (!seekTo(moviEnd_)) return CHUNK_NONE;
            for (;;) {
                if (!readBytes(h, 8)) return CHUNK_NONE;
                uint32_t sz = rd32(h + 4);
                uint32_t padded = sz + (sz & 1);
                uint32_t start = pos_ + (uint32_t)bufPos_;
                if (fcc(h, "RIFF")) {
                    uint8_t t[4];
                    if (!readBytes(t, 4)) return CHUNK_NONE;
                    if (!fcc(t, "AVIX")) return CHUNK_NONE;
                    continue;  // descend into the RIFF, keep scanning
                }
                if (fcc(h, "LIST")) {
                    uint8_t t[4];
                    if (!readBytes(t, 4)) return CHUNK_NONE;
                    if (fcc(t, "movi")) {
                        moviEnd_ = start + padded;
                        break;
                    }
                    if (!seekTo(start + padded)) return CHUNK_NONE;
                    continue;
                }
                if (!skipBytes(padded)) return CHUNK_NONE;
            }
            continue;
        }

        if (!readBytes(h, 8)) return CHUNK_NONE;
        uint32_t sz = rd32(h + 4);
        uint32_t padded = sz + (sz & 1);

        if (fcc(h, "LIST")) {
            // 'rec ' grouping: just descend, its children are normal chunks.
            uint8_t t[4];
            if (!readBytes(t, 4)) return CHUNK_NONE;
            continue;
        }

        // Stream chunks are "NNxx" where NN is the stream index in decimal.
        if (h[0] >= '0' && h[0] <= '9' && h[1] >= '0' && h[1] <= '9') {
            int stream = (h[0] - '0') * 10 + (h[1] - '0');
            bool dc = (h[2] == 'd' && (h[3] == 'c' || h[3] == 'b'));
            bool wb = (h[2] == 'w' && h[3] == 'b');
            if (dc && stream == info_.videoStream) {
                chunkRemain_ = sz;
                chunkPad_ = sz & 1;
                *size = sz;
                return CHUNK_VIDEO;
            }
            if (wb && stream == info_.audioStream) {
                chunkRemain_ = sz;
                chunkPad_ = sz & 1;
                *size = sz;
                return CHUNK_AUDIO;
            }
        }
        // JUNK, ix##, unknown streams: skip.
        if (!skipBytes(padded)) return CHUNK_NONE;
    }
}

int Reader::readChunk(uint8_t* dst, uint32_t cap) {
    uint32_t take = chunkRemain_ < cap ? chunkRemain_ : cap;
    if (take && !readBytes(dst, take)) return -1;
    chunkRemain_ -= take;
    // Any remainder is skipped by the next nextChunk() call, so a caller
    // may keep calling readChunk() to consume a large chunk in pieces.
    return (int)take;
}

bool Reader::skipChunk() {
    bool ok = skipBytes(chunkRemain_ + chunkPad_);
    chunkRemain_ = chunkPad_ = 0;
    return ok;
}

}  // namespace avi
