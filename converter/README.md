# CYD-Simpsons converter

Desktop-side tool that transcodes episodes into the only format the ESP32 can
play in real time: MJPEG video + 8-bit unsigned mono PCM audio in an AVI.

Requires ffmpeg and ffprobe (`brew install ffmpeg`). Python 3.10+, no other dependencies.

```sh
./cyd_convert.py --preset balanced --out /Volumes/SDCARD/channels/1 ~/Videos/Simpsons/S05/
```

| Preset   | Video            | Audio        | Use                              |
|----------|------------------|--------------|----------------------------------|
| quality  | 288x160 @ 24 fps, q6  | 22050 Hz | Best look, biggest files         |
| balanced | 288x160 @ 20 fps, q8  | 16000 Hz | Default                          |
| smallest | 224x128 @ 20 fps, q10 | 16000 Hz | Slow SD cards or huge libraries  |

4:3 and 16:9 sources are pillarboxed or letterboxed automatically. Each output
gets a `.json` sidecar with duration, frame count, size, and average bitrate.

Use `--dry-run` to print the ffmpeg commands without running them.

Planned (later phases): per-frame size clamp, seek tables, library manifest,
full SD card layout builder.
