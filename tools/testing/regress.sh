#!/bin/bash
# Room 1 regression against the original's reference shots.
cd "$(dirname "$0")/../.."
E=./build/engine/edison.exe
run() { rm -rf "$1" && mkdir -p "$1" && shift_dir="$1" && shift && timeout 90 $E --game science --room 1 -A --hidden --capture "$shift_dir" 100 "$@" >/dev/null 2>&1; }
cmpq() { python build/scratch/cmp.py "$@" 2>/dev/null; }
run build/scratch/q_rest --quit-after 2000
echo "rest: $(python tools/reference/screendiff.py $(ls build/scratch/q_rest/*.bmp | tail -1) build/scratch/room1_ref.png 2>/dev/null)"
run build/scratch/q_sl --drag 1000 440 390 440 390 600 --move 1600 445 390 --drag 3000 220 305 220 305 600 --move 3600 225 305 --drag 5000 100 305 100 305 600 --move 5600 105 305 --quit-after 7000
echo "sliders:"; cmpq build/scratch/q_sl build/scratch/orig_play4 2000:s1 4000:s2
args=""; t=1000; for p in "200 200 205 200" "330 345 335 345" "330 345 330 350" "330 345 335 345" "330 345 330 350" "330 345 335 345"; do set -- $p; args="$args --drag $t $1 $2 $1 $2 80 --move $((t+300)) $3 $4"; t=$((t+1000)); done
run build/scratch/q_bt $args --quit-after 7000
echo "aim+types:"; cmpq build/scratch/q_bt build/scratch/orig_play3 1900:r1 2900:r2 3900:r3 4900:r4 5900:r5 6900:r6
args=""; t=1000; for c in "150 150" "300 120" "480 160" "250 260" "350 200" "520 230" "120 240" "450 270"; do set -- $c; args="$args --drag $t $1 $2 $1 $2 80 --move $((t+300)) $(( $1 + 4 )) $2"; t=$((t+1000)); done
run build/scratch/q_aim $args --quit-after 9500
echo "aims:"; cmpq build/scratch/q_aim build/scratch/orig_aim 1900:a1 2900:a2 3900:a3 4900:a4 5900:a5 6900:a6 7900:a7 8900:a8
run build/scratch/q_shot --drag 1000 180 250 180 250 80 --move 1300 184 250 --drag 2000 540 340 540 340 80 --move 2100 544 340 --quit-after 13000
echo "shot end: $(python tools/reference/screendiff.py $(ls build/scratch/q_shot/*.bmp | tail -1) build/scratch/orig_shot/bend.png 2>/dev/null)"
cmpq build/scratch/q_shot build/scratch/orig_shot 12800:bend
