#!/bin/bash
# Replays shots in the port and compares them with traces of the original
# kept from earlier checks (build/scratch/..., not in the repository; each
# made with memwatch.py, see README.md). A case whose trace is missing is
# skipped. Usage: retrace.sh [NAME...]
cd "$(dirname "$0")/../.."
E=./build/engine/edison.exe
OUT=build/scratch/retrace; mkdir -p $OUT
BODIES=' (-?\d+),(-?\d+),(-?\d+),(-?\d+),(-?\d+),(-?\d+) (-?\d+),(-?\d+),(-?\d+),(-?\d+),(-?\d+),(-?\d+) (-?\d+),(-?\d+),(-?\d+),(-?\d+),(-?\d+),(-?\d+) (-?\d+),(-?\d+),(-?\d+),(-?\d+),(-?\d+),(-?\d+)'
REM=' c (-?\d+),(-?\d+),(-?\d+) v (-?\d+),(-?\d+),(-?\d+) r (-?\d+),(-?\d+),(-?\d+)'
# name | original trace | port pattern (empty: c and v; rem: with the
# remainders; bodies: the ball and three loose magnets) | port arguments
cases=(
"mode0|build/scratch/m1/p0/o2.txt|rem|--room 35 --click 1000 330 225 --click 2500 330 225 --drag 12000 208 266 208 266 80 --move 12180 212 266 --drag 13000 540 340 540 340 80 --move 13130 544 340 --quit-after 22000"
"mode1back|build/scratch/m1/o101/trace.txt||--room 2 --drag 2000 400 176 400 176 80 --move 2180 404 176 --drag 3000 540 340 540 340 80 --move 3130 544 340 --click 7000 316 224 --click 7500 316 224 --click 8000 316 224 --quit-after 13000"
"mode1left|build/scratch/m1/o100/trace.txt||--room 2 --drag 2000 156 212 156 212 80 --move 2180 160 212 --drag 3000 540 340 540 340 80 --move 3130 544 340 --click 7000 316 224 --click 7500 316 224 --click 8000 316 224 --quit-after 13000"
"lipsStone|build/scratch/t7/olips/trace6.txt||--room 2 --drag 2000 122 236 122 236 80 --move 2180 126 236 --drag 3000 540 340 540 340 80 --move 3130 544 340 --quit-after 6000"
"magnets3|build/scratch/mag/o3/trace.txt|rem|--room 3 --quit-after 14000"
"magnets13|build/scratch/mag/o13/trace.txt|bodies|--room 13 --quit-after 14000"
"magnets33|build/scratch/mag/o33/trace.txt|rem|--room 33 --drag 3000 334 330 334 330 80 --move 3150 330 300 --drag 4000 410 170 410 170 80 --move 4180 414 170 --drag 5000 540 340 540 340 80 --move 5130 544 340 --quit-after 14000"
"lever61|build/scratch/sw/o61/trace.txt||--room 61 --click 2000 349 78 --click 8200 349 78 --quit-after 13000"
"levers62|build/scratch/sw/o62/trace.txt||--room 62 --quit-after 13000"
"bullseye29|build/scratch/sw/o29/trace.txt||--room 29 --drag 2000 270 216 270 216 80 --move 2180 274 216 --drag 3000 540 340 540 340 80 --move 3130 544 340 --quit-after 11000"
"lipsIce|build/scratch/t7/olips0/trace.txt||--room 2 --drag 4000 334 330 334 330 80 --move 4150 330 300 --drag 4600 334 330 334 330 80 --move 4750 330 300 --drag 5200 334 330 334 330 80 --move 5350 330 300 --drag 5800 334 330 334 330 80 --move 5950 330 300 --drag 6400 122 236 122 236 80 --move 6580 126 236 --drag 7400 540 340 540 340 80 --move 7530 544 340 --quit-after 20000"
)
for c in "${cases[@]}"; do
  IFS='|' read -r name trace pattern args <<< "$c"
  if [ $# -gt 0 ] && [[ " $* " != *" $name "* ]]; then continue; fi
  if [ ! -f "$trace" ]; then echo "$name: no trace ($trace)"; continue; fi
  log=$OUT/$name.log; rm -f $log
  SCI_DEBUG=1 EDISON_LOG=$log timeout 120 $E --game science -A --hidden $args >/dev/null 2>&1
  if [ "$pattern" = rem ]; then r=$(python tools/reference/tracecmp.py "$trace" $log --port-pattern "$REM" | tail -1)
  elif [ "$pattern" = bodies ]; then r=$(python tools/reference/tracecmp.py "$trace" $log --port-pattern "$BODIES" | tail -1)
  else r=$(python tools/reference/tracecmp.py "$trace" $log | tail -1); fi
  echo "$name: $r"
done
