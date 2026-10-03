#!/bin/bash
# Traces one shot in the original (memwatch) and the port (SCI_DEBUG), then
# compares: trace.sh NAME "EVENTS"
# EVENTS: "ms x y hold;..." presses (held hold ms, then a move 4 right),
# times from the room's start (the original's room is up 9 s after start).
cd "$(dirname "$0")/../.."
name=$1; events=$2; total=${3:-14000}
out=build/scratch/tr_$name; rm -rf $out; mkdir -p $out
W=D:/tools/edison-run/WMAINSKP.EXE
B="[[5ffc+ae]+f77]"
EXPRS="cx=[$B]+2 cy=[$B]+4 cz=[$B]+6 vx=[$B+2]+62 vy=[$B+2]+64 vz=[$B+2]+66"

# The original's script and the port's arguments, from the same events.
script=$out/orig.txt; args=""; t=0; echo "wait 9" > $script
IFS=';' read -ra evs <<< "$events"
for e in "${evs[@]}"; do
  set -- $e
  echo "wait $(python -c "print(max(0,($1-$t)/1000))")" >> $script
  echo "down $2 $3" >> $script; echo "wait $(python -c "print($4/1000)")" >> $script
  echo "up $2 $3" >> $script; echo "wait 0.1" >> $script; echo "move $(( $2 + 4 )) $3" >> $script
  t=$(( $1 + $4 + 100 ))
  args="$args --drag $1 $2 $3 $2 $3 $4 --move $t $(( $2 + 4 )) $3"
done
echo "wait $(python -c "print(($total-$t)/1000)")" >> $script
echo "shot end.png" >> $script

# The port.
rm -f $out/port.log; mkdir -p $out/port
SCI_DEBUG=1 EDISON_LOG=$out/port.log timeout 90 ./build/engine/edison.exe --game science --room 1 -A --hidden \
  --capture $out/port 100 $args --quit-after $total >/dev/null 2>&1

# The original (no other winevdm left: memwatch would find its memory).
gone() { for i in $(seq 1 50); do tasklist | grep -qi otvdmw || return 0; sleep 0.2; done; }
gone
pwsh -NoProfile -File tools/reference/otvdm.ps1 start $W >/dev/null
(pwsh -NoProfile -File tools/reference/otvdm.ps1 run $script $out >/dev/null &)
sleep 8
python tools/reference/memwatch.py watch $W --every 1 --for $(( total / 1000 + 3 )) --out $out/orig_trace.txt $EXPRS >/dev/null
sleep 2  # (the script's last shot)
pwsh -NoProfile -File tools/reference/otvdm.ps1 stop >/dev/null
gone

python tools/reference/tracecmp.py $out/orig_trace.txt $out/port.log
echo "end frame: $(python tools/reference/screendiff.py $(ls $out/port/*.bmp | tail -1) $out/end.png 2>/dev/null)"
