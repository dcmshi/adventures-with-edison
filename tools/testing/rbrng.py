"""Rock and Bach's random numbers against the original's: a scenario of
rbcompare.py played in both, rand()'s seed (DS:3088, f36_0ec2: Microsoft
C's, never seeded, so 1 at the start) followed in the original by
memwatch.py and logged by the port (EDISON_RNGLOG), each seed placed in
the generator's one sequence by stepping it.

  python tools/testing/rbrng.py NAME ... [--compare-only]

The studio's sign draws once a flash (3 a second), so the count of draws
at a moment is the clock's; what's compared is their pattern: the draws
between one seen state and the next, in bursts (the colour chips' 24 at
once, a flash's one), each side's sizes in order. Out:
build/scratch/rbrng/NAME/ (watch.txt, rng.log). Needs what rbcompare.py
does.
"""
import argparse
import os
import shutil
import subprocess
import sys
import threading
import time

import rbcompare
from testlib import REFERENCE, SCRATCH, say

OUT = SCRATCH / "rbrng"
WORDS = ["u:3088", "u:308a"]
LIMIT = 200_000  # steps searched for a seed


def step(s):
    return (s * 0x343FD + 0x269EC3) & 0xFFFFFFFF


def place(seeds):
    """Each seed's draw count from 1 (None past LIMIT)."""
    left, found, s = set(seeds), {}, 1
    if 1 in left:
        found[1] = 0
    for n in range(1, LIMIT + 1):
        s = step(s)
        if s in left:
            found[s] = n
        if len(found) == len(left):
            break
    return found


def port_draws(log):
    """(ms, draw count) for each of the port's draws."""
    rows = [line.split() for line in log.read_text().splitlines()]
    seeds = [int(r[1], 16) for r in rows if len(r) == 2]
    pos = place(seeds)
    return [(int(r[0]), pos.get(int(r[1], 16))) for r in rows if len(r) == 2]


def orig_draws(watch):
    """(ms, draw count) for each seed memwatch saw."""
    rows = []
    for line in watch.read_text().splitlines():
        p = line.split()
        if len(p) == 3 and p[0].isdigit():
            rows.append((int(p[0]), int(p[1]) & 0xFFFF | (int(p[2]) & 0xFFFF) << 16))
    pos = place([s for _, s in rows])
    out = []
    for ms, s in rows:
        n = pos.get(s)
        if not out or out[-1][1] != n:
            out.append((ms, n))
    return out


def bursts(draws, gap=30):
    """Draws within GAP ms of each other as one burst: its size."""
    out, last_ms, start = [], None, None
    prev = 0
    for ms, n in draws:
        if n is None:
            continue
        if last_ms is not None and ms - last_ms > gap:
            out.append(prev - start)
            start = prev
        if start is None:
            start = 0
        last_ms, prev = ms, n
    if start is not None:
        out.append(prev - start)
    return out


def check(name):
    d = OUT / name
    port, orig = port_draws(d / "rng.log"), orig_draws(d / "watch.txt")
    lost = sum(n is None for _, n in orig) + sum(n is None for _, n in port)
    pb, ob = bursts(port), bursts(orig)
    say(f"{name}: port {port[-1][1] if port else 0} draws, original {orig[-1][1] if orig else 0}; "
        f"{lost} seeds not in the sequence")
    say(f"  port bursts:     {pb}")
    say(f"  original bursts: {ob}")


def watch_orig(d, seconds, delay):
    exe = str(rbcompare.run_folder() / "WINMSKIP.EXE")
    tool = [sys.executable, str(REFERENCE / "memwatch.py")]

    def run():
        # Once the activity is up: WINMSKIP's data segment moves as it
        # loads, and in the library again some seconds in (memwatch keeps
        # the copy it found). A draw before is still placed from seed 1.
        time.sleep(rbcompare.START + delay)
        for _ in range(10):  # until memwatch finds it
            w = subprocess.run(tool + ["watch", exe, "--every", "1", "--for", str(seconds), "--out",
                                       str(d / "watch.txt"), *WORDS], capture_output=True, text=True)
            (d / "memwatch.txt").write_text(w.stdout + w.stderr)
            if w.returncode == 0:
                return
            time.sleep(1)

    t = threading.Thread(target=run, daemon=True)
    t.start()
    return t


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("names", nargs="+")
    ap.add_argument("--compare-only", action="store_true")
    ap.add_argument("--delay", type=float, default=0.5, help="seconds into the activity to start the watch")
    opts = ap.parse_args()
    for name in opts.names:
        s = rbcompare.SCENARIOS[name]
        d = OUT / name
        if not opts.compare_only:
            d.mkdir(parents=True, exist_ok=True)
            log = d / "rng.log"
            log.unlink(missing_ok=True)
            os.environ["EDISON_RNGLOG"] = str(log)
            try:
                rbcompare.play_port(name, s, 1.0)
            finally:
                del os.environ["EDISON_RNGLOG"]
            end = max([t for t, _ in s["shots"]] + [e[0] for e in s["events"]])
            (d / "watch.txt").unlink(missing_ok=True)
            w = watch_orig(d, end + 2 - opts.delay, opts.delay)
            rbcompare.play_orig(name, s)
            w.join(timeout=15)
        check(name)
    return 0


if __name__ == "__main__":
    sys.exit(main())
