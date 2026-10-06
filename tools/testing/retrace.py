"""Replays shots in the port and compares them with traces of the original
kept from earlier checks (build/scratch/..., not in the repository; each
made with memwatch.py, see README.md). A case whose trace is missing is
skipped. Each result is checked against retrace.expected (what the port
matched when last accepted); exits 1 if one differs or hangs.

Usage: retrace.py [--accept] [NAME...]
  --accept  write the results run into retrace.expected, once checked.
E=path tests another build."""
import argparse
import sys
from pathlib import Path

from testlib import SCRATCH, edison, parallel, python, run_game, say

OUT = SCRATCH / "retrace"
EXPECTED = Path(__file__).with_name("retrace.expected")

N = r"(-?\d+)"
G = ",".join([N] * 6)
# The port's log lines tracecmp.py reads (default: the ball's c and v).
PATTERNS = {
    "rem": rf" c {N},{N},{N} v {N},{N},{N} r {N},{N},{N}",  # with the remainders
    "bodies": " " + " ".join([G] * 4),                        # the ball and three loose magnets
    "rack": rf"^bodies t\d+ " + " ".join([G] * 7),            # the ball and six other balls
}


def shot(at, x, y):
    """Aim at (x, y) at `at` ms (a press, then the mouse 4 to the right)."""
    return f"--drag {at} {x} {y} {x} {y} 80 --move {at + 180} {x + 4} {y}"


def fire(at):
    """The shoot button at `at` ms."""
    return f"--drag {at} 540 340 540 340 80 --move {at + 130} 544 340"


def ball_type(at):
    """The ball-type button once at `at` ms (the next type)."""
    return f"--drag {at} 330 345 330 345 80 --move {at + 300} 335 345"


def power(at):
    """The power slider one up at `at` ms."""
    return f"--drag {at} 334 330 334 330 80 --move {at + 150} 330 300"


# name: (original trace, pattern (None: c and v), port arguments)
CASES = {
    "mode0": ("m1/p0/o2.txt", "rem", f"--room 35 --click 1000 330 225 --click 2500 330 225 --drag 12000 208 266 208 266 80 --move 12180 212 266 {fire(13000)} --quit-after 22000"),
    "mode1back": ("m1/o101/trace.txt", None, f"--room 2 {shot(2000, 400, 176)} {fire(3000)} --click 7000 316 224 --click 7500 316 224 --click 8000 316 224 --quit-after 13000"),
    "mode1left": ("m1/o100/trace.txt", None, f"--room 2 {shot(2000, 156, 212)} {fire(3000)} --click 7000 316 224 --click 7500 316 224 --click 8000 316 224 --quit-after 13000"),
    "lipsStone": ("t7/olips/trace6.txt", None, f"--room 2 {shot(2000, 122, 236)} {fire(3000)} --quit-after 6000"),
    "magnets3": ("mag/o3/trace.txt", "rem", "--room 3 --quit-after 14000"),
    "magnets13": ("mag/o13/trace.txt", "bodies", "--room 13 --quit-after 14000"),
    "magnets33": ("mag/o33/trace.txt", "rem", f"--room 33 --drag 3000 334 330 334 330 80 --move 3150 330 300 {shot(4000, 410, 170)} {fire(5000)} --quit-after 14000"),
    "lever61": ("sw/o61/trace.txt", None, "--room 61 --click 2000 349 78 --click 8200 349 78 --quit-after 13000"),
    "levers62": ("sw/o62/trace.txt", None, "--room 62 --quit-after 13000"),
    "bullseye29": ("sw/o29/trace.txt", None, f"--room 29 {shot(2000, 270, 216)} {fire(3000)} --quit-after 11000"),
    "emCatch25": ("em/c25/ball.txt", None, f"--room 25 {shot(5000, 400, 140)} {fire(6600)} --quit-after 9000"),
    "emGlass25": ("em/c25r/ball.txt", None, f"--room 25 {ball_type(1000)} {shot(5000, 400, 140)} {fire(8300)} --quit-after 10500"),
    "emRubber25": ("em/c25z/ball.txt", None, f"--room 25 {ball_type(1000)} {ball_type(1600)} {ball_type(2200)} {ball_type(2800)} {shot(5000, 400, 140)} {fire(8300)} --quit-after 10500"),
    "fan98": ("fan/o98/ball.txt", None, f"--room 98 {shot(2000, 460, 80)} {fire(3000)} --quit-after 7000"),
    "fanIce25": ("fan/o25i/ball.txt", None, f"--room 25 {ball_type(1000)} {ball_type(1600)} {shot(3000, 200, 150)} {fire(4000)} --quit-after 8000"),
    "suckhole22": ("suck/o22/ball.txt", None, f"--room 22 {shot(2000, 308, 156)} {fire(3000)} --quit-after 16000"),
    "smileyRubber46": ("t11/r46/ball.txt", None, f"--room 46 {shot(2000, 300, 130)} {fire(3000)} --quit-after 10000"),
    "smileyGlass46": ("t11/g46/ball.txt", None, f"--room 46 {ball_type(1000)} {ball_type(1600)} {shot(3000, 300, 125)} {fire(4000)} --quit-after 11000"),
    "rack71": ("t0/o71/ball.txt", "rack", f"--room 71 {shot(2000, 400, 150)} {fire(3000)} --quit-after 12000"),
    "lipsIce": ("t7/olips0/trace.txt", None, f"--room 2 {power(4000)} {power(4600)} {power(5200)} {power(5800)} {shot(6400, 122, 236)} {fire(7400)} --quit-after 20000"),
}


def read_expected():
    out = {}
    if EXPECTED.exists():
        for line in EXPECTED.read_text().splitlines():
            name, _, result = line.partition(": ")
            out[name] = result
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--accept", action="store_true")
    ap.add_argument("names", nargs="*")
    opts = ap.parse_args()
    unknown = [n for n in opts.names if n not in CASES]
    if unknown:
        say("no such case:", *unknown)
        return 1
    expected = read_expected()
    exe = edison(OUT)

    def one(name):
        trace, pattern, args = CASES[name]
        log = OUT / f"{name}.log"
        run = run_game(exe, ["--game", "science", *args.split()], log, env={"SCI_DEBUG": "1"})
        if run.hung:
            return run.hung_report(name), "HUNG"
        extra = ["--port-pattern", PATTERNS[pattern]] if pattern else []
        lines = python("tracecmp.py", SCRATCH / trace, log, *extra).strip().splitlines()
        return None, lines[-1] if lines else "(no result)"

    jobs = []
    for name in opts.names or CASES:
        if not (SCRATCH / CASES[name][0]).exists():
            say(f"{name}: no trace (build/scratch/{CASES[name][0]})")
            continue
        jobs.append((name, lambda n=name: one(n)))
    say(f"({len(jobs)} cases running)")
    results, differ = {}, 0
    for name, (report, result) in parallel(jobs):
        results[name] = result
        want = expected.get(name)
        mark = "ok" if result == want else f"DIFFERS (expected: {want or 'none'})"
        differ += result != want
        say(report or f"{name}: {result} {mark}")
    if opts.accept:
        expected.update({n: r for n, r in results.items() if r != "HUNG"})
        EXPECTED.write_text("".join(f"{n}: {r}\n" for n, r in sorted(expected.items())))
        say(f"retrace: results written to {EXPECTED.name}")
        return 0
    say(f"retrace: PASS ({len(jobs)} cases)" if not differ else f"retrace: FAIL ({differ} of {len(jobs)} cases differ)")
    return 1 if differ else 0


if __name__ == "__main__":
    sys.exit(main())
