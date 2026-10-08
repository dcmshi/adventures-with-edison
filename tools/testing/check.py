"""All the checks, one after another, each timed and limited: the unit
tests (ctest), room 1 against the original's shots (regress.py), the shots
traced in the original (retrace.py), the scenarios against the original's
shots of their last --orig run (scenario.py: room 1's holes, the lessons,
the game over and won), and smoke tests of the testing switches (the aim
search, the dialogs skipped, the heartbeat). PASS or FAIL for each; exits 1
if any failed. About 40 s.

Usage: check.py [unit] [regress] [retrace] [scenario] [smoke] (default: all)"""
import subprocess
import sys
import time
from pathlib import Path

import aimsearch
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
    return ok, "\n".join(lines)


STEPS = {
    "unit": lambda: command(["ctest", "--output-on-failure"], 120, cwd=ROOT / "build"),
    "regress": lambda: command([sys.executable, str(HERE / "regress.py")], 60),
    "retrace": lambda: command([sys.executable, str(HERE / "retrace.py")], 120),
    "scenario": lambda: command([sys.executable, str(HERE / "scenario.py")], 180),
    "smoke": smoke,
}


def main():
    steps = sys.argv[1:] or list(STEPS)
    failed = []
    for name in steps:
        if name not in STEPS:
            say(f"{name}: no such step ({', '.join(STEPS)})")
            failed.append(name)
            continue
        start = time.monotonic()
        ok, output = STEPS[name]()
        say(f"{name}: {'PASS' if ok else 'FAIL'} ({time.monotonic() - start:.0f} s)")
        if not ok:
            for line in output.strip().splitlines()[-15:]:
                say("    " + line)
            failed.append(name)
    say("check: PASS" if not failed else f"check: FAIL ({' '.join(failed)})")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
