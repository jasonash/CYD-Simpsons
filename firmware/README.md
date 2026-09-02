# CYD-Simpsons firmware

PlatformIO project targeting the ESP32-2432S028R Cheap Yellow Display.

## Build and flash

Install the PlatformIO CLI (`pipx install platformio`) or the VS Code extension, then:

```sh
cd firmware
pio run -e cyd_2432s028r -t upload
pio device monitor
```

If the screen stays white or colors are wrong, try the ST7789 clone profile:

```sh
pio run -e cyd_2432s028r_st7789 -t upload
```

## Layout

```
src/
  main.cpp        Phase 0 bring-up sketch (display, SD benchmark, touch, DAC tone)
  boards/         Board profiles. All GPIO numbers live here.
  player/         AVI parse, JPEG strip decode, A/V sync      (Phase 0/1)
  channels/       Scheduler, shuffle bag, broadcast position   (Phase 1/2)
  effects/        Static, fuzz, channel OSD                    (Phase 1/3)
  web/            HTTP server, JSON API, embedded assets       (Phase 4)
```

## Phase 0 bring-up sketch

On boot the sketch shows color bars, then prints a report to the display and serial:

- CPU speed, free heap, largest allocatable block, PSRAM (expect 0)
- SD mount status, card size, first few root entries
- Sustained sequential read speed (reads `/bench.bin` if present, else the largest root file)
- A 440 Hz tone through the speaker
- Touch coordinates drawn as dots and printed over serial

Record the SD MB/s and heap numbers in the project status doc. They set the media bitrate budget.
