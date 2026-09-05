# CYD-Simpsons

Turn a Cheap Yellow Display (ESP32-2432S028R) into a tiny, always-on retro
television that plays The Simpsons from a microSD card.

It is deliberately TV-like, not media-player-like: you turn it on and something
is already playing. Tap to change the channel with a burst of static. A
randomizer decides what airs. Fake commercials and interference are planned.

You supply your own legally obtained episodes. This project ships no media.

## Status

Phase 0 (feasibility spike). Bring-up firmware and a converter exist; real-time
playback is not implemented yet. See `firmware/README.md`.

## Hardware

- ESP32-2432S028R "CYD": ESP32-WROOM-32, 2.8" ILI9341 320x240, XPT2046 touch, microSD, speaker header. No PSRAM, which shapes the whole design.
- microSD card, A1 class recommended, FAT32.
- Small speaker on the onboard amp header.

## Layout

```
firmware/    PlatformIO project for the ESP32
converter/   Python + ffmpeg tool that makes playable AVI files
tools/       Test media generator, helper scripts
```

## Quick start

1. `cd firmware && pio run -t upload && pio device monitor` to flash the bring-up sketch.
2. `tools/make_test_media.sh 30` to generate a test clip, copy it to the SD root.
3. `converter/cyd_convert.py --out /Volumes/SD/ episodes/` to convert real episodes.
   Sources are scaled to fill the screen and centre-cropped by default
   (`--fit cover`); pass `--fit contain` for letterbox/pillarbox instead.

## Media format

MJPEG video (320x240, 15 fps by default) with 8-bit mono PCM audio in an AVI
container. Roughly 230 MB per 22 minute episode.

## License

Apache 2.0. See LICENSE.
