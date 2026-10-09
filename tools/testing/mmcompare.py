"""Mystery at the Museums against the original: a timeline of clicks, holds
and keys from "Please pick a level", played in the port (EDISON_SKIP:
setup starts at the level pick as the player SKIP; EDISON_SQUARE: every
square plays one puzzle; EDISON_FLOOR, a scenario's floor=True: the
first visit to the office goes into the Museum; EDISON_TIME, time=S: the
countdown from S seconds; EDISON_FOUND, found=True: every object found) and in the original under
winevdm (MALLSKIP.EXE, tools/reference/mall_skip.py, the same), each of
the original's shots against the port's closest frame near its time.

  python tools/testing/mmcompare.py [NAME ...] [--port-only | --compare-only] [--list]
      [--watch EXPR ...] [--peek SECONDS EXPR ...]

Both draw the same random numbers (segment 46's generator is never
seeded: docs/MYSTERY.md), so the boards, the puzzles and their layouts are
the same. Time 0 is the level's click. Shots and diffs (the pixels that
differ in magenta) go to build/scratch/mmcompare/NAME/. Needs EDISON_RUN
and OTVDM, and the CD mounted (the speech WAVs are on it). The original's
MYSTERY.HS and MEDISON.COL are put back after each run, SKIP.INF removed.

A shot that differs also gets zoom-SHOT.png: the differing area, the
original's on the left, the port's on the right, enlarged. --watch has
tools/reference/memwatch.py follow the original's data segment while it
plays (its expressions: `88ac` the word at DS:88AC, `u:9396`, `[x]+2`...;
lines to NAME/orig/watch.txt, milliseconds since it found the game);
--peek reads them once, SECONDS after the level's click. mmsolve.py works
out the puzzles' moves.
"""
import argparse
import os
import shutil
import subprocess
import sys
import threading
import time
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


# Keys held by name: the original's virtual-key codes (with their scan codes:
# VK_CLEAR is the keypad's 5, 4Ch), the port's --key names.
VKEYS = {"esc": 27, "kp5": 12, "left": 37, "up": 38, "right": 39, "down": 40, "shift": 16}


def press(t, text):
    """Typed as key presses: the port's --press (SDL key events, Caps Lock
    on), the original's as key()."""
    return (t, "press", text)


def keyhold(t, name, seconds=0.08):
    return (t, "keyhold", name, seconds)


def office(level=0):
    """The level picked at 0, then the Director's letter read (six presses
    of its arrow, held 80 ms as a click: the last is still held, as the
    game sees it, when the objects are up, which skips their wait): the
    office's map is up by about 30 s."""
    return [click(0, *LEVELS[level])] + [hold(11 + 1.5 * i, *ARROW) for i in range(6)]


DOOR = (268, 309)
OBJECT0 = (0xDC + 4 + 0x18, 0x5C + 6 + 0x12)  # the map's first object (DS:0CF8)
MAZE_EXIT = (180, 13)  # the bonus maze's exit (level 6's maze): a click gives up  # the office's door to the Museum (DS:0E76: 210-326, 284-334)

SCENARIOS = {
    "office": dict(events=office(0), shots=[(-0.01, "pick"), (1, "letsdoit"), (6, "office"), (10, "letter"), (13, "page2"),
                                            (20, "objects"), (26, "goodluck"), (32, "map")]),
    "skipfloor": dict(floor=True, events=[click(0, *LEVELS[0])],
                      shots=[(5, "office"), (7, "in"), (9, "floor"), (12, "floor2")]),
    "clock": dict(events=office(0), shots=[(18 + 0.5 * i, f"c{i:02d}") for i in range(21)]),
    # The end of a game out of time (mall_skip.py --time): the sad ending
    # and the high scores.
    "timeout": dict(time=10, events=office(0), shots=[(28 + i, f"t{28 + i}") for i in range(48)]),
    # Every object found (mall_skip.py --found): the dance, the final quiz,
    # the bonus maze, the happy ending and the high scores. (The maze's shots,
    # f27-f35, differ: its loop is unpaced in the original; mmmaze.py
    # compares its wanderers pass by pass.)
    "allfound": dict(found=True, events=office(0) + [click(36, *MAZE_EXIT)],
                     shots=[(26 + i, f"f{26 + i}") for i in range(70)]),
    # P in the office: "Game Paused!" (f06_229a) till a mouse button is held.
    "pause": dict(events=office(0) + [key(34, "p"), hold(38, 320, 300)],
                  shots=[(33 + 0.5 * i, f"p{i:02d}") for i in range(16)]),
    "floor": dict(events=office(0) + [click(34, *DOOR)], shots=[(33.9, "map"), (35, "door"), (37, "floor1"),
                                                               (40, "floor2"), (44, "floor3")]),
    # An object on the office's map held 6 s (g09_051e drops the button, so
    # its name goes once Edison has said it, though it's still held).
    "gridhold": dict(events=office(0) + [hold(34, *OBJECT0, 6.0)],
                     shots=[(33.9, "map")] + [(34.5 + 0.5 * i, f"g{i:02d}") for i in range(16)]),
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
                                    + decode(15.5, "WASHINGTON"),
                                    [(13.2, "picked"), (13.8, "wrong"), (14.6, "after"), (16.3, "right"),
                                     (17.5, "one"), (22.5, "mid"), (30.1, "last"), (30.7, "right10"), (32, "won"),
                                     (33.5, "won2"), (36.5, "won3"), (40.5, "won4")])
# (The decoding starts at 15.5 s so the solve, a second after the last
# letter, comes halfway between two of the clock's seconds: the clock runs
# on through the count, and a second there takes a step off it.)
# Unscrambling (level 1): "DLOLAR DIME  AS IDME NPENY" (phrase 1, DS:39B6)
# by four swaps; slot 16 is clicked twice before 17, which still swaps
# them ([3D80] counts on).
SCENARIOS["play06d1"] = puzzle_play(6, 1, [click(t, *code_slot(i)) for t, i in
                                           [(13, 1), (13.6, 2), (15, 16), (15.6, 16), (16.4, 17), (18, 21),
                                            (18.6, 22), (19.7, 22), (20.3, 23)]],
                                    [(13.2, "one"), (13.8, "swap1"), (15.8, "same"), (16.6, "swap2"),
                                     (18.8, "swap3"), (20.5, "won"), (22, "won2"), (25, "won3"), (30, "won4")])


# The Folded Cube (g29_10c0): net K (DS:674A's panel at 1B2h, DEh), the
# arrows that turn the cube (DS:6708's at 40h, 110h), the machine.
CUBE_NETS = [(0x1B2 + x + 0x26, 0xDE + y + 0x1C) for x, y in ((0x10, 0xE), (0x68, 0xE), (0x10, 0x52), (0x68, 0x52))]
CUBE_ARROWS = [(0x40 + x + w // 2, 0x110 + y + h // 2)
               for x, y, w, h in ((0x18, 2, 0x28, 0x19), (0x28, 0x39, 0x28, 0x17), (0, 0x1E, 0x28, 0x19),
                                  (0x40, 0x19, 0x20, 0x1C))]
CUBE_MACHINE = (0x15E + 0x15, 0x162 + 0x10)
# (The arrows turn the cube 180h a pass of the loop, unpaced: under winevdm
# about 570 a second, so a held arrow isn't compared; the port turns once
# a frame.)
SCENARIOS["play00d0"] = puzzle_play(0, 0, [click(17, *CUBE_NETS[0]), click(19, *CUBE_MACHINE),
                                          click(23, *CUBE_NETS[3])]
                                    + [click(23 + 3 * i, *CUBE_NETS[k]) for i, k in enumerate([0, 1, 3, 1], 1)],
                                    [(14, "cube1"), (17.3, "wrong"),
                                     (19.5, "machine"), (21, "machine2"), (23.3, "right"), (24.5, "cube2"),
                                     (27.5, "cube3"), (30.5, "cube4"), (33.5, "cube5"), (35.5, "won"), (37, "won2"),
                                     (40, "won3"), (45, "won4")])
# (Each cube's right net, from the generator: 3, 0, 1, 3, 1.)
# The hardest level (two faces traded on the wrong nets): six misses, so
# no bonus and the result's mode 2 (Edison's speech for a score <= 0).
SCENARIOS["play00d7"] = puzzle_play(0, 7, [click(t, *CUBE_NETS[k]) for t, k in
                                          [(13, 0), (13.6, 1), (14.2, 2), (15, 3), (17, 0), (17.6, 1), (18.2, 2),
                                           (19, 3), (21, 2), (23, 2), (25, 1)]],
                                    [(12.5, "cube1"), (13.3, "miss1"), (14.4, "miss3"), (15.5, "cube2"),
                                     (18.4, "miss6"), (19.5, "cube3"), (21.5, "cube4"), (23.5, "cube5"),
                                     (25.3, "won"), (27, "won2"), (30, "won3"), (35, "won4")])



# Liberty Planetarium (g28_178e): picture K (DS:909C's panel at 1C0h, EBh),
# the dome (a click shows or hides the lines), the help button (f06_2436
# at Eh, 78h), the projector (DS:5FE2). A right pick shows the fact (at
# the top: 28:1148), which a click puts away.
PLAN_BOXES = [(0x1C0 + x + 0x28, 0xEB + y + 0x1D) for x, y in ((0, 0), (0x58, 0), (0, 0x46), (0x58, 0x46))]
PLAN_DOME = (0xF0 + 0x5A, 0x28 + 0x50)
PLAN_HELP = (0xE + 0x1E, 0x78 + 0x1E)
PLAN_MACHINE = (0x26 + 0x1B, 0x2E + 0xB)
BALL_BOXES = CUBE_NETS  # the same panel (DS:674A)


def plan_rounds(t, picks):
    return [e for i, k in enumerate(picks) for e in (click(t + 4 * i, *PLAN_BOXES[k]), click(t + 4 * i + 2, 320, 200))]


# (Each round's right picture, from the generator: 1, 2, 2, 2, 1.)
SCENARIOS["play01d0"] = puzzle_play(1, 0, [click(13, *PLAN_BOXES[0]), click(14, *PLAN_DOME), click(15, *PLAN_HELP),
                                          click(17, 320, 200), click(18, *PLAN_MACHINE)]
                                    + plan_rounds(27, [1, 2, 2, 2, 1]),
                                    [(12.5, "r1"), (13.3, "wrong"), (14.3, "nolines"), (15.5, "help"),
                                     (17.5, "helped"), (19, "projector"), (22, "projector2"), (26.5, "projected"),
                                     (27.5, "fact1"), (29.5, "r2"), (31.5, "fact2"), (39.5, "fact4"), (41.5, "r5"),
                                     (43.5, "fact5"), (45.5, "won"), (47, "won2"), (50, "won3"), (55, "won4")])
# The hardest level (dots, a star moved on the wrong ones; answers 1, 0, 1,
# 1, 2): six misses, so no bonus.
SCENARIOS["play01d2"] = puzzle_play(1, 2, [click(t, *PLAN_BOXES[k]) for t, k in [(13, 0), (13.6, 2), (14.2, 3), (15, 1)]]
                                    + [click(17, 320, 200)]
                                    + [click(t, *PLAN_BOXES[k]) for t, k in [(19, 1), (19.6, 2), (20.2, 3), (21, 0)]]
                                    + [click(23, 320, 200)] + plan_rounds(25, [1, 1, 2]),
                                    [(12.5, "r1"), (14.4, "miss3"), (15.5, "fact1"), (18, "r2"), (20.4, "miss6"),
                                     (21.5, "fact2"), (25.5, "fact3"), (33.5, "fact5"), (35.5, "won"), (37, "won2"),
                                     (40, "won3"), (45, "won4")])


# The 3D Ball Sculpture (g30_136c): the Folded Cube's panels. (Answers 2,
# 2, 3, 1, 1.)
SCENARIOS["play02d0"] = puzzle_play(2, 0, [click(13, *BALL_BOXES[0]), click(14.5, *CUBE_MACHINE), click(18, *BALL_BOXES[2])]
                                    + [click(21.3 + 3 * i, *BALL_BOXES[k]) for i, k in enumerate([2, 3, 1, 1])],
                                    [(12.5, "r1"), (13.3, "wrong"), (15, "machine"), (18.3, "right"), (19.5, "r2"),
                                     (22.5, "r3"), (25.5, "r4"), (28.5, "r5"), (30.6, "won"), (32, "won2"),
                                     (35, "won3"), (40, "won4")])
# The hardest level (answers 2, 0, 2, 0, 2): six misses, no bonus. (Its
# time is the level of the game before's: level 0's in a new game.)
SCENARIOS["play02d5"] = puzzle_play(2, 5, [click(t, *BALL_BOXES[k]) for t, k in
                                          [(13, 0), (13.6, 1), (14.2, 3), (15, 2), (16.5, 1), (17.1, 2), (17.7, 3),
                                           (18.5, 0), (21.5, 2), (24.5, 0), (27.5, 2)]],
                                    [(12.5, "r1"), (14.4, "miss3"), (15.5, "r2"), (17.9, "miss6"), (19, "r3"),
                                     (22, "r4"), (25, "r5"), (27.8, "won"), (29, "won2"), (32, "won3"), (37, "won4")])


# The Question and Answer Period (f19_16a2): block K (DS:368E's panel at
# B0h, 4Eh: 48h x 2Eh), the pads 1-4 (DS:37B2 at C2h, 142h, every 48h),
# help (218h, F8h), the machine (DS:37D6).
QA_BLOCKS = [(0xB0 + x + 0x24, 0x4E + y + 0x17) for x, y in
             ((0x20, 0), (0x68, 0), (0xB0, 0), (0, 0x27), (0x48, 0x27), (0x90, 0x27), (0xD8, 0x27), (0x20, 0x4E),
              (0x68, 0x4E), (0xB0, 0x4E), (0, 0x75), (0x48, 0x75), (0x90, 0x75), (0xD8, 0x75), (0x20, 0x9C),
              (0xB0, 0x9C))]
QA_PADS = [(0xC2 + 0x48 * k + 0x13, 0x142 + 0xA) for k in range(4)]
QA_HELP = (0x218 + 0x28, 0xF8 + 0xB)
QA_MACHINE = (0x58 + 0x24, 0xF4 + 0x13)
# Each block's right answer (resource 3F0B's questions, as the generator
# shuffles them with no facts learned).
QA_RIGHT = [2, 2, 0, 0, 2, 2, 2, 3, 0, 3, 1, 3, 1, 3, 1, 1]
QA_EXIT = (0x228 + 0x28, 0x16B + 0xB)  # f06_23d8's, the final quiz's only
# Every object found at level 9 (index 3): the dance, then the final quiz
# (nine questions in 4 minutes): three answered (pad 0, right or wrong),
# then its exit: the sad ending and the high scores.
SCENARIOS["allfound3"] = dict(found=True, events=office(3) + [e for k in range(3) for e in (
                                  click(30 + 3 * k, *QA_BLOCKS[k]), click(31 + 3 * k, *QA_PADS[0]))]
                              + [click(40.5, *QA_EXIT)],
                              shots=[(24 + i, f"q{24 + i}") for i in range(50)])
# Concentration's facts are forgotten at the start of each game (f19_0000,
# 09:248a): two games at level 3, whose final quiz asks the facts learned
# first. Game 1: square 17's Concentration (museum 4: theme 1, DS:147E), its
# two pairs (a fact each), back to the office (every object found: the
# final quiz), a question, given up. Then play again, this level; game 2:
# back from the floor, the quiz's first block: "The U.S. flag was planted
# on the moon by" (with the facts kept, "Inuit sculptures are mostly carved
# from").
FACTS_SQUARE = (224, 225)  # square 17's building (room 4: 93h, C0h)
FLOOR_BACK = (0x3E + 0x27, 0x13C + 0x25)  # the floor's way back (DS:18CA)
SCENARIOS["factsclear"] = dict(floor=True, found=True, puzzle=7, difficulty=0, events=[
    click(0, *LEVELS[3]), click(9, *FACTS_SQUARE), click(12, *door(1, 2)), click(13, *door(1, 3)),
    click(17, 320, 200), click(18, *door(2, 2)), click(19, *door(2, 3)), click(22, 320, 200), click(34, *FLOOR_BACK),
    click(49, *QA_BLOCKS[0]), click(52, *QA_EXIT), click(106, 37, 242), click(109, 165, 242), hold(125, 107, 228),
    click(140, *FLOOR_BACK), click(155, *QA_BLOCKS[0])],
    shots=[(14.5, "fact1"), (20.5, "fact2"), (36, "office"), (48.5, "quiz1"), (50.5, "question1"), (80, "scores"),
           (105, "again"), (108, "looks"), (124, "level"), (138, "floor2"), (154, "quiz2"), (155.8, "question2"),
           (157, "question2b")])
# From the title (setup=True: MALLFREE.EXE, the port without EDISON_SKIP;
# time 0 is the start, the port's a second sooner: winevdm's own start).
LOOKS = [(0x12C + 0x30 + 0x36, 0x86 + y + 0xF) for y in (0x24, 0x42, 0x60, 0x7E)]  # DS:07D8, panel DS:080A
LOOKS_DONE = (0x12C + 0x39 + 0x37, 0x86 + 0xB0 + 0x15)
SETUP = dict(setup=True, start=0, lead=-1.1)
SCENARIOS["setupnew"] = dict(SETUP, events=[key(17, "zed|"), click(21, *LOOKS[0]), click(22, *LOOKS[0]),
                                            click(23, *LOOKS[1]), click(24, *LOOKS[2]), click(25, *LOOKS[3]),
                                            click(27, *LOOKS_DONE)],
                             shots=[(1 + 0.5 * i, f"n{i:02}") for i in range(88)])
# The name typed as keys with Caps Lock on: the library takes each key's
# character from its scan code (DS:72C8, Shift only: 39:0218), so the port
# shows "Zed" as the original does (the first letter made upper case by
# f08_0380), not "ZED".
SCENARIOS["setupcaps"] = dict(SETUP, events=[press(17, "zed"), press(18.5, "|")],
                              shots=[(16 + 0.5 * i, f"c{i:02}") for i in range(12)])
QUIT = (0x1FC + 0x1E, 0x158 + 0xB)  # the office's EXIT (DS:0DC6)
# The office's yes/no (f06_2976 at W/4 + fh, 2H/5 + 1.5 fh): clicked, as its
# keys are read held ([929C + scan / 8]) once a pass, and a typed one can
# come and go between two passes.
YES, NO = (192, 203), (320, 203)
# A new player's game left in the office: "Do you want to quit this game?"
# yes, "Do you want to play again?" no, "I'll save this game.": ZED.INF
# kept (orig/ZED.INF, port/save/ZED.INF), then loaded by loadgame.
SCENARIOS["savegame"] = dict(SETUP, keep=["ZED.INF"], events=[key(17, "zed|"), key(21, "|"), click(28, *LEVELS[0])]
                             + [hold(39 + 1.5 * i, *ARROW) for i in range(6)]
                             + [click(62, *QUIT), click(65, *YES), click(69, *NO)],
                             shots=[(20 + 2 * i, f"a{20 + 2 * i}") for i in range(18)]
                             + [(56 + i, f"a{56 + i}") for i in range(15)])
# A new player's game left in the office for another: "Do you want to quit
# this game?" yes, "Do you want to play again?" yes: the setup again
# (mode 0Fh), Edison as the player made him.
SCENARIOS["againsetup"] = dict(SETUP, events=[key(17, "zed|"), key(21, "|"), click(28, *LEVELS[0])]
                               + [hold(39 + 1.5 * i, *ARROW) for i in range(6)]
                               + [click(62, *QUIT), click(65, *YES), click(69, *YES)],
                               shots=[(60 + i, f"g{60 + i}") for i in range(30) if i != 9])  # (69: the click)
# ZED again (savegame's original file): keep the looks, play the saved game.
SCENARIOS["loadgame"] = dict(SETUP, players={"ZED.INF": OUT / "savegame" / "orig" / "Zed.INF"},
                             events=[key(17, "zed|"), click(20, 165, 241), click(23, 37, 250)],
                             shots=[(16 + i, f"l{16 + i}") for i in range(44)])
# The custom board editor (f11_19b4): "MAKE CUSTOM BOARD" at the level pick,
# the first map, OK.
CUSTOM = (0x14 + 2 + 0x56, 200 + 0x34 + 6)  # DS:0A24's last button (mode 2)
EDIT_MAPS = [(45 + 78 * i, 20) for i in range(8)]
EDIT_OK = (318, 370)
EDIT_GAMES = [(0x28, 0x10C), (0x28, 0xDC)]  # games 1 and 2 (the left column, bottom up)
EDIT_READY = (0x22E, 0x16A)  # "Level Ready"
# Under winevdm each blit takes the source's colour table at the time, so
# the editor's own palette (11:1a55: the picture's 0 and FF, not black and
# white) and the forced one of the screens shown before and after mix on
# the display; the port has one palette at a time: its 0 and FF differ.
EDIT_SAME = [((140, 164, 180), (0, 0, 0)), ((255, 255, 255), (236, 252, 252)), ((236, 252, 252), (255, 255, 255))]
SCENARIOS["editor"] = dict(SETUP, same=EDIT_SAME, events=[key(17, "zed|"), key(21, "|"), click(28, *CUSTOM), click(32, *EDIT_MAPS[0]),
                                          click(36, *EDIT_OK), click(38, *EDIT_GAMES[0]), click(40, 175, 95),
                                          click(42, *EDIT_GAMES[1]), click(44, 230, 140), click(47, *EDIT_READY)],
                           shots=[(27 + i, f"e{27 + i}") for i in range(34)])
SCENARIOS["play04d0"] = puzzle_play(4, 0, [click(13.5, *QA_BLOCKS[0]), click(14.5, *QA_PADS[0]), click(17.5, *QA_HELP),
                                          click(19, 320, 200), click(20, *QA_MACHINE)]
                                    + [e for k in range(1, 16) for e in (click(19.5 + 3 * k, *QA_BLOCKS[k]),
                                                                         click(20.5 + 3 * k, *QA_PADS[QA_RIGHT[k]]))],
                                    [(12.5, "board"), (14.2, "q0"), (14.8, "wrong"), (16, "wrong2"), (17, "answered"),
                                     (18, "help"), (20.5, "machine"), (22.8, "q1"), (23.7, "right1"), (25, "q1done"),
                                     (40, "mid"), (65, "q15"), (65.8, "right15"), (67, "won"), (69, "won2"),
                                     (72, "won3"), (77, "won4")])


# Dropping Squares (g18_22de): the buttons under the well (DS:34EC's panel
# at D5h, 131h: left, right, turn, drop), help (8, A3h), exit (223h, A3h).
# The drop falls a row a pass of the loop, unpaced in the original, so the
# shots come after each column lands. (Winning is 53 squares of cloth: not
# played; the exit lever ends it.)
DROP_LEFT, DROP_RIGHT = (0xD5 + 0x32, 0x131 + 0x28), (0xD5 + 0x96, 0x131 + 0x28)
DROP_TURN, DROP_DROP = (0xD5 + 0x62, 0x131 + 0xA), (0xD5 + 0x64, 0x131 + 0x4C)
DROP_HELP, DROP_EXIT = (8 + 0x2A, 0xA3 + 0x26), (0x223 + 0x2A, 0xA3 + 0x26)


def drop_moves(t, buttons, gap=0.2):
    return [click(t + gap * i, *b) for i, b in enumerate(buttons)]


SCENARIOS["play05d0"] = puzzle_play(5, 0, drop_moves(13, [DROP_LEFT, DROP_LEFT, DROP_TURN, DROP_DROP])
                                    + drop_moves(16, [DROP_RIGHT] * 3 + [DROP_DROP]) + drop_moves(19, [DROP_DROP])
                                    + drop_moves(21, [DROP_LEFT] * 5 + [DROP_DROP])
                                    + drop_moves(24, [DROP_TURN, DROP_DROP])
                                    + [click(26, *DROP_HELP), click(28, 320, 200)]
                                    + drop_moves(30, [DROP_RIGHT, DROP_DROP]) + [click(33, *DROP_EXIT)],
                                    [(12.5, "c1"), (13.5, "moved1"), (15.3, "landed1"), (16.3, "moved2"), (18.3, "landed2"),
                                     (20.5, "landed3"), (23.5, "landed4"), (25.5, "landed5"), (27, "help"),
                                     (29, "helped"), (32, "landed6"), (34, "exit"), (36, "exit2"), (40, "exit3")])


# The Circuit Analyzer (g16_089c, Mastermind): plug K (DS:30CC's panel at
# 8, A0h: column K / 4, place K % 4), column C's check button (DS:31A4 at
# 5, 65h), the meter (DS:31C8), help. A plug goes empty, colour 0, 1...
def circuit_plug(k):
    return (8 + (k // 4) * 0x34 + 0x18, 0xA0 + (k % 4) * 0x32 + 0xD)


def circuit_check(c):
    return (5 + c * 0x34 + 0x17, 0x65 + 0xE)


CIRCUIT_METER = (0x21C + 0x32, 0x15E + 0x17)
CIRCUIT_HELP = (0x12 + 0x1C, 0xC + 9)


def circuit_try(t, column, colours, gap=0.3):
    """Column's four plugs set to the colours (colour v: v + 1 clicks), then
    its check button."""
    events = []
    for place, v in enumerate(colours):
        for _ in range(v + 1):
            events.append(click(t, *circuit_plug(column * 4 + place)))
            t += gap
    return events + [click(t + 0.3, *circuit_check(column))]


# (The codes, from the generator: level 0 1, 0, 0, 1; level 7 3, 2, 4, 1.)
SCENARIOS["play08d0"] = puzzle_play(8, 0, circuit_try(13, 0, [0, 0, 0, 0])
                                    + [click(15.5, *CIRCUIT_METER), click(17.5, *CIRCUIT_HELP), click(19, 320, 200)]
                                    + circuit_try(20, 1, [1, 0, 0, 1]),
                                    [(12.5, "start"), (13.5, "plugs"), (14.8, "checked"), (16, "meter"),
                                     (18, "help"), (20.5, "plugs2"), (22, "plugs3"), (22.5, "won"), (24, "won2"),
                                     (27, "won3"), (32, "won4")])
SCENARIOS["play08d7"] = puzzle_play(8, 7, circuit_try(13, 0, [0, 1, 2, 3]) + circuit_try(17.5, 1, [3, 2, 4, 1]),
                                    [(12.5, "start"), (16, "plugs"), (17, "checked"), (21, "plugs2"), (22.7, "won"),
                                     (24, "won2"), (27, "won3"), (32, "won4")])


# Stackup (g25_1f6a): row I's stack C (DS:8CDE's panel at 168h, 23h: 46h x
# 32h, rows 37h apart), help (26h, 15Fh), the monitor's gadget (DS:B73C).
def stack(i, c):
    return (0x168 + c * 0x46 + 0x23, 0x23 + i * 0x37 + 0x19)


STACK_HELP, STACK_MACHINE = (0x26 + 0x22, 0x15F + 0xE), (0x12C + 0x1C, 0x162 + 0x12)
# (Each row's right stack, from the generator: level 0 2, 0, 0, 2, 1; level
# 2 2, 1, 2, 1, 1. A wrong pick slides the bars together.)
SCENARIOS["play09d0"] = puzzle_play(9, 0, [click(13, *stack(0, 0)), click(15, *STACK_HELP), click(16.5, 320, 200),
                                          click(17.5, *STACK_MACHINE)]
                                    + [click(t, *stack(i, c)) for t, i, c in
                                       [(21, 1, 0), (22, 2, 0), (23, 3, 2), (24.3, 4, 1)]],
                                    [(12.5, "rows"), (14.5, "wrong"), (15.5, "help"), (17, "helped"), (18.5, "machine"),
                                     (21.3, "right1"), (23.3, "right3"), (24.5, "won"), (26, "won2"), (30, "won3"),
                                     (35, "won4")])
SCENARIOS["play09d2"] = puzzle_play(9, 2, [click(t, *stack(i, c)) for t, i, c in
                                          [(13, 0, 0), (15, 1, 0), (17, 2, 0), (19, 3, 1), (20, 4, 1)]],
                                    [(12.5, "rows"), (14.5, "wrong1"), (18.5, "wrong3"), (19.5, "right4"),
                                     (20.5, "won"), (22, "won2"), (26, "won3"), (31, "won4")])


# The picture puzzles (segments 12-14): the board's cell R, C (loadPicture's
# layout for an N x N board of a W x H picture), the hold-to-view button
# (DS:223A), the gadget (DS:220A), help.
def pic_cell(n, w, h, r, c):
    tw, th = w // n, h // n
    left, top = 0x140 - (tw * n + (n - 1) * 2) // 2, 0x100 - (th * n + (n - 1) * 2) // 2
    return (left + c * (tw + 2) + tw // 2, top + r * (th + 2) + th // 2)


PIC_VIEW, PIC_GADGET, PIC_HELP = (0x118 + 0x35, 0x54 + 0xC), (0x232 + 0x1C, 0x10C + 0xD), (0x6C + 0x24, 0x12 + 0x11)


def pic_extras(t):
    """The whole picture held a second, the gadget, help: about 9 s."""
    return [hold(t, *PIC_VIEW, 1.0), click(t + 2, *PIC_GADGET), click(t + 7, *PIC_HELP), click(t + 8.5, 320, 200)]


def switch_solve(t, n, size, board, gap=0.4):
    """Tiles swapped into place in order (two clicks a swap)."""
    board, events = list(board), []
    for at in range(n * n):
        if board[at] != at:
            other = board.index(at)
            for k in (at, other):
                events.append(click(t, *pic_cell(n, *size, k // n, k % n)))
                t += gap
            board[at], board[other] = board[other], board[at]
    return events


# (The deals, from the generator.)
SCENARIOS["play12d0"] = puzzle_play(12, 0, pic_extras(13) + switch_solve(23, 2, (320, 200), [1, 0, 2, 3]),
                                    [(12.5, "board"), (13.5, "view"), (14.5, "viewed"), (16, "gadget"), (18, "show"),
                                     (20.5, "help"), (22, "helped"), (23.2, "marked"), (23.8, "won"), (25, "won2"),
                                     (28, "won3"), (33, "won4")])
SCENARIOS["play12d6"] = puzzle_play(12, 6, switch_solve(13, 8, (304, 192), [
    22, 36, 3, 62, 51, 55, 63, 49, 14, 9, 24, 54, 28, 52, 34, 27, 6, 18, 41, 38, 5, 43, 53, 25, 48, 19, 47, 58, 8, 2, 31,
    57, 42, 61, 10, 33, 16, 35, 12, 44, 21, 7, 20, 46, 56, 59, 17, 60, 45, 37, 29, 40, 11, 30, 50, 26, 4, 39, 23, 32, 1, 0,
    15, 13]), [(12.5, "board"), (20, "swaps"), (35, "swaps2"), (50, "swaps3"), (61.6, "won"), (63, "won2"), (66, "won3"),
               (71, "won4")])


def slide_moves(t, n, size, cells, gap=0.6):
    return [click(t + gap * i, *pic_cell(n, *size, r, c)) for i, (r, c) in enumerate(cells)]


# (The shortest slides back from the generator's mix. The bonus count has
# no waits, so the original's takes as long as its drawing; the shots
# come after it.)
SCENARIOS["play10d0"] = puzzle_play(10, 0, pic_extras(13) + slide_moves(23, 2, (320, 200),
                                                                        [(1, 1), (1, 0), (0, 0), (0, 1), (1, 1)]),
                                    [(12.5, "board"), (13.5, "view"), (16, "gadget"), (20.5, "help"), (23.3, "slid"),
                                     (24.5, "slid3"), (25.7, "won"), (27, "won2"), (30, "won3"), (35, "won4")])
SCENARIOS["play10d1"] = puzzle_play(10, 1, slide_moves(13, 3, (300, 180), [
    (2, 1), (2, 0), (1, 0), (0, 0), (0, 1), (0, 2), (1, 2), (1, 1), (1, 0), (0, 0), (0, 1), (1, 1), (2, 1), (2, 2)]),
                                    [(12.5, "board"), (15.3, "slid4"), (18.9, "slid10"), (22, "won"), (23, "won2"),
                                     (26, "won3"), (31, "won4")])


def arrow_button(n, size, a):
    """The Arrow Puzzle's arrow A (0-7 above column A, 8-15 below, 16-23
    left of row A - 16, 24-31 right) for N x N tiles of a W x H picture."""
    w, h = size
    b = n + 2
    tw, th = w // n, h // n
    left, top = 0x140 - (tw * b + (b - 1) * 2) // 2, 0x100 - (th * b + (b - 1) * 2) // 2
    right, bottom = left + b * tw + (b - 1) * 2, top + b * th + (b - 1) * 2
    g, k = divmod(a, 8)
    if g < 2:
        return (left + k * (tw + 2) + tw // 2, top - 20 + 9 if g == 0 else bottom + 9)
    return (left - 20 + 9 if g == 2 else right + 9, top + k * (th + 2) + th // 2)


PIC_LEVER = (0x258 + 0xF, 0xBA + 0x20)  # DS:21E6: give up
# (Level 0's shortest way from the generator's deal; level 1's is too long
# to search, so it shoves a few rows and columns and gives up.)
# (The arrows are held 80 ms, as the original's script's clicks are: the
# win ends the loop with the last one still down, so it stays pressed. The
# clock starts at the first shove; half a second apart, the win comes clear
# of a second: one during the lever's 0.4 s counts the bonus twice, the
# second time from nothing, in both games: g12_1416.)
SCENARIOS["play13d0"] = puzzle_play(13, 0, pic_extras(13) + [hold(23 + 0.5 * i, *arrow_button(2, (172, 100), a))
                                                             for i, a in enumerate([3, 17, 26, 3, 17, 27, 3])],
                                    [(12.5, "board"), (13.5, "view"), (16, "gadget"), (20.5, "help"), (23.3, "shove1"),
                                     (24.3, "shove3"), (25.5, "shove6"), (27, "won"), (28, "won2"), (30, "won3"),
                                     (35, "won4")])
SCENARIOS["play13d1"] = puzzle_play(13, 1, [hold(13 + 0.6 * i, *arrow_button(3, (204, 120), a))
                                            for i, a in enumerate([0, 8, 16, 24, 2, 18, 12, 28])]
                                    + [click(19, *PIC_LEVER)],
                                    [(12.5, "board"), (14, "shove3"), (16, "shove6"), (18, "shove8"), (19.5, "lever"),
                                     (21, "lever2"), (24, "lever3"), (28, "lever4")])


# Color Transformation (g27_1032): row I's choice K (DS:5034's buttons at
# F5h + 50h K, 4Bh + 30h I: 46h x 28h), help, the monitor's gadget.
def colour_choice(i, k):
    return (0xF5 + k * 0x50 + 0x23, 0x4B + i * 0x30 + 0x14)


# (Each row's right choice, from the generator: level 0 2, 2, 0, 1, 3; level
# 8 2, 1, 2, 0, 0. Five right first time cycle the colours at the end.)
SCENARIOS["play11d0"] = puzzle_play(11, 0, [click(13.5, *STACK_HELP), click(14.7, 320, 200), click(15.5, *STACK_MACHINE)]
                                    + [click(t, *colour_choice(i, k)) for i, (t, k) in
                                       enumerate([(19, 2), (20, 2), (21, 0), (22, 1), (23.3, 3)])],
                                    [(12.5, "rows"), (14, "help"), (16.5, "machine"), (19.3, "right1"),
                                     (22.3, "right4"), (23.5, "cycle"), (24.5, "cycle2"), (25.5, "cycle3"),
                                     (27.5, "won"), (29, "won2"), (35, "won3")])
SCENARIOS["play11d8"] = puzzle_play(11, 8, [click(t, *colour_choice(i, k)) for t, i, k in
                                          [(13, 0, 0), (13.6, 0, 2), (14.5, 1, 0), (15.1, 1, 2), (15.7, 1, 3),
                                           (16.5, 1, 1), (17.5, 2, 2), (18.5, 3, 0), (19.3, 4, 0)]],
                                    [(12.5, "rows"), (13.3, "wrong"), (13.9, "right1"), (16, "wrong3"),
                                     (16.8, "right2"), (19.5, "won"), (21, "won2"), (24, "won3"), (29, "won4")])
# The gadget held 5 s: its flag stays till the release (g27_005c), so it
# goes round again, three times. (Not past 20 s: the release comes during the
# third, and the original takes it after the move that otvdm.ps1 posts first,
# a poll later, so it starts a fourth; the port sees the button up.)
SCENARIOS["play11d0g"] = puzzle_play(11, 0, [hold(13.5, *STACK_MACHINE, 5.0)],
                                     [(12.5, "rows")] + [(13.8 + 0.5 * i, f"g{i:02d}") for i in range(13)])


# What Comes Next (g26_19d8): row I's answer K (DS:9006's panel at 14Fh,
# 23h: 3Ch x 32h, rows 37h apart); help and the monitor as Stackup's.
def next_answer(i, k):
    return (0x14F + k * 0x3C + 0x1E, 0x23 + i * 0x37 + 0x19)


# (Each row's right answer, probed in the port: level 0 0, 2, 2, 0, 1;
# level 5 0, 1, 0, 1, 0.)
SCENARIOS["play14d0"] = puzzle_play(14, 0, [click(13.5, *STACK_HELP), click(14.7, 320, 200), click(15.5, *STACK_MACHINE),
                                          click(18.5, *next_answer(0, 1))]
                                    + [click(t, *next_answer(i, k)) for i, (t, k) in
                                       enumerate([(19.5, 0), (20.5, 2), (21.5, 2), (22.5, 0), (23.3, 1)])],
                                    [(12.5, "rows"), (14, "help"), (16.5, "machine"), (18.8, "wrong"), (19.8, "right1"),
                                     (22.8, "right4"), (24, "won"), (26, "won2"), (30, "won3"), (36, "won4")])
SCENARIOS["play14d5"] = puzzle_play(14, 5, [click(t, *next_answer(i, k)) for t, i, k in
                                          [(13, 0, 1), (13.6, 0, 2), (14.3, 0, 0), (15, 1, 0), (15.7, 1, 1),
                                           (16.5, 2, 0), (17.5, 3, 1), (18.5, 4, 0)]],
                                    [(12.5, "rows"), (13.9, "wrong2"), (14.6, "right1"), (16, "right2"),
                                     (18.8, "won"), (20.5, "won2"), (25, "won3"), (31, "won4")])


# The Dig (g21_185c): belt slot I (the five in view), wall place A, B
# (DS:3DD2's panel; g21_0a74's centres), help, the solution (held), the
# gadget. A player of level 6 or below plays the penguins: each tile must
# go back to its own place. A click picks a tile up and the next puts it
# down (g21_0be8). The solution shows while its button is held (play15d0s).
# The penguins' walk at the end (g21_1684) is a step a busy-wait, about
# 1 ms under winevdm: the port's frames every 1 ms through its walk, so a
# shot of the original's finds the same step.


def dig_move(t, frm, to):
    return [click(t, *frm), click(t + 1, *to)]

def dig_slot(i):
    return (i * 0x65 + 0x76, 0x15D)


def dig_wall(a, b):
    return (a * 0x64 + 0x78, b * 0x3C + 0x4B)


DIG_HELP, DIG_SOLUTION, DIG_GADGET = (0x1B0 + 0x23, 0x10), (0x24C + 0x1A, 0xE0 + 0x24), (0x1D, 0x48 + 0x19)
# (The dug-out tiles' places, from the generator: level 0 (4, 3), (0, 1);
# level 1 those, (2, 3), (3, 2).)
SCENARIOS["play15d0"] = puzzle_play(15, 0, dig_move(13.3, dig_slot(1), dig_wall(4, 3))
                                    + [click(15.5, *DIG_HELP), click(17, 320, 200), click(20, *DIG_GADGET)]
                                    + dig_move(25, dig_slot(0), dig_wall(4, 3)) + dig_move(27, dig_slot(1), dig_wall(0, 1)),
                                    [(12.5, "wall"), (13.6, "held"), (14.9, "back"), (16, "help"), (21, "gadget"), (24.5, "gadget2"), (26.5, "placed"),
                                     (28.6, "won"), (30, "won2"), (33, "won3"), (38, "won4")])
SCENARIOS["play15d0"]["dense"] = [(28.4, 28.7, 1)]
SCENARIOS["play15d0s"] = puzzle_play(15, 0, [hold(13.3, *DIG_SOLUTION, 1.0)],
                                     [(12.5, "wall"), (13.6, "solution"), (14.0, "solution2"), (14.8, "back")])
SCENARIOS["play15d1"] =puzzle_play(15, 1, [e for i, (a, b) in enumerate([(4, 3), (0, 1), (2, 3), (3, 2)])
                                            for e in dig_move(13.3 + 2 * i, dig_slot(i), dig_wall(a, b))],
                                    [(12.5, "wall"), (14.8, "placed1"), (16.8, "placed2"), (19.1, "placed3"),
                                     (20.9, "won"), (22, "won2"), (25, "won3"), (30, "won4")])
SCENARIOS["play15d1"]["dense"] = [(20.7, 21.0, 1)]

# The keys the puzzles read (the latched table [B774], or [929C] held): H
# shows the help in Binary Lights (f17_0b9c), Dropping Squares (g18_0bc4),
# Stackup (f25_189c) and What Comes Next (f26_0c82); Dropping Squares turns
# its piece with the keypad's 5 too; Stackup's 0 makes the time 2000 s and
# 1-3 start it again at that level; Esc held ends the Slide Puzzle (14:0552)
# and Esc the Planetarium, unscored (28:1489); F on the floor counts the
# next square's game as won unplayed (10:080e).
SCENARIOS["keys03"] = puzzle_play(3, 0, [key(13, "h"), click(15, 320, 200)],
                                  [(12.5, "r1"), (14, "help"), (15.8, "helped")])
SCENARIOS["keys05"] = puzzle_play(5, 0, [keyhold(13, "kp5"), key(14, "h"), click(16, 320, 200)],
                                  [(12.5, "c1"), (13.5, "turned"), (15, "help"), (17.4, "helped")])
SCENARIOS["keys09"] = puzzle_play(9, 0, [key(13.5, "h"), click(15, 320, 200), key(16.5, "0"), key(19, "3")],
                                  [(12.5, "rows"), (14.2, "help"), (15.8, "helped"), (17.8, "time"), (19.6, "restart"),
                                   (21, "restart2"), (23, "restart3")])
# Any key ends a wait ([B756], set for each WM_KEYDOWN: 39:0218), Shift
# alone too: Binary Lights' help closed with it.
SCENARIOS["keysany"] = puzzle_play(3, 0, [key(13, "h"), keyhold(15, "shift")],
                                   [(12.5, "r1"), (14, "help"), (15.8, "closed")])
SCENARIOS["keys14"] = puzzle_play(14, 0, [key(13.5, "h"), click(15, 320, 200)],
                                  [(12.5, "rows"), (14.2, "help"), (15.8, "helped")])
SCENARIOS["keys10"] = puzzle_play(10, 0, [keyhold(13, "esc", 0.3)],
                                  [(12.5, "board"), (13.6, "esc"), (15, "esc2"), (18, "esc3"), (22, "esc4")])
SCENARIOS["keys01"] = puzzle_play(1, 0, [keyhold(13, "esc")],
                                  [(12.5, "r1"), (13.6, "esc"), (15, "esc2"), (18, "esc3")])
SCENARIOS["keysF"] = dict(floor=True, puzzle=3, difficulty=0,
                          events=[click(0, *LEVELS[0]), key(8.2, "f"), click(9, *BUILDING)],
                          shots=[(8.9, "floor"), (9.6, "won"), (11, "won2"), (14, "won3"), (18, "won4")])


def run_folder():
    if not os.environ.get("EDISON_RUN"):
        sys.exit("set EDISON_RUN to the folder with the game files")
    return Path(os.environ["EDISON_RUN"])


# Dropping Squares' clock (f18's digitalTime box): its second and the
# column's step come from one timer (g18_0468), at a phase the original's
# setup sets (the calls it takes before the column speeds up from every 10
# to every 5), and that changes from run to run under winevdm: the step with
# the second, as the port's, or up to 0.4 s before it. So its clock is
# matched apart.
DROP_CLOCK = (0x1C1, 0xCD, 0x1C1 + 0x41, 0xCD + 0x19)
for _s in SCENARIOS.values():
    if _s.get("puzzle") == 5:
        _s["apart"] = [DROP_CLOCK]


def memwatch(d, watch=None, peek=None, seconds=0.0):
    """memwatch.py on the original while otvdm.ps1 plays (a thread): the
    expressions watched to D/watch.txt, or peeked once PEEK = (seconds
    after the level's click, expressions) to D/peek.txt."""
    exe = str(run_folder() / "MALLSKIP.EXE")
    tool = [sys.executable, str(REFERENCE / "memwatch.py")]

    def run():
        if peek:
            time.sleep(START + peek[0])
            out = subprocess.run(tool + ["peek", exe, *peek[1]], capture_output=True, text=True)
            (d / "peek.txt").write_text(out.stdout + out.stderr)
            return
        time.sleep(START / 2)  # the game started
        for _ in range(10):  # until memwatch finds it
            w = subprocess.run(tool + ["watch", exe, "--every", "1", "--for", str(seconds), "--out",
                                       str(d / "watch.txt"), *watch], capture_output=True, text=True)
            if w.returncode == 0:
                return
            time.sleep(1)

    t = threading.Thread(target=run, daemon=True)
    t.start()
    return t


def play_orig(name, s, watch=None, peek=None):
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
    if "time" in s:
        skip += ["--time", str(s["time"])]
    if s.get("found"):
        skip.append("--found")
    if s.get("setup"):  # from the title: MALLFREE.EXE (free_mouse.py), nothing skipped
        exe = "MALLFREE.EXE"
        subprocess.run([sys.executable, str(REFERENCE / "free_mouse.py")], check=True, stdout=subprocess.DEVNULL)
    else:
        exe = "MALLSKIP.EXE"
        subprocess.run(skip, check=True, stdout=subprocess.DEVNULL)
    lines = [f"wait {s.get('start', START)}"]
    # A hold is its press and its release, so shots can come between them.
    timeline = []
    for e in s["events"]:
        if e[1] == "hold":
            timeline += [(e[0], (e[0], "down", e[2], e[3])), (e[0] + e[4], (e[0] + e[4], "up", e[2], e[3]))]
        elif e[1] == "keyhold":
            vk = VKEYS[e[2]]
            timeline += [(e[0], (e[0], "keydown", vk)), (e[0] + e[3], (e[0] + e[3], "keyup", vk))]
        else:
            timeline.append((e[0], e))
    timeline += [(t, ("shot", n)) for t, n in s["shots"]]
    now = 0.0
    for at, e in sorted(timeline, key=lambda p: p[0]):
        if at > now:
            lines.append(f"wait {at - now:.2f}")
            now = at
        if e[0] == "shot":
            lines.append(f"shot {e[1]}.png")
        elif e[1] in ("click", "down", "up"):
            lines.append(f"{e[1]} {e[2]} {e[3]}")
        elif e[1] in ("keydown", "keyup"):
            lines.append(f"{e[1]} {e[2]}")
        elif e[1] in ("type", "press"):
            lines.append(f"type {e[2]}")
    (d / "script.txt").write_text("\n".join(lines) + "\n")
    kept = {f: (run / f).read_bytes() for f in SAVES if (run / f).exists()}
    for f, src in s.get("players", {}).items():  # players' files to start with
        shutil.copy2(src, run / f)
    players = set(run.glob("*.INF")) - {run / f for f in s.get("players", {})}
    watcher = None
    if watch or peek:
        end = max([t for t, _ in s["shots"]] + [e[0] for e in s["events"]])
        watcher = memwatch(d, watch, peek, START / 2 + end + 2)
    try:
        subprocess.run(["pwsh", "-NoProfile", "-File", str(REFERENCE / "otvdm.ps1"), "play", exe,
                        str(d / "script.txt"), str(d)], check=True)
    finally:
        for f, data in kept.items():
            (run / f).write_bytes(data)
        (run / "SKIP.INF").unlink(missing_ok=True)
        for f in set(run.glob("*.INF")) - players:  # the players the scenario made
            if f.name.upper() in s.get("keep", ()):
                shutil.copy2(f, d / f.name)
            f.unlink()
        if watcher:
            watcher.join(timeout=10)
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
    for f, src in s.get("players", {}).items():
        shutil.copy2(src, save / f)
    lead = s.get("lead", LEAD)
    args = []
    for e in s["events"]:
        ms = str(int((e[0] + lead) * 1000))
        if e[1] == "click":
            args += ["--click", ms, str(e[2]), str(e[3])]
        elif e[1] == "hold":
            args += ["--drag", ms, str(e[2]), str(e[3]), str(e[2]), str(e[3]), str(int(e[4] * 1000))]
        elif e[1] == "type":
            args += ["--type", ms, e[2]]
        elif e[1] == "press":
            args += ["--press", ms, e[2]]
        elif e[1] == "keyhold":
            args += ["--key", ms, e[2], str(int(e[3] * 1000))]
    for a, b, every in s.get("dense", []):  # the port's frames every EVERY ms from A till B
        args += ["--capture-dense", str(int((a + lead) * 1000)), str(int((b + lead) * 1000)), str(every)]

    end = max([t for t, _ in s["shots"]] + [e[0] for e in s["events"]]) + lead + 2
    env = dict(os.environ)
    if not s.get("setup"):
        env["EDISON_SKIP"] = "1"
    if s.get("floor"):
        env["EDISON_FLOOR"] = "1"
    if "time" in s:
        env["EDISON_TIME"] = str(s["time"])
    if s.get("found"):
        env["EDISON_FOUND"] = "1"
    if "puzzle" in s:
        env["EDISON_SQUARE"] = f"{s['puzzle']},{s['difficulty']}" if "difficulty" in s else str(s["puzzle"])
    with open(d / "run.log", "w") as log:
        subprocess.run([str(exe), str(ROOT / "original" / "cd" / "DSK3"), "--game", "mystery", "--hidden",
                        "--virtual-clock", "--save", str(save), "--capture", str(d), "100", *args,
                        "--quit-after", str(int(end * 1000))],
                       cwd=d, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=end + 60)


def zoom(orig, port, box, pad=8, most=600):
    """The area BOX (and PAD round it) of both, side by side, enlarged to
    at most MOST pixels wide."""
    x0, y0 = max(box[0] - pad, 0), max(box[1] - pad, 0)
    x1, y1 = min(box[2] + pad, orig.width), min(box[3] + pad, orig.height)
    w, h = x1 - x0, y1 - y0
    pair = Image.new("RGB", (w * 2 + 2, h), (255, 0, 255))
    pair.paste(orig.crop((x0, y0, x1, y1)), (0, 0))
    pair.paste(port.crop((x0, y0, x1, y1)), (w + 2, 0))
    k = max(1, min(8, most // pair.width))
    return pair.resize((pair.width * k, pair.height * k), Image.NEAREST)


def is_colour(im, rgb):
    """255 where IM is the colour RGB, else 0."""
    r, g, b = ImageChops.difference(im, Image.new("RGB", im.size, rgb)).split()
    return ImageChops.lighter(ImageChops.lighter(r, g), b).point(lambda v: 0 if v else 255)


def compare(name, s, window):
    """Each original shot against the port's frames within the window of
    its time: the fewest pixels that differ. The scenario's "apart" areas
    (x0, y0, x1, y1) are matched on their own, each in its best frame."""
    d = OUT / name
    lead = s.get("lead", LEAD)
    frames = sorted((int(p.stem), p) for p in (d / "port").glob("*.bmp"))
    for t, n in s["shots"]:
        o = Image.open(d / "orig" / f"{n}.png").convert("RGB")
        best = None
        apart = s.get("apart", ())
        best_apart = [None] * len(apart)
        for ms, p in frames:
            if abs(ms / 1000 - (t + lead)) > window:
                continue
            f = Image.open(p).convert("RGB")
            mask = ImageChops.difference(f, o).convert("L").point(lambda v: 255 if v else 0)
            for a, b in s.get("same", ()):  # colour pairs taken as equal (the original's, the port's)
                mask = ImageChops.subtract(mask, ImageChops.multiply(is_colour(o, a), is_colour(f, b)))
            for k, box in enumerate(apart):
                ck = mask.crop(box).histogram()[255]
                if best_apart[k] is None or ck < best_apart[k]:
                    best_apart[k] = ck
            if apart:
                mask = mask.copy()
                for box in apart:
                    mask.paste(0, box)
            c = mask.histogram()[255]
            if best is None or c < best[0]:
                best = (c, ms, mask, f)
        if best is None:
            print(f"{name} {n}: no port frame near {t + lead:.1f} s")
            continue
        c, ms, mask, f = best
        c += sum(best_apart)
        where = ""
        if c and not mask.getbbox():
            where = " in the areas apart"
        elif c:
            box = mask.getbbox()
            where = f" in {box}"
            zoom(o, f, box).save(d / f"zoom-{n}.png")
            f.paste((255, 0, 255), mask=mask)
            f.save(d / f"diff-{n}.png")
        print(f"{name} {n}: {c} pixels (port {ms / 1000 - lead:.1f} s){where}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("names", nargs="*")
    ap.add_argument("--port-only", action="store_true", help="play the port again, keep the original's shots")
    ap.add_argument("--compare-only", action="store_true", help="compare the last runs' frames")
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--window", type=float, default=1.5, help="seconds around each shot to search the port's frames")
    ap.add_argument("--watch", nargs="+", metavar="EXPR", help="memwatch.py these in the original as it plays")
    ap.add_argument("--peek", nargs="+", metavar=("SECONDS", "EXPR"),
                    help="memwatch.py these once in the original, SECONDS after the level's click")
    a = ap.parse_args()
    peek = (float(a.peek[0]), a.peek[1:]) if a.peek else None
    if a.list:
        for n, s in SCENARIOS.items():
            print(n, f"({len(s['shots'])} shots)")
        return
    for name in a.names or list(SCENARIOS):
        s = SCENARIOS[name]
        if not a.compare_only:
            play_port(name, s)
            if not a.port_only:
                play_orig(name, s, a.watch, peek)
        compare(name, s, a.window)


if __name__ == "__main__":
    main()
