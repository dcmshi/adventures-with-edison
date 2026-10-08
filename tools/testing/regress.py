"""Room 1 regression against the original's reference shots
(build/scratch/orig_*, taken with otvdm.ps1 run; not in the repository).
Every frame compared must be exact (view and panel); the two whole screens
no worse than their known counts. Exits 1 otherwise or if a run hangs.
E=path tests another build."""
import glob
import re
import shutil
import sys

from PIL import Image

import cmp
from testlib import SCRATCH, edison, parallel, python, run_game, say

OUT = SCRATCH / "regress"


def drags(points, hold=80, step=1000, start=1000):
    """A press held `hold` ms at each (x, y) a second apart, then a move to
    (mx, my): the panel's buttons and aims as a person clicks them."""
    args, t = [], start
    for x, y, mx, my in points:
        args += ["--drag", t, x, y, x, y, hold, "--move", t + 300, mx, my]
        t += step
    return args


CASES = {
    "rest": ["--quit-after", 2000],
    "sl": ["--drag", 1000, 440, 390, 440, 390, 600, "--move", 1600, 445, 390,
           "--drag", 3000, 220, 305, 220, 305, 600, "--move", 3600, 225, 305,
           "--drag", 5000, 100, 305, 100, 305, 600, "--move", 5600, 105, 305, "--quit-after", 7000],
    "bt": drags([(200, 200, 205, 200), (330, 345, 335, 345), (330, 345, 330, 350),
                 (330, 345, 335, 345), (330, 345, 330, 350), (330, 345, 335, 345)]) + ["--quit-after", 7000],
    "aim": drags([(x, y, x + 4, y) for x, y in [(150, 150), (300, 120), (480, 160), (250, 260),
                                                (350, 200), (520, 230), (120, 240), (450, 270)]]) + ["--quit-after", 9500],
    "shot": ["--drag", 1000, 180, 250, 180, 250, 80, "--move", 1300, 184, 250,
             "--drag", 2000, 540, 340, 540, 340, 80, "--move", 2100, 544, 340, "--quit-after", 13000],
}
# The whole screen at the end against a shot of the original: known counts
# (the table's grid lines, the score, the left column's balls: random, as
# the original's, and moved by every draw, the music's too), failing only if
# they grow.
WHOLE = {"rest": (SCRATCH / "room1_ref.png", 514), "shot": (SCRATCH / "orig_shot" / "bend.png", 677)}
# Frames against the original's at the same times.
FRAMES = {
    "sl": ("orig_play4", ["2000:s1", "4000:s2"]),
    "bt": ("orig_play3", [f"{t}900:r{t}" for t in range(1, 7)]),
    "aim": ("orig_aim", [f"{t}900:a{t}" for t in range(1, 9)]),
    "shot": ("orig_shot", ["12800:bend"]),
}


def main():
    exe = edison(OUT)
    fail = False

    def one(name):
        d = OUT / name
        shutil.rmtree(d, ignore_errors=True)
        args = ["--game", "science", "--room", "1", "--capture", str(d), "100", *map(str, CASES[name])]
        return run_game(exe, args, d / "run.log")

    for name, run in parallel([(n, lambda n=n: one(n)) for n in CASES]):
        if run.hung:
            say(run.hung_report(name))
            fail = True
    for name, (ref, known) in WHOLE.items():
        last = sorted(glob.glob(str(OUT / name / "*.bmp")))[-1:]
        out = python("screendiff.py", last[0], ref) if last else ""
        m = re.search(r"(\d+) pixels differ", out)
        n = int(m.group(1)) if m else None
        say(f"{name} (whole screen): {n if n is not None else '?'} pixels differ (known: {known})")
        fail |= n is None or n > known
    for name, (orig, frames) in FRAMES.items():
        bad, same = [], 0
        for spec in frames:
            ms, q = spec.split(":")
            a = Image.open(cmp.frame(str(OUT / name), int(ms))).convert("RGB")
            b = Image.open(SCRATCH / orig / f"{q}.png").convert("RGB")
            v, p = cmp.count(a, b, cmp.VIEW), cmp.count(a, b, cmp.PANEL)
            if v[0] or p[0]:
                bad.append(f"    {q} view {v} panel {p}")
            else:
                same += 1
        say(f"{name}: {same} of {len(frames)} frames the same")
        for b in bad:
            say(b)
        fail |= bool(bad)
    say("regress:", "FAIL" if fail else "PASS")
    return 1 if fail else 0


if __name__ == "__main__":
    sys.exit(main())
