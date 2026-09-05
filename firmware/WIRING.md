# Wiring

Board: ESP32-2432S028R (2.8" CYD). Pin names refer to the ESP32 GPIO numbers
printed on the board's headers. Check the header labels on your board before
soldering; the 1.25 mm connectors vary between revisions.

## External I2S amplifier (MAX98357A), recommended

The CYD's onboard audio path (internal DAC on GPIO26 into the SC8002B amp)
picks up impulse noise from both SPI buses whenever the SD card or the
display is busy (measured 2026-09-05). An I2S amplifier bypasses that path
entirely and gives 16-bit audio.

Only GPIO22 and GPIO27 are free outputs on the headers, so the old DAC line
doubles as I2S data. The onboard amp then amplifies the data signal into its
unused speaker header, which is harmless.

| MAX98357A pin | Connect to                     | Notes                                              |
|---------------|--------------------------------|----------------------------------------------------|
| VIN           | 5 V (VIN on the P1 header)     | 2.5 to 5.5 V; use 5 V for full output              |
| GND           | GND                            |                                                    |
| BCLK          | GPIO22 (CN1 or P3 header)      | bit clock                                          |
| LRC           | GPIO27 (CN1 header)            | word select                                        |
| DIN           | GPIO26                         | the speaker amp input line; GPIO4 (red LED) works too, change `PIN_I2S_DOUT` in `src/boards/cyd_2432s028r.h` |
| GAIN          | unconnected                    | 9 dB. GND = 12 dB, VIN = 6 dB, 100k to GND = 15 dB, 100k to VIN = 3 dB |
| SD            | unconnected                    | amp enabled, (L+R)/2 output; the firmware sends the same sample on both channels |
| Speaker +/-   | 4 ohm 3 W speaker              | no series pot needed                               |

Firmware: hold the screen for the settings menu, set AUDIO OUT to I2S AMP,
exit. Takes effect at the next episode and is saved in NVS.

## Onboard speaker header (fallback)

Speaker on the 2-pin speaker header. Expect clicks and hiss that track SD
and display activity. A 50 ohm 3 W wirewound pot wired as a rheostat in the
speaker positive lead tames the level; it does not remove the noise.

## Power

5 V through the board's micro USB, or 5 V and GND on the P1 header from a
USB-C breakout mounted in the case. The board draws about 200 mA playing.

## Touch panel

XPT2046 on GPIO25 (CLK), 32 (MOSI), 39 (MISO), 33 (CS), 36 (IRQ), bit-banged.
GPIO25 is also DAC channel 1; the firmware keeps that DAC disabled, otherwise
the panel reads as permanently pressed whenever audio runs.

## Free pins

None once the I2S amp is wired. GPIO35 on the P3 header is input only (LDR or
a button). GPIO0 is the BOOT button. GPIO4, 16, 17 drive the RGB LED and can
be repurposed as outputs if the LED is not needed.
