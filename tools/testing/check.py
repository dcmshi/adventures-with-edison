"""All the checks, one after another, each timed and limited: the unit
tests (ctest), room 1 against the original's shots (regress.py), the shots
traced in the original (retrace.py), the scenarios against the original's
shots of their last --orig run (scenario.py: room 1's holes, the lessons,
the game over and won), and smoke tests of the testing switches and tools
(the aim search, the dialogs skipped, the heartbeat, deadscan.py's known
functions). PASS or FAIL for each; exits 1 if any failed. About 40 s.

Usage: check.py [unit] [regress] [retrace] [scenario] [smoke] [--jobs N]
  (default: all steps; --jobs N: the scenarios N at a time, 1 when the
  machine is busy, its limit stretched to match)"""
import math
import re
import subprocess
import sys
import time
from pathlib import Path

import aimsearch
from scenarios import SCENARIOS
from testlib import ROOT, SCRATCH, edison, run_game, say

HERE = Path(__file__).parent
OUT = SCRATCH / "check"


def command(cmd, limit, cwd=ROOT):
    """(passed, output) of a command; a hang is a failure."""
    try:
        r = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True, timeout=limit)
        return r.returncode == 0, r.stdout + r.stderr
    except subprocess.TimeoutExpired as e:
        return False, f"HUNG (killed after {limit} s)\n{e.stdout or ''}"


def smoke():
    lines, ok = [], True
    # The aim search finds room 22's suckhole (the shot retrace.py plays).
    try:
        aims = aimsearch.search("-6,-1,296,320,148,164,4", ["--game", "science", "--room", "22"],
                                jobs=2, limit=30)
        hit = any(a.startswith("aim 308,156 power 7: kind 6") for a in aims)
    except RuntimeError as e:
        aims, hit = [str(e)], False
    lines.append("aim search: ok" if hit else "aim search: room 22's suckhole not found at 308,156\n" + "\n".join(aims))
    ok &= hit
    exe = edison(OUT)
    # Room 96 greets with a box: unanswered, the heartbeat says it waits...
    run = run_game(exe, ["--game", "science", "--room", "96", "--quit-after", "5000"], OUT / "dialog.log", limit=20)
    text = run.log.read_text(errors="replace") if run.log.exists() else ""
    hit = "waiting in dialog (message 0040)" in text and not run.hung
    lines.append("heartbeat in a dialog: " + ("ok" if hit else "missing\n" + text[-300:]))
    ok &= hit
    # ...and with SCI_SKIPDIALOGS it's answered and the room ticks on.
    run = run_game(exe, ["--game", "science", "--room", "96", "--quit-after", "5000"], OUT / "skip.log",
                   env={"SCI_SKIPDIALOGS": "1"}, limit=20)
    text = run.log.read_text(errors="replace") if run.log.exists() else ""
    hit = "dialog (message 0040) skipped" in text and "room 96 tick" in text and not run.hung
    lines.append("dialogs skipped: " + ("ok" if hit else "the box wasn't answered or the room didn't tick\n" + text[-300:]))
    ok &= hit
    # deadscan.py's classes for functions read by hand (test_deadscan.py).
    hit, output = command([sys.executable, str(HERE / "test_deadscan.py")], 60)
    lines.append(output.strip())
    ok &= hit
    # ...and no function it finds dead that Ghidra sees called from live
    # code (ghidrarefs.py; needs ExportRefs.java's lists, else skipped).
    if all((ROOT / "extracted" / "ghidra" / f"{n}.refs.txt").exists() for n in ("mall", "winmain", "wmain")):
        hit, output = command([sys.executable, str(HERE / "ghidrarefs.py")], 120)
        lines.append(output.strip())
        ok &= hit
    else:
        lines.append("ghidrarefs: skipped (no extracted/ghidra/*.refs.txt: tools/ghidra/ExportRefs.java)")
    return ok, "\n".join(lines)


def scenario(jobs):
    """scenario.py, jobs at a time (0: all), about 60 s a round. (Each run
    has its own limit, scenario.py's play_port, the dense ones' minutes:
    this one only catches scenario.py itself stuck.)"""
    if not jobs:
        return command([sys.executable, str(HERE / "scenario.py")], 900)
    rounds = math.ceil(len(SCENARIOS) / jobs)
    return command([sys.executable, str(HERE / "scenario.py"), "--jobs", str(jobs)], max(900, 120 * rounds))


# The lines of a failed step's output worth showing: verdicts other than a
# pass, hangs, worse shots, errors, and the summaries.
NOTABLE = re.compile(r"FAIL|HUNG|WORSE|REPORT|no port frame|Traceback|Error|error|^scenarios: |^\s*\d+% tests passed|tests failed")


# A step's count, shown on its line: ctest's and scenario.py's summaries.
COUNT = re.compile(r"^\s*(\d+% tests passed.*|scenarios: .*)$", re.M)


def counted(output):
    m = COUNT.findall(output)
    return f"; {m[-1].strip()}" if m else ""


def notable(output, most=40):
    lines = output.strip().splitlines()
    picked = [l for l in lines if NOTABLE.search(l)]
    # A traceback's last line says what it was.
    if any("Traceback" in l for l in lines) and lines and lines[-1] not in picked:
        picked.append(lines[-1])
    return (picked or lines[-15:])[-most:]


JOBS = 0
STEPS = {
    "unit": lambda: command(["ctest", "--output-on-failure"], 120, cwd=ROOT / "build"),
    "regress": lambda: command([sys.executable, str(HERE / "regress.py")], 60),
    "retrace": lambda: command([sys.executable, str(HERE / "retrace.py")], 120),
    "scenario": lambda: scenario(JOBS),
    "smoke": smoke,
}


def main():
    global JOBS
    args = sys.argv[1:]
    if "--jobs" in args:
        i = args.index("--jobs")
        JOBS = int(args[i + 1])
        del args[i:i + 2]
    steps = args or list(STEPS)
    failed = []
    for name in steps:
        if name not in STEPS:
            say(f"{name}: no such step ({', '.join(STEPS)})")
            failed.append(name)
            continue
        start = time.monotonic()
        ok, output = STEPS[name]()
        OUT.mkdir(parents=True, exist_ok=True)
        log = OUT / f"{name}.log"
        log.write_text(output, encoding="utf-8", errors="replace")
        say(f"{name}: {'PASS' if ok else 'FAIL'} ({time.monotonic() - start:.0f} s{counted(output)})")
        if not ok:
            for line in notable(output):
                say("    " + line)
            say(f"    (all of it: {log.relative_to(ROOT)})")
            failed.append(name)
    say("check: PASS" if not failed else f"check: FAIL ({' '.join(failed)})")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
