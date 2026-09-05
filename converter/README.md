# CYD-Simpsons converter

Desktop-side tool that transcodes episodes into the only format the ESP32 can
play in real time: MJPEG video + 8-bit unsigned mono PCM audio in an AVI.

Requires ffmpeg and ffprobe (`brew install ffmpeg`). Python 3.10+, no other dependencies.

```sh
./cyd_convert.py --preset balanced --out /Volumes/SDCARD/channels/1 ~/Videos/Simpsons/S05/
```

| Preset   | Video                  | Audio    | Use                                              |
|----------|------------------------|----------|--------------------------------------------------|
| quality  | 320x240 @ 20 fps, q8   | 16000 Hz | Smoothest; about 1% dropped frames in busy scenes |
| balanced | 320x240 @ 15 fps, q8   | 16000 Hz | Default. Fills the panel with decode headroom    |
| smallest | 320x240 @ 15 fps, q12  | 16000 Hz | Smaller files, softer picture                    |

All presets fill the 320x240 panel. Sources are scaled to cover the canvas and
centre-cropped by default (`--fit cover`); `--fit contain` letterboxes instead.
`--fps N` and `--qscale N` override a preset. Each output gets a `.json`
sidecar with duration, frame count, size, and average bitrate.

Expect roughly 10 MB per minute at the default preset (a 22 minute episode is
about 230 MB).

Use `--dry-run` to print the ffmpeg commands without running them.

Planned (later phases): per-frame size clamp, seek tables, library manifest,
full SD card layout builder.
