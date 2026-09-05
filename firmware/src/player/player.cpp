#include "player.h"

#include <Arduino.h>
#include <JPEGDEC.h>
#include <TFT_eSPI.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "../boards/board.h"
#include "audio_out.h"
#include "avi_reader.h"

namespace player {

// Video frames are read ahead into a small ring so the audio DMA queue
// (128 ms) can stay full while the frame that matches the audio being
// played is the one on screen. 4 slots = 200 ms at 20 fps, which covers
// the DMA depth plus one frame of decode jitter.
static const int kRingSlots = 4;
static const uint32_t kSlotBytes = 24 * 1024;   // largest JPEG frame accepted
static const uint32_t kAudioBufBytes = 4 * 1024;

struct Slot {
    uint8_t* buf = nullptr;
    uint32_t len = 0;
    uint32_t index = 0;   // frame number in the file
};

static TFT_eSPI* s_tft = nullptr;
static JPEGDEC s_jpeg;
static bool s_dma = false;
static bool s_dmaInit = false;   // set by initDisplayDma()

// Attach TFT_eSPI's DMA engine. Call after tft.init() and BEFORE the SD
// driver is started, so the display gets DMA channel 1 and the SD driver's
// automatic pick lands on channel 2.
bool initDisplayDma(TFT_eSPI* tft) {
    s_dmaInit = tft->initDMA();
    return s_dmaInit;
}
static bool dmaAvailable() { return s_dmaInit; }
static Slot s_ring[kRingSlots];
static uint8_t* s_audioBuf = nullptr;
static Stats s_stats;
static int s_offX = 0, s_offY = 0;   // letterbox origin on screen

// Reader task (core 0) -> decoder (core 1, the caller of play()).
// freeQ holds slot indices the reader may fill; fullQ holds filled slots in
// file order. A -1 on fullQ means end of file.
static QueueHandle_t s_freeQ = nullptr;
static QueueHandle_t s_fullQ = nullptr;
static const int kEndOfFile = -1;
static volatile bool s_stopReq = false;
static bool s_wasStopped = false;

struct ReaderArgs {
    avi::Reader* rd;
    bool useAudio;
};

// Runs on core 0. Owns the SD reads and the blocking audio writes, so the
// decoder never waits on I/O and the DAC never waits on a decode.
static void readerTask(void* p) {
    ReaderArgs* a = (ReaderArgs*)p;
    avi::Reader& rd = *a->rd;
    uint32_t frameIndex = 0;
    uint32_t chunkSize = 0;

    while (!s_stopReq) {
        avi::ChunkType ct = rd.nextChunk(&chunkSize);
        if (ct == avi::CHUNK_NONE) break;

        if (ct == avi::CHUNK_AUDIO) {
            uint32_t remain = chunkSize;
            while (remain) {
                uint32_t take = remain < kAudioBufBytes ? remain : kAudioBufBytes;
                uint32_t r0 = micros();
                int got = rd.readChunk(s_audioBuf, take);
                s_stats.winReadUs += micros() - r0;
                if (got <= 0) { s_stats.audioShortWrites++; break; }
                if (a->useAudio) {
                    uint32_t w0 = micros();
                    size_t w = audio::write(s_audioBuf, (size_t)got);
                    s_stats.winAudioWaitUs += micros() - w0;
                    if (w < (size_t)got) s_stats.audioShortWrites++;
                }
                remain -= (uint32_t)got;
            }
            s_stats.audioChunks++;
            continue;
        }

        uint32_t myIndex = frameIndex++;
        if (chunkSize > s_stats.maxFrameBytes) s_stats.maxFrameBytes = chunkSize;
        if (chunkSize > kSlotBytes) {
            s_stats.framesBad++;
            rd.skipChunk();
            continue;
        }
        int slot;
        xQueueReceive(s_freeQ, &slot, portMAX_DELAY);
        Slot& sl = s_ring[slot];
        uint32_t r0 = micros();
        int got = rd.readChunk(sl.buf, kSlotBytes);
        s_stats.winReadUs += micros() - r0;
        if (got <= 0) {
            s_stats.framesBad++;
            xQueueSend(s_freeQ, &slot, portMAX_DELAY);
            continue;
        }
        sl.len = (uint32_t)got;
        sl.index = myIndex;
        xQueueSend(s_fullQ, &slot, portMAX_DELAY);
    }
    int eof = kEndOfFile;
    xQueueSend(s_fullQ, &eof, portMAX_DELAY);
    vTaskDelete(nullptr);
}

// JPEGDEC hands us one block of MCUs at a time; push it straight to the
// panel. With DMA, JPEGDEC alternates between the two halves of its pixel
// buffer (JPEG_USES_DMA), so this block can go out over SPI while the next
// one decodes. pushImageDMA waits for the previous transfer, swaps bytes in
// place, and queues this one.
static int drawMcu(JPEGDRAW* d) {
    if (s_dma) {
        s_tft->pushImageDMA(d->x + s_offX, d->y + s_offY, d->iWidth, d->iHeight, d->pPixels);
    } else {
        s_tft->pushImage(d->x + s_offX, d->y + s_offY, d->iWidth, d->iHeight, d->pPixels);
    }
    return 1;
}

void begin(TFT_eSPI* tft) {
    s_tft = tft;
    for (int i = 0; i < kRingSlots; i++) {
        if (!s_ring[i].buf) s_ring[i].buf = (uint8_t*)malloc(kSlotBytes);
    }
    if (!s_audioBuf) s_audioBuf = (uint8_t*)malloc(kAudioBufBytes);
    if (!s_freeQ) s_freeQ = xQueueCreate(kRingSlots, sizeof(int));
    if (!s_fullQ) s_fullQ = xQueueCreate(kRingSlots + 1, sizeof(int));
    // JPEGDEC emits native little-endian RGB565 in RAM. TFT_eSPI's pushImage
    // needs swapBytes(true) for uint16 pixel data in RAM (its default
    // expects byte-swapped image arrays from flash). The other pairing,
    // RGB565_BIG_ENDIAN + swapBytes(false), came out with yellow rendered
    // as blue on the ILI9341 (2026-09-03).
    s_jpeg.setPixelType(RGB565_LITTLE_ENDIAN);
    s_tft->setSwapBytes(true);
    // The DMA engine must be attached before the SD driver takes a DMA
    // channel (main.cpp calls initDMA right after tft.init()). If that did
    // not happen, fall back to CPU-driven pushes.
    s_dma = dmaAvailable();
    Serial.printf("[player] display DMA %s\n", s_dma ? "on" : "off");
}

const Stats& stats() { return s_stats; }

static void report(const avi::Info& info) {
    Stats& s = s_stats;
    uint32_t winMs = millis() - s.winStartMs;
    float fps = winMs ? (s.winFrames * 1000.0f / winMs) : 0.0f;
    float decAvg = s.winFrames ? s.winDecodeUs / 1000.0f / s.winFrames : 0.0f;
    float readAvg = s.winFrames ? s.winReadUs / 1000.0f / s.winFrames : 0.0f;
    float audioWait = s.winFrames ? s.winAudioWaitUs / 1000.0f / s.winFrames : 0.0f;
    float syncWait = s.winFrames ? s.winSyncWaitUs / 1000.0f / s.winFrames : 0.0f;
    Serial.printf("[player] t=%5.1fs fps=%5.2f shown=%u drop=%u bad=%u | decode avg %.1f max %.1f ms | "
                  "sd %.1f ms | i2s wait %.1f ms | sync wait %.1f ms | drift %+d ms | underrun %u | aq min %u | short %u | maxfrm %u B | heap %u\n",
                  s.elapsedMs / 1000.0f, fps, s.framesShown, s.framesDropped, s.framesBad,
                  decAvg, s.winDecodeMaxUs / 1000.0f, readAvg, audioWait, syncWait,
                  (int)s.avDriftMs, (unsigned)s.audioUnderruns, (unsigned)s.winAudioQueueMin,
                  (unsigned)s.audioShortWrites, (unsigned)s.maxFrameBytes,
                  (unsigned)ESP.getFreeHeap());
    s.winFrames = 0;
    s.winDecodeUs = s.winDecodeMaxUs = s.winReadUs = 0;
    s.winAudioWaitUs = s.winSyncWaitUs = 0;
    s.winAudioQueueMin = UINT32_MAX;
    s.winStartMs = millis();
}

bool wasStopped() { return s_wasStopped; }

static uint32_t s_busyMs = 0;
void setBusyMs(uint32_t ms) { s_busyMs = ms; }
uint32_t busyMs() { return s_busyMs; }

bool play(const char* path, uint32_t reportEveryFrames, StopFn stop) {
    if (!s_tft || !s_ring[0].buf || !s_audioBuf) {
        Serial.println("[player] not initialised");
        return false;
    }

    avi::Reader rd;
    if (!rd.open(path)) {
        Serial.printf("[player] cannot open %s: %s\n", path, rd.lastError());
        return false;
    }
    const avi::Info& info = rd.info();
    Serial.printf("[player] %s: %ux%u %s, %u us/frame (%.2f fps), %u frames, audio %s %u Hz %u ch %u bit, %u KB\n",
                  path, info.width, info.height, info.videoFourcc, info.usPerFrame,
                  info.usPerFrame ? 1e6f / info.usPerFrame : 0.0f, info.totalFrames,
                  info.hasAudio ? "yes" : "no", info.audioRate, info.audioChannels, info.audioBits,
                  info.fileSize / 1024);

    bool useAudio = info.hasAudio && info.audioFormat == 1 && info.audioChannels == 1 && info.audioBits == 8;
    if (info.hasAudio && !useAudio) {
        Serial.println("[player] audio format unsupported (need PCM u8 mono), playing silent");
    }

    s_offX = ((int)DISPLAY_W - (int)info.width) / 2;
    s_offY = ((int)DISPLAY_H - (int)info.height) / 2;
    if (s_offX < 0) s_offX = 0;
    if (s_offY < 0) s_offY = 0;
    s_tft->fillScreen(TFT_BLACK);

    s_stats = Stats();
    s_stats.winStartMs = millis();
    s_stats.winAudioQueueMin = UINT32_MAX;
    s_stopReq = false;
    s_wasStopped = false;

    if (useAudio && !audio::begin(info.audioRate)) {
        Serial.println("[player] audio init failed, playing silent");
        useAudio = false;
    }

    const uint32_t usPerFrame = info.usPerFrame ? info.usPerFrame : 50000;
    const uint32_t t0 = millis();

    // Reset the ring queues and start the reader on the other core.
    xQueueReset(s_freeQ);
    xQueueReset(s_fullQ);
    for (int i = 0; i < kRingSlots; i++) xQueueSend(s_freeQ, &i, 0);
    ReaderArgs args = {&rd, useAudio};
    TaskHandle_t reader = nullptr;
    xTaskCreatePinnedToCore(readerTask, "avi_reader", 4096, &args, 2, &reader, 0);

    auto clockMs = [&]() -> uint32_t {
        return useAudio ? audio::clockMs() : (millis() - t0);
    };

    uint32_t presented = 0;
    for (;;) {
        int slot;
        xQueueReceive(s_fullQ, &slot, portMAX_DELAY);
        if (slot == kEndOfFile) break;
        if (!s_stopReq && stop && stop()) {
            // Tell the reader to wind down, then keep recycling slots so it
            // can reach its end-of-file marker (it may be blocked on freeQ).
            s_stopReq = true;
            s_wasStopped = true;
        }
        if (s_stopReq) {
            xQueueSend(s_freeQ, &slot, portMAX_DELAY);
            continue;
        }
        Slot& sl = s_ring[slot];
        uint32_t ptsMs = (uint32_t)(((uint64_t)sl.index * usPerFrame) / 1000);
        uint32_t nowMs = clockMs();

        if (nowMs > ptsMs + usPerFrame / 1000) {
            // More than a frame late: drop. Audio never waits for video.
            s_stats.framesDropped++;
        } else {
            if (ptsMs > nowMs) {
                uint32_t w0 = micros();
                while ((nowMs = clockMs()) < ptsMs) delay(1);
                s_stats.winSyncWaitUs += micros() - w0;
            }
            s_stats.avDriftMs = (int32_t)ptsMs - (int32_t)nowMs;
            uint32_t d0 = micros();
            if (s_jpeg.openRAM(sl.buf, sl.len, drawMcu)) {
                if (s_dma) s_tft->startWrite();
                s_jpeg.decode(0, 0, s_dma ? JPEG_USES_DMA : 0);
                s_jpeg.close();
                if (s_dma) {
                    s_tft->dmaWait();
                    s_tft->endWrite();
                }
                if (s_busyMs) {
                    uint32_t b0 = micros();
                    while (micros() - b0 < s_busyMs * 1000) { /* spin */ }
                }
                uint32_t dus = micros() - d0;
                s_stats.winDecodeUs += dus;
                if (dus > s_stats.winDecodeMaxUs) s_stats.winDecodeMaxUs = dus;
                s_stats.framesShown++;
                s_stats.winFrames++;
            } else {
                s_stats.framesBad++;
            }
        }
        xQueueSend(s_freeQ, &slot, portMAX_DELAY);

        presented++;
        s_stats.elapsedMs = millis() - t0;
        if (useAudio) {
            s_stats.audioUnderruns = audio::underruns();
            uint32_t q = audio::queuedSamples();
            if (q < s_stats.winAudioQueueMin) s_stats.winAudioQueueMin = q;
        }
        if (reportEveryFrames && (presented % reportEveryFrames) == 0) report(info);
    }

    s_stats.elapsedMs = millis() - t0;
    report(info);
    Serial.printf("[player] %s: %u shown, %u dropped, %u bad in %.1f s (%.2f fps overall), audio played %llu samples\n",
                  s_wasStopped ? "stopped" : "done", s_stats.framesShown, s_stats.framesDropped, s_stats.framesBad,
                  s_stats.elapsedMs / 1000.0f,
                  s_stats.elapsedMs ? s_stats.framesShown * 1000.0f / s_stats.elapsedMs : 0.0f,
                  useAudio ? audio::samplesPlayed() : 0ULL);

    if (useAudio) audio::end();
    rd.close();
    return true;
}

}  // namespace player
