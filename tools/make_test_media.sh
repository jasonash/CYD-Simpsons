#!/usr/bin/env sh
# Generate a synthetic test AVI (color bars + moving text + 440 Hz tone) in the
# CYD playback format, so the Phase 0 player spike can run without a real episode.
#
#   tools/make_test_media.sh [seconds] [out.avi]
set -eu

SECS="${1:-30}"
OUT="${2:-test_${SECS}s.avi}"
W=288; H=160; FPS=20; Q=8; AR=16000

command -v ffmpeg >/dev/null 2>&1 || { echo "ffmpeg not found (brew install ffmpeg)" >&2; exit 1; }

# Frame counter overlay needs the drawtext filter (libfreetype). Homebrew's
# default ffmpeg build omits it, so fall back to the bare testsrc2 pattern.
if ffmpeg -hide_banner -filters 2>/dev/null | grep -q ' drawtext '; then
  VF="drawtext=text='%{n}':fontsize=24:fontcolor=white:x=10:y=10:box=1:boxcolor=black@0.5,format=yuvj420p"
else
  echo "note: ffmpeg lacks drawtext, skipping frame counter overlay" >&2
  VF="format=yuvj420p"
fi

ffmpeg -hide_banner -y \
  -f lavfi -i "testsrc2=size=${W}x${H}:rate=${FPS}" \
  -f lavfi -i "sine=frequency=440:sample_rate=${AR}" \
  -t "$SECS" \
  -vf "$VF" \
  -c:v mjpeg -q:v "$Q" -vtag MJPG \
  -c:a pcm_u8 -ac 1 -ar "$AR" \
  -f avi "$OUT"

echo "wrote $OUT"
ls -lh "$OUT"
