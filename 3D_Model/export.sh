#!/usr/bin/env bash
# Render every printable part of simpsons_tv.scad to 3D_Model/stl/<part>.stl
# and refresh the preview PNGs. Requires OpenSCAD (2021.01 app bundle on macOS).
set -euo pipefail
cd "$(dirname "$0")"
OPENSCAD="${OPENSCAD:-/Applications/OpenSCAD-2021.01.app/Contents/MacOS/OpenSCAD}"
[ -x "$OPENSCAD" ] || OPENSCAD="$(command -v openscad)"
PARTS="front_shell rear_shell knob knob_keeper button_carrier grill leg_l leg_r set_top_box antenna speaker_clamp"
mkdir -p stl preview
for p in ${1:-$PARTS}; do
    echo "== $p"
    "$OPENSCAD" -q -D "part=\"$p\"" -o "stl/$p.stl" simpsons_tv.scad 2>&1 | tee /dev/stderr | grep -qiE "WARNING|ERROR" && { echo "OpenSCAD warned while rendering $p, aborting"; exit 1; }
done
echo "== mesh check (one closed shell per part)"
python3 check_stl.py --strict $(for p in ${1:-$PARTS}; do echo "stl/$p.stl"; done)
if [ $# -eq 0 ]; then
    echo "== previews"
    "$OPENSCAD" -q -o preview/front.png    --imgsize=1400,1000 --projection=p --camera=-60,-300,170,59,30,45 --colorscheme=Tomorrow simpsons_tv.scad
    "$OPENSCAD" -q -o preview/rear.png     --imgsize=1400,1000 --projection=p --camera=230,350,160,59,30,45 --colorscheme=Tomorrow simpsons_tv.scad
    "$OPENSCAD" -q -o preview/exploded.png --imgsize=1400,1000 --projection=p --camera=-150,-330,200,59,30,45 --colorscheme=Tomorrow -D explode=20 simpsons_tv.scad
fi
echo done
