"""Mystery at the Museums against the original: a timeline of clicks, holds
and keys from "Please pick a level", played in the port (EDISON_SKIP:
setup starts at the level pick as the player SKIP; EDISON_SQUARE: every
square plays one puzzle; EDISON_FLOOR, a scenario's floor=True: the
first visit to the office goes into the Museum) and in the original under
winevdm (MALLSKIP.EXE, tools/reference/mall_skip.py, the same), each of
the original's shots against the port's closest frame near its time.

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
    "skipfloor": dict(floor=True, events=[click(0, *LEVELS[0])],
                      shots=[(5, "office"), (7, "in"), (9, "floor"), (12, "floor2")]),
    "clock": dict(events=office(0), shots=[(18 + 0.5 * i, f"c{i:02d}") for i in range(21)]),
    "floor": dict(events=office(0) + [click(34, *DOOR)], shots=[(33.9, "map"), (35, "door"), (37, "floor1"),
                                                               (40, "floor2"), (44, "floor3")]),
}


# The puzzles (DS:1A9C) and their numbers of levels (DS:197C): difficulty
# 0 to the count - 1.
PUZZLES = ["Folded Cube", "Liberty Planetarium", "3D Ball Sculpture", "Binary Lights", "Question and Answer Period",
           "Dropping Squares", "Codes", "Concentration", "Circuit Analyzer", "Stackup", "Slide Puzzle",
           "Color Transformation", "Switch Puzzle", "Arrow Puzzle", "What Comes Next", "The Dig"]
LEVEL_COUNTS = [8, 3, 6, 4, 1, 4, 2, 9, 8, 3, 5, 9, 7, 3, 6, 8]
BUILDING = (95, 75)  # square 0's building on the floor (f10_0328's first)


def puzzle_start(p, d):
    """Into the Museum (floor=True), then square 0's building, which plays
    puzzle P at difficulty D: its first screens."""
    return dict(floor=True, puzzle=p, difficulty=d, events=[click(0, *LEVELS[0]), click(9, *BUILDING)],
                shots=[(8.9, "floor"), (10, "s10"), (12, "s12"), (15, "s15"), (20, "s20")])


SCENARIOS["cube_long"] = dict(puzzle=0, difficulty=0, events=office(0) + [click(34, *DOOR), click(43, *BUILDING)],
                              shots=[(42.9, "floor"), (46, "s46")])

for _p, _n in enumerate(LEVEL_COUNTS):
    for _d in sorted({0, _n - 1}):
        SCENARIOS[f"p{_p:02d}d{_d}"] = puzzle_start(_p, _d)


# Playing the puzzles through: each starts as puzzle_start's (square 0's
# building at 9 s), then the moves.
def puzzle_play(p, d, moves, shots):
    return dict(floor=True, puzzle=p, difficulty=d, events=[click(0, *LEVELS[0]), click(9, *BUILDING)] + moves,
                shots=shots)


# Binary Lights (g17_1256): switch K's button (DS:32BC's panel at 94h,
# 20h, each 1Eh x 71h).
BINARY = [(0x94 + x + 15, 0x20 + y + 0x38) for y in (5, 0x85) for x in (0xB, 0x5C, 0xAB, 0xFB)]


def switches(t, ks, gap=0.5):
    return [click(t + gap * i, *BINARY[k]) for i, k in enumerate(ks)]


SCENARIOS["play03d0"] = puzzle_play(3, 0, switches(13, [0, 1, 3]) + switches(20, [0, 1, 2, 3, 5]) + switches(29, [0, 3])
                                    + switches(36, [0, 4]) + switches(42, [1, 1, 0, 2, 3]),
                                    [(12, "r1"), (13.6, "r1b"), (14.6, "solved1"), (16, "flash"), (19, "r2"),
                                     (22.2, "solved2"), (28, "r3"), (35, "r4"), (41, "r5"), (42.3, "wrong"),
                                     (44.3, "solved5"), (45.5, "won"), (47, "won2"), (50, "won3"), (55, "won4")])
BINARY_HELP = (0x20 + 0x22, 0x9E + 9)  # f06_2436's lesson button
BINARY_EXIT = (0x1EE + 0x22, 0x16 + 0xE)  # f06_23d8's
SCENARIOS["play03d3"] = puzzle_play(3, 3, switches(13, [4, 0, 2, 3]) + [click(20, *BINARY_HELP), click(23, 320, 200),
                                                                        click(25, *BINARY_EXIT)],
                                    [(13.2, "invalid"), (14.6, "solved1"), (19, "r2"), (21, "help"), (24, "helped"),
                                     (25.5, "exit"), (27, "exit2"), (30, "exit3"), (34, "exit4")])


# Concentration (g15_1142): door R, C's picture (DS:2ED6's panel at 20h,
# 0Ch: buttons 3Ch x 2Ah every 54h x 34h).
def door(r, c):
    return (0x20 + 6 + 0x54 * c + 0x1E, 0xC + 4 + 0x34 * r + 0x15)


SCENARIOS["play07d0"] = puzzle_play(7, 0, [click(13, *door(1, 2)), click(14, *door(2, 2)),
                                          click(17, *door(2, 2)), click(18, *door(2, 3)), click(21, 320, 200),
                                          click(23, *door(1, 2)), click(24, *door(1, 3)), click(27, 320, 200)],
                                    [(13.3, "one"), (14.3, "miss"), (15.6, "closed"), (18.3, "pair"), (19.5, "fact"),
                                     (21.5, "found"), (24.3, "pair2"), (25.5, "fact2"), (27.5, "won"), (29, "won2"),
                                     (32, "won3"), (36, "won4")])


def door_switch(r, c, k):
    """Door R, C's colour switch K (0 the top, 1 the one under it)."""
    return (0x20 + 0x54 * c + 0x45 + 6, 0xC + 0x34 * r + 3 + 0x11 * k + 5)


CONC_EXIT = (0x216 + 0x42 + 0x14, 0x23 + 0x26)  # DS:2A0C's lever
CONC_GADGET = (0x216 + 8 + 0x18, 0x23 + 0x41 + 0x12)
CONC_HELP = (0x220 + 0x18, 0xCC + 0x1C)
SCENARIOS["play07d8"] = puzzle_play(7, 8, [click(13, *door(0, 0)), click(14, *door(0, 1)), click(15, *door(0, 2)),
                                          click(18, *CONC_GADGET), click(22, *CONC_HELP), click(24, 320, 200),
                                          click(26, *door_switch(4, 5, 1)), click(27, *door_switch(4, 5, 0)),
                                          click(29, *CONC_EXIT)],
                                    [(13.3, "one"), (15.3, "three"), (16.5, "closed"), (18.4, "gadget"),
                                     (19.3, "gadget2"), (23, "help"), (24.5, "helped"), (26.4, "switch1"),
                                     (27.4, "switch0"), (29.4, "lever"), (31, "left"), (34, "left2"), (38, "left3")])


# Codes (g20_1474): the message's symbol I (DS:3AA8's panel) and the
# chart's letter K (DS:3AC2's, decoding only).
def code_slot(i):
    return (0x48 + 0x26 * (i % 13) + 0x13, 0x24 + (0 if i < 13 else (i // 13) * 0x2C + 0x10) + 0x16)


def code_letter(ch):
    k = ord(ch) - 65
    return (0x48 + 0x26 * (k % 13) + 0x13, 0xDA + (0 if k < 13 else 0x3C) + 0x16)


def decode(t, word, gap=1.6):
    return [e for i, ch in enumerate(word) for e in (click(t + gap * i, *code_slot(i)),
                                                      click(t + gap * i + 0.4, *code_letter(ch)))]


SCENARIOS["play06d0"] = puzzle_play(6, 0, [click(13, *code_slot(0)), click(13.4, *code_letter("B"))]
                                    + decode(15, "WASHINGTON"),
                                    [(13.2, "picked"), (13.8, "wrong"), (14.6, "after"), (15.8, "right"),
                                     (17, "one"), (22, "mid"), (29.6, "last"), (30.2, "right10"), (31.5, "won"),
                                     (33, "won2"), (36, "won3"), (40, "won4")])


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
    if s.get("floor"):
        skip.append("--floor")
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
    shots = {(d / f"{n}.png").read_bytes() for _, n in s["shots"] if (d / f"{n}.png").exists()}
    if len(s["shots"]) > 1 and len(shots) == 1:
        # One picture throughout: the display asleep (nothing drawn) or
        # something over the game. Not kept: it may show the desktop.
        shutil.rmtree(d)
        sys.exit(f"{name}: every shot of the original is the same picture (the display asleep?); deleted")


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
    if s.get("floor"):
        env["EDISON_FLOOR"] = "1"
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
