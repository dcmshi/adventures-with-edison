"""Plays scenarios (tools/testing/scenarios.py: a table room, a timeline of
presses, drags, moves and typing, the moments to take shots) in the port and
compares each of the original's shots with the port's nearest frame.

Usage: scenario.py [NAME ...] [--orig] [--accept] [--list] [--window MS] [--jobs N] [--orig-jobs N]
  NAME      scenarios to play (default: all; a prefix picks several: lesson)
  --orig    run the original first (under winevdm, one at a time, about a
            minute each), else the shots of its last run are used; both sides
            start from the original's save files (WSCIENCE.HS, wscience.edi:
            the port gets copies, the original's are put back after each run)
  --accept  this run's counts become the known ones
  --list    the scenarios
  --window  how far (ms) from a shot's time to look for the port's frame
            (default 1500: the two sides' timelines start a little apart)
  --jobs    how many port runs at once (default: all; 1 when the machine
            is busy, as runs starved of time hang or are killed)
  --orig-jobs  how many runs of the original at once with --orig
            (default 1; workers.py: each its own game folder and tile)
Each shot is matched to the port frame with the fewest differing pixels
(tolerance 24, the scenario's masks left out) within the window (frames
every 10 ms within 1 s of a scenario's "dense" shots), and fails
when that is more than its known count. A scenario's first run (no known
counts) only reports.

Out: build/scratch/scenario/NAME/port/ (frames every 100 ms), orig/ (the
shots), known.json beside them. Local only, as regress.py: the original's
shots aren't in the repository. Needs EDISON_RUN and OTVDM for --orig (see
README.md).
"""
import argparse
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw

from scenarios import SCENARIOS, VKEYS
from testlib import ROOT, SCRATCH, edison, parallel, run_game, say
import workers as W

OUT = SCRATCH / "scenario"
KNOWN = OUT / "known.json"
SAVES = ("WSCIENCE.HS", "wscience.edi")


def run_folder():
    return Path(os.environ.setdefault("EDISON_RUN", "D:/tools/edison-run"))


# --- the port --------------------------------------------------------------------

def port_args(s):
    args = []
    # Shots of something moving (Edison's walks, his blink): a frame every
    # 10 ms within 1 s of them, as 100 ms can fall between the original's.
    times = dict((n, t) for t, n in s["shots"])
    for n in s.get("dense", ()):
        args += ["--capture-dense", int((times[n] - 1) * 1000), int((times[n] + 1) * 1000), 10]
    for e in s["events"]:
        ms, kind = int(e[0] * 1000), e[1]
        if kind == "click":
            args += ["--drag", ms, e[2], e[3], e[2], e[3], 80]
        elif kind == "drag":
            args += ["--drag", ms, e[2], e[3], e[4], e[5], int(e[6] * 1000)]
        elif kind == "move":
            args += ["--move", ms, e[2], e[3]]
        elif kind == "type":
            args += ["--type", ms, e[2]]
        elif kind == "key":
            args += ["--key", ms, e[2], int(e[3] * 1000)]
    return [str(a) for a in args]


def play_port(exe, name, s):
    d = OUT / name / "port"
    shutil.rmtree(d, ignore_errors=True)
    save = d / "save"
    save.mkdir(parents=True)
    # The original's save files, so both sides start the same.
    for f in SAVES:
        src = run_folder() / f
        if src.exists():
            shutil.copy2(src, save / f.lower())
    args = ["--game", "science", "--room", str(s["room"]), "--save", str(save), "--capture", str(d), "100",
            *port_args(s), "--quit-after", str(int((s["length"] + 1) * 1000))]
    # Its time: a frame saved every 100 ms (every 10 ms around the dense
    # shots) makes a run slower than its virtual clock when others share the
    # machine, so three times its length and a minute (testlib's length + 20
    # killed the lessons and hole-level1 at three jobs), and a quarter of a
    # second for each dense frame (with every scenario at once the hole runs
    # took over 100 s for 4 s of dense frames).
    times = dict((n, t) for t, n in s["shots"])
    dense = {ms for n in s.get("dense", ()) for ms in range(int((times[n] - 1) * 100), int((times[n] + 1) * 100))}
    limit = 3 * (s["length"] + 1) + 60 + len(dense) // 4
    return run_game(exe, args, d / "run.log", env=s.get("env"), limit=limit)


# --- the original ----------------------------------------------------------------

def orig_script(s):
    """otvdm.ps1 lines: the events and shots in time order."""
    timeline = [(e[0], e) for e in s["events"]] + [(t, ("shot", n)) for t, n in s["shots"]]
    lines, now = [], 0.0
    for at, e in sorted(timeline, key=lambda x: x[0]):
        if at > now:
            lines.append(f"wait {at - now:.2f}")
            now = at
        if e[0] == "shot":
            lines.append(f"shot {e[1]}.png")
        elif e[1] == "click":
            lines += [f"down {e[2]} {e[3]}", "wait 0.08", f"up {e[2]} {e[3]}"]
            now += 0.08
        elif e[1] == "drag":
            lines += [f"down {e[2]} {e[3]}", f"move {e[4]} {e[5]}", f"wait {e[6]:.2f}", f"up {e[4]} {e[5]}"]
            now += e[6]
        elif e[1] == "move":
            lines.append(f"move {e[2]} {e[3]}")
        elif e[1] == "type":
            lines.append(f"type {e[2]}")
        elif e[1] == "key":
            lines += [f"keydown {VKEYS[e[2]]}", f"wait {e[3]:.2f}", f"keyup {VKEYS[e[2]]}"]
            now += e[3]
    return lines + ["wait 2"]


def play_orig(name, s, env=None):
    """origrun.py plays the scenario; ENV a worker's (workers.py: its own
    game folder), else this process's."""
    d = OUT / name / "orig"
    shutil.rmtree(d, ignore_errors=True)
    run = Path(env["EDISON_RUN"]) if env else run_folder()
    backup = OUT / f"saves{env['OTVDM_WORKER'] if env else ''}"
    backup.mkdir(parents=True, exist_ok=True)
    kept = []
    for f in SAVES:
        if (run / f).exists():
            shutil.copy2(run / f, backup / f)
            kept.append(f)
    try:
        subprocess.run([sys.executable, str(Path(__file__).parent / "origrun.py"), str(d), str(s["room"]),
                        "--watch", "0", "--", *orig_script(s)],
                       cwd=Path(__file__).parent, timeout=s["length"] + 120, env=env,
                       capture_output=env is not None)
    except subprocess.TimeoutExpired:
        say(f"{name}: the original HUNG")
    finally:
        # The game writes its high scores and look: put the files back.
        for f in kept:
            shutil.copy2(backup / f, run / f)


# --- comparing -------------------------------------------------------------------

def masked(img, masks):
    img = img.convert("RGB")
    if masks:
        draw = ImageDraw.Draw(img)
        for x0, y0, x1, y1 in masks:
            draw.rectangle([x0, y0, x1 - 1, y1 - 1], fill=(0, 0, 0))
    return img


def differing(a, b):
    d = ImageChops.difference(a, b).convert("L").point(lambda v: 255 if v > 24 else 0)
    return d.histogram()[255], d.getbbox()


def compare(name, s, window):
    """{shot: (count, frame, box)}: None without a port frame in the window,
    False without a shot of the original."""
    port, orig = OUT / name / "port", OUT / name / "orig"
    frames = sorted(port.glob("*.bmp"))
    out = {}
    for t, shot in s["shots"]:
        o = orig / f"{shot}.png"
        if not o.exists():
            out[shot] = False
            continue
        b = masked(Image.open(o), s["masks"])
        best = None
        for f in frames:
            if abs(int(f.name[:5]) - int(t * 1000)) > window:
                continue
            n, box = differing(masked(Image.open(f), s["masks"]), b)
            if best is None or n < best[0]:
                best = (n, f.name, box)
        out[shot] = best
    return out


def summary(verdicts):
    """The last line: "scenarios: 12 PASS, 1 FAIL (game-won), 2 HUNG (...)"."""
    parts = []
    for v in ("PASS", "FAIL", "HUNG", "REPORT"):
        names = [n for n, w in verdicts.items() if w == v]
        if names:
            parts.append(f"{len(names)} {v}" + (f" ({', '.join(names)})" if v != "PASS" else ""))
    return "scenarios: " + (", ".join(parts) or "none run")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("names", nargs="*")
    ap.add_argument("--orig", action="store_true")
    ap.add_argument("--accept", action="store_true")
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--window", type=int, default=1500)
    ap.add_argument("--jobs", type=int, default=0)
    ap.add_argument("--orig-jobs", type=int, default=1)
    opts = ap.parse_args()
    if opts.list:
        for n, s in SCENARIOS.items():
            say(f"{n:12} room {s['room']:3}, {s['length']:4.0f} s, shots {' '.join(x for _, x in s['shots'])}")
        return 0
    names = [n for n in SCENARIOS if not opts.names or any(n.startswith(p) for p in opts.names)]
    if not names:
        say(f"no scenario matches {' '.join(opts.names)} (--list)")
        return 2
    OUT.mkdir(parents=True, exist_ok=True)
    known = json.loads(KNOWN.read_text()) if KNOWN.exists() else {}

    if opts.orig:
        os.environ.setdefault("OTVDM", "D:/tools/otvdm/otvdm-v0.9.0/otvdmw.exe")
        if opts.orig_jobs > 1:
            # Side by side (workers.py), each in its own copy of the game folder.
            for n, k, _ in W.deal(names, opts.orig_jobs, lambda n, k, env: play_orig(n, SCENARIOS[n], env)):
                say(f"{n}: the original (worker {k})")
        else:
            for n in names:
                say(f"{n}: the original")
                play_orig(n, SCENARIOS[n])
    exe = edison(OUT)
    fail = False
    verdicts = {}
    jobs = [(n, lambda n=n: play_port(exe, n, SCENARIOS[n])) for n in names]
    hung = set()
    for n, run in parallel(jobs, opts.jobs or None):
        if run.hung:
            say(run.hung_report(n))
            hung.add(n)
            verdicts[n] = "HUNG"
            fail = True
    for n in names:
        if n in hung:
            continue  # (its last frame may be half written)
        result = compare(n, SCENARIOS[n], opts.window)
        mine = known.get(n, {})
        lines, bad = [], False
        for shot, r in result.items():
            if r is False:
                lines.append(f"    {shot}: no shot of the original (--orig)")
                continue
            if r is None:
                lines.append(f"    {shot}: no port frame within the window")
                bad = True
                continue
            count, frame, box = r
            k = mine.get(shot)
            worse = k is not None and count > k
            bad |= worse
            note = f"known {k}" if k is not None else "no known count"
            lines.append(f"    {shot}: {count} pixels differ ({note}){' WORSE' if worse else ''}, frame {frame}, box {box}")
            if opts.accept:
                mine[shot] = count
        if opts.accept:
            known[n] = mine
        verdict = "FAIL" if bad else ("PASS" if mine else "REPORT")
        verdicts[n] = verdict
        say(f"{n}: {verdict}")
        for line in lines:
            say(line)
        fail |= bad
    say(summary(verdicts))
    if opts.accept:
        KNOWN.write_text(json.dumps(known, indent=1, sort_keys=True) + "\n")
        say(f"known counts kept in {KNOWN.relative_to(ROOT)}")
    return 1 if fail and not opts.accept else 0


if __name__ == "__main__":
    sys.exit(main())
