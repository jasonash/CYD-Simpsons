// CYD-Simpsons: Phase 0 bring-up and measurement sketch.
//
// Goal: prove each subsystem works on this exact board and collect the numbers
// the design depends on (SD throughput, free RAM). Results print over serial at
// 115200 and are summarized on the display.
//
// Once numbers are recorded in docs/PROJECT_STATUS.md, this file becomes the
// real firmware entry point and the checks move into a diagnostics mode.

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <TFT_eSPI.h>
#include <XPT2046_Bitbang.h>

#include "boards/board.h"

static TFT_eSPI tft;
static SPIClass sdSpi(VSPI);
static XPT2046_Bitbang touch(PIN_TOUCH_MOSI, PIN_TOUCH_MISO, PIN_TOUCH_CLK, PIN_TOUCH_CS);

static bool sdOk = false;
static float sdMBps = 0.0f;
static uint16_t lineY = 0;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static void logLine(const char *fmt, ...) {
    char buf[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    Serial.println(buf);
    if (lineY < DISPLAY_H - 10) {
        tft.drawString(buf, 4, lineY, 2);
        lineY += 16;
    }
}

static void setLed(bool r, bool g, bool b) {
    digitalWrite(PIN_LED_R, r ? LED_ON : LED_OFF);
    digitalWrite(PIN_LED_G, g ? LED_ON : LED_OFF);
    digitalWrite(PIN_LED_B, b ? LED_ON : LED_OFF);
}

// ---------------------------------------------------------------------------
// Subsystem checks
// ---------------------------------------------------------------------------
static void checkDisplay() {
    tft.init();
    tft.setRotation(TFT_ROTATION);

    // Color bars: instantly shows wrong driver, wrong RGB order, or inversion.
    const uint16_t bars[] = {TFT_WHITE, TFT_YELLOW, TFT_CYAN, TFT_GREEN,
                             TFT_MAGENTA, TFT_RED, TFT_BLUE, TFT_BLACK};
    const int barW = DISPLAY_W / 8;
    for (int i = 0; i < 8; i++) {
        tft.fillRect(i * barW, 0, barW, DISPLAY_H, bars[i]);
    }
    delay(5000);

    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    lineY = 4;
}

static void checkSd() {
    sdSpi.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
    sdOk = SD.begin(PIN_SD_CS, sdSpi, SD_SPI_HZ);
    if (!sdOk) {
        logLine("SD: mount FAILED");
        return;
    }

    uint64_t sizeMB = SD.cardSize() / (1024ULL * 1024ULL);
    logLine("SD: mounted, %llu MB, type %d", sizeMB, (int)SD.cardType());

    // List the root so we know the card layout is what we think it is.
    File root = SD.open("/");
    int count = 0;
    for (File f = root.openNextFile(); f && count < 6; f = root.openNextFile()) {
        logLine("  %s %s (%u KB)", f.isDirectory() ? "[D]" : "   ", f.name(),
                (unsigned)(f.size() / 1024));
        count++;
    }
    root.close();
}

// Sustained sequential read benchmark. Reads the largest file in the root
// (or /bench.bin if present) in 4 KB chunks for up to 8 MB. This is the number
// that caps total media bitrate.
static void benchSd() {
    if (!sdOk) return;

    String target = "/bench.bin";
    if (!SD.exists(target)) {
        File root = SD.open("/");
        size_t best = 0;
        for (File f = root.openNextFile(); f; f = root.openNextFile()) {
            if (!f.isDirectory() && f.size() > best) {
                best = f.size();
                target = String("/") + f.name();
            }
        }
        root.close();
        if (best == 0) {
            logLine("SD bench: no file to read");
            return;
        }
    }

    File f = SD.open(target);
    if (!f) {
        logLine("SD bench: open failed");
        return;
    }

    // Run the read at several chunk sizes: the player's read granularity is
    // a tuning knob, and this tells us how much of the ceiling is the card
    // versus per-call FatFs overhead.
    static uint8_t buf[16384];
    const size_t limit = 8UL * 1024UL * 1024UL;
    const size_t chunks[] = {4096, 16384};
    for (size_t chunk : chunks) {
        f.seek(0);
        size_t total = 0;
        uint32_t t0 = millis();
        while (total < limit) {
            int n = f.read(buf, chunk);
            if (n <= 0) break;
            total += n;
        }
        uint32_t dt = millis() - t0;
        sdMBps = dt > 0 ? (total / 1048576.0f) / (dt / 1000.0f) : 0.0f;
        logLine("SD bench %uK: %u KB in %u ms = %.2f MB/s", (unsigned)(chunk / 1024),
                (unsigned)(total / 1024), (unsigned)dt, sdMBps);
    }
    f.close();
}

// Short test tone through the internal DAC so we know the amp and speaker are
// alive. Blocking; fine for bring-up. Real playback will use I2S in DAC mode.
static void checkAudio() {
    const int freqHz = 440;
    const int durMs = 300;
    const int rate = 16000;
    const int samples = rate * durMs / 1000;
    const uint32_t periodUs = 1000000UL / rate;

    for (int i = 0; i < samples; i++) {
        float phase = (float)(i % (rate / freqHz)) / (rate / freqHz);
        uint8_t v = 128 + (uint8_t)(60.0f * sinf(phase * 2.0f * PI));
        dacWrite(PIN_AUDIO_DAC, v);
        delayMicroseconds(periodUs);
    }
    dacWrite(PIN_AUDIO_DAC, 128);
    logLine("Audio: 440 Hz tone sent to GPIO%d", PIN_AUDIO_DAC);
}

static void checkMisc() {
    logLine("Board: %s @ %u MHz", BOARD_NAME, (unsigned)getCpuFrequencyMhz());
    logLine("Heap: %u free, %u largest block", (unsigned)ESP.getFreeHeap(),
            (unsigned)ESP.getMaxAllocHeap());
    logLine("PSRAM: %u", (unsigned)ESP.getPsramSize());
    logLine("LDR: %d", analogRead(PIN_LDR));
}

// ---------------------------------------------------------------------------
// Arduino entry points
// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("\nCYD-Simpsons bring-up");

    pinMode(PIN_LED_R, OUTPUT);
    pinMode(PIN_LED_G, OUTPUT);
    pinMode(PIN_LED_B, OUTPUT);
    setLed(true, false, false);  // red while testing

    checkDisplay();
    checkMisc();
    checkSd();
    benchSd();
    checkAudio();

    touch.begin();
    logLine("Touch: ready, tap the screen");

    setLed(false, sdOk, !sdOk);  // green if SD works, blue if not
}

void loop() {
    TouchPoint p = touch.getTouch();
    if (p.zRaw > 0) {
        int x = map(p.xRaw, TOUCH_RAW_MIN, TOUCH_RAW_MAX, 0, DISPLAY_W);
        int y = map(p.yRaw, TOUCH_RAW_MIN, TOUCH_RAW_MAX, 0, DISPLAY_H);
        x = constrain(x, 0, DISPLAY_W - 1);
        y = constrain(y, 0, DISPLAY_H - 1);
        tft.fillCircle(x, y, 3, TFT_YELLOW);
        Serial.printf("touch raw=(%d,%d,%d) mapped=(%d,%d)\n", p.xRaw, p.yRaw,
                      p.zRaw, x, y);
        delay(30);
    }
    delay(10);
}
