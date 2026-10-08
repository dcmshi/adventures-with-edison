"""Mystery at the Museums against the original: a timeline of clicks, holds
and keys from "Please pick a level", played in the port (EDISON_SKIP:
setup starts at the level pick as the player SKIP; EDISON_SQUARE: every
square plays one puzzle) and in the original under winevdm (MALLSKIP.EXE,
tools/reference/mall_skip.py, the same), each of the original's shots
against the port's closest frame near its time.

  python tools/testing/mmcompare.py [NAME ...] [--port-only | --compare-only] [--list]

Both draw the same random numbers (segment 46's generator is never
seeded: docs/MYSTERY.md), so the boards, the puzzles and their layouts are
the same. Time 0 is the level's click. Shots and diffs (the pixels that
differ in magenta) go to build/scratch/mmcompare/NAME/. Needs EDISON_RUN
and OTVDM, and the CD mounted (the speech WAVs are on it). The original's
MYSTERY.HS and MEDISON.COL are put back after each run, SKIP.INF removed.
"""
import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

from PIL import Image, ImageChops

from testlib import REFERENCE, ROOT, SCRATCH, edison

OUT = SCRATCH / "mmcompare"
START = 13.0  # seconds from MALLSKIP.EXE's start to its level pick, its speech over
LEAD = 11.0   # the same in the port (a click during the speech only cuts it short)
SAVES = ("MYSTERY.HS", "MEDISON.COL")

# "Please pick a level": levels 6-13 (0-7) in a 4 x 2 grid.
LEVELS = [(42 + 43 * (i % 4), 209 + 17 * (i // 4)) for i in range(8)]
ARROW = (327, 371)  # the Director's letter's arrow (DS:0ECA)


def click(t, x, y):
    return (t, "click", x, y)


def hold(t, x, y, seconds=0.08):
    return (t, "hold", x, y, seconds)


def key(t, text):
    return (t, "type", text)


def office(level=0):
    """The level picked at 0, then the Director's letter read (six presses
    of its arrow, held 80 ms as a click: the last is still held, as the
    game sees it, when the objects are up, which skips their wait): the
    office's map is up by about 30 s."""
    return [click(0, *LEVELS[level])] + [hold(11 + 1.5 * i, *ARROW) for i in range(6)]


DOOR = (268, 309)  # the office's door to the Museum (DS:0E76: 210-326, 284-334)

SCENARIOS = {
    "office": dict(events=office(0), shots=[(-0.01, "pick"), (1, "letsdoit"), (6, "office"), (10, "letter"), (13, "page2"),
                                            (20, "objects"), (26, "goodluck"), (32, "map")]),
    "clock": dict(events=office(0), shots=[(18 + 0.5 * i, f"c{i:02d}") for i in range(21)]),
    "floor": dict(events=office(0) + [click(34, *DOOR)], shots=[(33.9, "map"), (35, "door"), (37, "floor1"),
                                                               (40, "floor2"), (44, "floor3")]),
}


def run_folder():
    if not os.environ.get("EDISON_RUN"):
        sys.exit("set EDISON_RUN to the folder with the game files")
    return Path(os.environ["EDISON_RUN"])


def play_orig(name, s):
    """MALLSKIP.EXE written for the scenario, then otvdm.ps1 runs it and
    the timeline; the save files put back after."""
    d = OUT / name / "orig"
    shutil.rmtree(d, ignore_errors=True)
    d.mkdir(parents=True)
    run = run_folder()
    skip = [sys.executable, str(REFERENCE / "mall_skip.py")]
    if "puzzle" in s:
        skip += ["--puzzle", str(s["puzzle"])]
    if "difficulty" in s:
        skip += ["--difficulty", str(s["difficulty"])]
    subprocess.run(skip, check=True, stdout=subprocess.DEVNULL)
    lines = [f"wait {START}"]
    timeline = [(e[0], e) for e in s["events"]] + [(t, ("shot", n)) for t, n in s["shots"]]
    now = 0.0
    for at, e in sorted(timeline, key=lambda p: p[0]):
        if at > now:
            lines.append(f"wait {at - now:.2f}")
            now = at
        if e[0] == "shot":
            lines.append(f"shot {e[1]}.png")
        elif e[1] == "click":
            lines.append(f"click {e[2]} {e[3]}")
        elif e[1] == "hold":
            lines += [f"down {e[2]} {e[3]}", f"wait {e[4]}", f"up {e[2]} {e[3]}"]
            now += e[4]
        elif e[1] == "type":
            lines.append(f"type {e[2]}")
    (d / "script.txt").write_text("\n".join(lines) + "\n")
    kept = {f: (run / f).read_bytes() for f in SAVES if (run / f).exists()}
    try:
        subprocess.run(["pwsh", "-NoProfile", "-File", str(REFERENCE / "otvdm.ps1"), "play", "MALLSKIP.EXE",
                        str(d / "script.txt"), str(d)], check=True)
    finally:
        for f, data in kept.items():
            (run / f).write_bytes(data)
        (run / "SKIP.INF").unlink(missing_ok=True)


def play_port(name, s):
    d = OUT / name / "port"
    shutil.rmtree(d, ignore_errors=True)
    exe = edison(d)
    save = d / "save"
    save.mkdir()
    for f in SAVES:  # the original's high scores and Edison's colours
        if (run_folder() / f).exists():
            shutil.copy2(run_folder() / f, save / f)
    args = []
    for e in s["events"]:
        ms = str(int((e[0] + LEAD) * 1000))
        if e[1] == "click":
            args += ["--click", ms, str(e[2]), str(e[3])]
        elif e[1] == "hold":
            args += ["--drag", ms, str(e[2]), str(e[3]), str(e[2]), str(e[3]), str(int(e[4] * 1000))]
        elif e[1] == "type":
            args += ["--type", ms, e[2]]
    end = max([t for t, _ in s["shots"]] + [e[0] for e in s["events"]]) + LEAD + 2
    env = dict(os.environ, EDISON_SKIP="1")
    if "puzzle" in s:
        env["EDISON_SQUARE"] = f"{s['puzzle']},{s['difficulty']}" if "difficulty" in s else str(s["puzzle"])
    with open(d / "run.log", "w") as log:
        subprocess.run([str(exe), str(ROOT / "original" / "cd" / "DSK3"), "--game", "mystery", "--hidden",
                        "--virtual-clock", "--save", str(save), "--capture", str(d), "100", *args,
                        "--quit-after", str(int(end * 1000))],
                       cwd=d, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=end + 60)


def compare(name, s, window):
    """Each original shot against the port's frames within the window of
    its time: the fewest pixels that differ."""
    d = OUT / name
    frames = sorted((int(p.stem), p) for p in (d / "port").glob("*.bmp"))
    for t, n in s["shots"]:
        o = Image.open(d / "orig" / f"{n}.png").convert("RGB")
        best = None
        for ms, p in frames:
            if abs(ms / 1000 - (t + LEAD)) > window:
                continue
            f = Image.open(p).convert("RGB")
            mask = ImageChops.difference(f, o).convert("L").point(lambda v: 255 if v else 0)
            c = mask.histogram()[255]
            if best is None or c < best[0]:
                best = (c, ms, mask, f)
        if best is None:
            print(f"{name} {n}: no port frame near {t + LEAD:.1f} s")
            continue
        c, ms, mask, f = best
        where = ""
        if c:
            where = f" in {mask.getbbox()}"
            f.paste((255, 0, 255), mask=mask)
            f.save(d / f"diff-{n}.png")
        print(f"{name} {n}: {c} pixels (port {ms / 1000 - LEAD:.1f} s){where}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("names", nargs="*")
    ap.add_argument("--port-only", action="store_true", help="play the port again, keep the original's shots")
    ap.add_argument("--compare-only", action="store_true", help="compare the last runs' frames")
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--window", type=float, default=1.5, help="seconds around each shot to search the port's frames")
    a = ap.parse_args()
    if a.list:
        for n, s in SCENARIOS.items():
            print(n, f"({len(s['shots'])} shots)")
        return
    for name in a.names or list(SCENARIOS):
        s = SCENARIOS[name]
        if not a.compare_only:
            play_port(name, s)
            if not a.port_only:
                play_orig(name, s)
        compare(name, s, a.window)


if __name__ == "__main__":
    main()
