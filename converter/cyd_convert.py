#!/usr/bin/env python3
"""Convert video into the CYD-Simpsons playback format.

Target: Motion JPEG video + 8-bit unsigned mono PCM audio in an AVI container,
which is what an ESP32 without PSRAM can decode in real time.

Usage:
    cyd_convert.py [--preset balanced] [--out DIR] input1.mkv [input2.mp4 ...]

Requires ffmpeg and ffprobe on PATH.
"""
from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
from dataclasses import asdict, dataclass
from pathlib import Path


@dataclass(frozen=True)
class Preset:
    width: int
    height: int
    fps: int
    # ffmpeg mjpeg -q:v scale: 2 (best) .. 31 (worst). Lower = bigger frames.
    qscale: int
    audio_hz: int
    note: str


# Dimensions are kept to multiples of 16 so JPEG MCU blocks tile cleanly, which
# keeps the strip decoder simple.
PRESETS: dict[str, Preset] = {
    "quality": Preset(288, 160, 24, 6, 22050, "Largest files, smoothest motion"),
    "balanced": Preset(288, 160, 20, 8, 16000, "Default. Should fit the SD budget"),
    "smallest": Preset(224, 128, 20, 10, 16000, "For slow cards or big libraries"),
}

VIDEO_EXTS = {".mkv", ".mp4", ".avi", ".mov", ".m4v", ".webm", ".ts", ".wmv"}


def require_tool(name: str) -> None:
    if shutil.which(name) is None:
        sys.exit(f"error: {name} not found on PATH. Install ffmpeg (brew install ffmpeg).")


def build_ffmpeg_cmd(src: Path, dst: Path, p: Preset) -> list[str]:
    # scale with force_original_aspect_ratio=decrease then pad to the canvas:
    # 4:3 seasons get pillarboxed, 16:9 seasons get letterboxed, automatically.
    vf = (
        f"fps={p.fps},"
        f"scale={p.width}:{p.height}:force_original_aspect_ratio=decrease:flags=lanczos,"
        f"pad={p.width}:{p.height}:(ow-iw)/2:(oh-ih)/2:color=black,"
        f"format=yuvj420p"
    )
    return [
        "ffmpeg", "-hide_banner", "-y",
        "-i", str(src),
        "-map", "0:v:0", "-map", "0:a:0?",
        "-vf", vf,
        "-c:v", "mjpeg", "-q:v", str(p.qscale), "-vtag", "MJPG",
        "-c:a", "pcm_u8", "-ac", "1", "-ar", str(p.audio_hz),
        "-f", "avi",
        str(dst),
    ]


def probe(path: Path) -> dict:
    out = subprocess.run(
        ["ffprobe", "-v", "error", "-show_format", "-show_streams", "-of", "json", str(path)],
        check=True, capture_output=True, text=True,
    ).stdout
    return json.loads(out)


def write_sidecar(dst: Path, src: Path, p: Preset) -> None:
    info = probe(dst)
    video = next((s for s in info["streams"] if s["codec_type"] == "video"), {})
    fmt = info.get("format", {})
    duration = float(fmt.get("duration", 0.0))
    size = int(fmt.get("size", 0))
    sidecar = {
        "source": src.name,
        "preset": asdict(p),
        "duration_s": round(duration, 2),
        "frames": int(video.get("nb_frames", 0) or 0),
        "size_bytes": size,
        "avg_kbps": round(size * 8 / duration / 1000, 1) if duration else None,
    }
    dst.with_suffix(".json").write_text(json.dumps(sidecar, indent=2) + "\n")
    print(f"  {duration/60:.1f} min, {size/1048576:.1f} MB, {sidecar['avg_kbps']} kbps avg")


def collect_inputs(args: list[str]) -> list[Path]:
    files: list[Path] = []
    for a in args:
        p = Path(a)
        if p.is_dir():
            files.extend(sorted(f for f in p.iterdir() if f.suffix.lower() in VIDEO_EXTS))
        elif p.is_file():
            files.append(p)
        else:
            print(f"warning: skipping {a} (not found)", file=sys.stderr)
    return files


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("inputs", nargs="+", help="video files or directories of them")
    ap.add_argument("--preset", choices=PRESETS, default="balanced")
    ap.add_argument("--out", type=Path, default=Path("out"), help="output directory")
    ap.add_argument("--dry-run", action="store_true", help="print ffmpeg commands only")
    ap.add_argument("--force", action="store_true", help="re-encode even if output exists")
    ns = ap.parse_args()

    if not ns.dry_run:
        require_tool("ffmpeg")
        require_tool("ffprobe")

    preset = PRESETS[ns.preset]
    inputs = collect_inputs(ns.inputs)
    if not inputs:
        sys.exit("error: no input files")

    ns.out.mkdir(parents=True, exist_ok=True)
    print(f"preset {ns.preset}: {preset.width}x{preset.height} @ {preset.fps} fps, "
          f"q{preset.qscale}, {preset.audio_hz} Hz u8 mono")

    failures = 0
    for i, src in enumerate(inputs, 1):
        dst = ns.out / (src.stem + ".avi")
        print(f"[{i}/{len(inputs)}] {src.name} -> {dst}")
        if dst.exists() and not ns.force:
            print("  exists, skipping (use --force)")
            continue
        cmd = build_ffmpeg_cmd(src, dst, preset)
        if ns.dry_run:
            print("  " + " ".join(cmd))
            continue
        try:
            subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
            write_sidecar(dst, src, preset)
        except subprocess.CalledProcessError as e:
            failures += 1
            print(f"  FAILED: {e.stderr.strip().splitlines()[-1] if e.stderr else e}", file=sys.stderr)

    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
