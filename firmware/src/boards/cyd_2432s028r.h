// ESP32-2432S028R "Cheap Yellow Display", 2.8" ILI9341 320x240 resistive touch.
//
// Bus map (the reason this board can stream video at all):
//   HSPI  -> display          (pins also given to TFT_eSPI via build flags)
//   VSPI  -> microSD
//   GPIO  -> XPT2046 touch    (not on a hardware SPI host; bit-banged)
//   DAC2  -> GPIO26 -> onboard amp -> speaker header (DAC1 is GPIO25, unused)
#pragma once

#define BOARD_NAME "ESP32-2432S028R"

// Display. TFT_eSPI gets these from build flags; duplicated here for anything
// that needs them at the application level (backlight PWM, etc).
#define PIN_TFT_BL        21
#define TFT_ROTATION      1        // 1 = landscape, USB on the right
#define DISPLAY_W         320
#define DISPLAY_H         240

// microSD on VSPI.
#define PIN_SD_SCK        18
#define PIN_SD_MISO       19
#define PIN_SD_MOSI       23
#define PIN_SD_CS         5
#define SD_SPI_HZ         40000000

// XPT2046 resistive touch, bit-banged.
#define PIN_TOUCH_CLK     25
#define PIN_TOUCH_MOSI    32
#define PIN_TOUCH_MISO    39
#define PIN_TOUCH_CS      33
#define PIN_TOUCH_IRQ     36
// Raw ADC range observed on typical panels; refine with a calibration routine.
#define TOUCH_RAW_MIN     200
#define TOUCH_RAW_MAX     3700

// Audio: internal 8-bit DAC channel 2 on GPIO26 feeds the onboard amplifier and its speaker header.
#define PIN_AUDIO_DAC     26
#define AUDIO_SAMPLE_RATE 16000

// RGB LED is common-anode: LOW turns a color on.
#define PIN_LED_R         4
#define PIN_LED_G         16
#define PIN_LED_B         17
#define LED_ON            LOW
#define LED_OFF           HIGH

// Light-dependent resistor for auto-brightness (stretch goal).
#define PIN_LDR           34

// Physical BOOT button, usable as a channel-change fallback.
#define PIN_BUTTON_BOOT   0
