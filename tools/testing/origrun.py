"""Runs a room in the original under winevdm (WMAINSKP.EXE, the opening
skipped) with an otvdm.ps1 script once the room is up, traces the ball and
[FFE] (the room ticks) with memwatch.py, and stops it. Each step is
limited in time and says what it's doing.

Usage: origrun.py OUTDIR ROOM [--watch S] [--delay S] [--extra EXPR...] [SCRIPT LINE...]
  SCRIPT LINEs: otvdm.ps1 run lines after the room is up ("down 540 340",
  "wait 0.1", "shot name.png", ...).
  --watch S   trace for S seconds (default 10; 0: no trace)
  --delay S   start tracing S seconds after starting (default 16: the room up)
  --extra     more memwatch.py expressions (name=expr)
  --follow S  memwatch.py's --follow (tracing from before the room is built:
              its data segment moves)
Out: OUTDIR/run.txt (the script), run.log, ball.txt (cx cy cz vx vy vz, then
the extras; t the room tick: --keep-t keeps it), the script's shots.
Needs EDISON_RUN and OTVDM (see README.md); defaults D:/tools/edison-run
and D:/tools/otvdm/otvdm-v0.9.0/otvdmw.exe."""
import argparse
import os
import subprocess
import sys
import time
from pathlib import Path

from testlib import REFERENCE, ROOT, no_window, say

BALL = "[[5ffc+ae]+f77]"


def ps1(*args, limit=30, wait=True):
    cmd = ["pwsh", "-NoProfile", "-File", str(REFERENCE / "otvdm.ps1"), *map(str, args)]
    if not wait:
        return subprocess.Popen(cmd, cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                                creationflags=no_window())
    try:
        return subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, timeout=limit,
                              creationflags=no_window())
    except subprocess.TimeoutExpired:
        say(f"otvdm.ps1 {args[0]}: HUNG (killed after {limit} s)")
        return None


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("out")
    ap.add_argument("room")
    ap.add_argument("script", nargs="*")
    ap.add_argument("--watch", type=float, default=10)
    ap.add_argument("--delay", type=float, default=16)
    ap.add_argument("--extra", nargs="*", default=[])
    ap.add_argument("--keep-t", action="store_true")
    ap.add_argument("--follow", type=float, default=0)
    opts = ap.parse_args()
    os.environ.setdefault("EDISON_RUN", "D:/tools/edison-run")
    os.environ.setdefault("OTVDM", "D:/tools/otvdm/otvdm-v0.9.0/otvdmw.exe")
    out = Path(opts.out)
    out.mkdir(parents=True, exist_ok=True)
    exe = Path(os.environ["EDISON_RUN"]) / "WMAINSKP.EXE"
    script = out / "run.txt"
    # (The S key takes two digits.)
    script.write_text("wait 10\ntype S%02d\nwait 5\n%s\n" % (int(opts.room), "\n".join(opts.script)))
    # The script's own length: its waits, to limit the run.
    length = sum(float(l.split()[1]) for l in script.read_text().splitlines() if l.startswith("wait "))
    start = time.monotonic()
    say(f"origrun: room {opts.room}, script {length:.0f} s, trace {opts.watch:.0f} s from {opts.delay:.0f} s")
    ps1("stop")
    time.sleep(2)
    if ps1("start", exe) is None:
        return 1
    run = ps1("run", script, out, wait=False)
    trace = None
    if opts.watch > 0:
        time.sleep(opts.delay)
        say(f"origrun: tracing ({time.monotonic() - start:.0f} s)")
        exprs = [f"cx=[{BALL}]+2", f"cy=[{BALL}]+4", f"cz=[{BALL}]+6", f"vx=[{BALL}+2]+62",
                 f"vy=[{BALL}+2]+64", f"vz=[{BALL}+2]+66", "t=d:ffe", *opts.extra]
        trace = out / "ball_t.txt"
        try:
            subprocess.run([sys.executable, str(REFERENCE / "memwatch.py"), "watch", str(exe), "--every", "1",
                            "--for", str(opts.watch), "--out", str(trace),
                            *(["--follow", str(opts.follow)] if opts.follow else []), *exprs],
                           cwd=ROOT, capture_output=True, timeout=opts.watch + 30)
        except subprocess.TimeoutExpired:
            say(f"origrun: memwatch HUNG (killed after {opts.watch + 30:.0f} s)")
    limit = max(length + 30 - (time.monotonic() - start), 5)
    try:
        log, _ = run.communicate(timeout=limit)
    except subprocess.TimeoutExpired:
        run.kill()
        log, _ = run.communicate()
        say(f"origrun: the script HUNG (killed after {length + 30:.0f} s)")
    (out / "run.log").write_text(log or "")
    ps1("stop")
    if trace and trace.exists():
        lines = trace.read_text().splitlines()
        if not opts.keep_t:  # (t is the 8th column)
            lines = [" ".join(l.split()[:7] + l.split()[8:]) for l in lines]
        (out / "ball.txt").write_text("\n".join(lines) + "\n")
        say(f"origrun: {len(lines)} states traced")
    say(f"origrun: done ({time.monotonic() - start:.0f} s); {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
