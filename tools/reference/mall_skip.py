"""Writes MALLSKIP.EXE next to MALL.EXE: Mystery at the Museums starting at
"Please pick a level", with the mouse left free (as free_mouse.py's
MALLFREE.EXE), and optionally every square of the floor running one
puzzle at one difficulty.

- The game loop (f02_00ba) first calls the setup (f08_232c) with mode 0x0F
  (as after a game) instead of 1, so the players' names and looks aren't
  reset.
- The setup jumps from its start (08:2339) over the title (f08_2284) to
  Edison's and Smitty's colours (f06_1ce0 with the looks [C65C], [C130],
  [92AC], [C76E]: 0, the defaults), which only mode 1 sets.
- The setup's first step ([bp-4], 08:2467) is 11, the level pick: the
  name, the player's file, the looks and the saved game's question are
  skipped. The player is "SKIP" ([B465]) with the file SKIP.INF ([C3F2],
  which the skipped step 2 would have built: g08_0154), so the run
  folder's own players are left alone; the game writes SKIP.INF there.
- --puzzle P (0-15, names at DS:1A9C; docs/MYSTERY.md) and --difficulty D
  (0-7): the floor (f10_0708) reads the square's puzzle at 10:09A4 and its
  difficulty at 10:098A; here they're constants, so any square clicked
  (a museum on the map goes straight into one) runs that puzzle.

It's for comparisons only: the CD's file is left as it is.

Usage: python tools/reference/mall_skip.py [--puzzle P] [--difficulty D] [RUN DIR]   (default $EDISON_RUN)
then:  tools/reference/otvdm.ps1 start MALLSKIP.EXE
"""
import argparse
import os
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from free_mouse import CLIP_AT, CLIP_NOW, CLIP_WAS  # noqa: E402
from ne import NEFile  # noqa: E402

# (segment, offset, the bytes there, what they are)
MODE = (2, 0xD8, bytes.fromhex("c646fa01"), "the first setup's mode (mov byte [bp-6], 1)")
LOOKS = (8, 0x2339, bytes.fromhex("8a460625ff00"), "the setup's mode test (mov al, [bp+6]; and ax, 0FFh)")
STEP = (8, 0x2467, bytes.fromhex("c746fc0000"), "the setup's first step (mov word [bp-4], 0)")
DIFFICULTY = (10, 0x98A, bytes.fromhex("8a470125ff00"), "the square's difficulty (mov al, [bx+1]; and ax, 0FFh)")
PUZZLE = (10, 0x9A4, bytes.fromhex("8a871c9325ff00"), "the square's puzzle (mov al, [bx+931Ch]; and ax, 0FFh)")
CLIP = (42, CLIP_AT, CLIP_WAS, "the ClipCursor call's pushes")
DGROUP = 73
NAME, NAME_AT, PATH_AT = b"SKIP", 0xB465, 0xC3F2  # (zeros there in the file)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("run", nargs="?", help="the folder with the game files (default $EDISON_RUN)")
    ap.add_argument("--puzzle", type=int, choices=range(16), metavar="P")
    ap.add_argument("--difficulty", type=int, choices=range(8), metavar="D")
    a = ap.parse_args()
    run = Path(a.run or os.environ.get("EDISON_RUN") or sys.exit("give the folder with the game files, or set EDISON_RUN"))
    exe = NEFile(run / "MALL.EXE")
    data = bytearray(exe.data)

    def at(seg, off):
        return exe.segments[seg - 1]["offset"] + off

    for seg, off, was, what in (MODE, LOOKS, STEP, DIFFICULTY, PUZZLE, CLIP):
        if data[at(seg, off):at(seg, off) + len(was)] != was:
            sys.exit(f"MALL.EXE: {what} isn't at {seg}:{off:04X} (another version?)")
    for where, text in ((NAME_AT, NAME), (PATH_AT, NAME + b".INF")):
        if any(data[at(DGROUP, where):at(DGROUP, where) + len(text) + 1]):
            sys.exit(f"MALL.EXE: DS:{where:04X} isn't empty (another version?)")
        data[at(DGROUP, where):at(DGROUP, where) + len(text)] = text
    data[at(*MODE[:2]) + 3] = 0x0F
    data[at(*LOOKS[:2]):at(*LOOKS[:2]) + 2] = bytes([0xEB, 0x11])  # jmp 234C
    data[at(*STEP[:2]) + 3] = 11
    if a.difficulty is not None:  # mov ax, D
        data[at(*DIFFICULTY[:2]):at(*DIFFICULTY[:2]) + 6] = bytes([0xB8, a.difficulty, 0, 0x90, 0x90, 0x90])
    if a.puzzle is not None:  # mov ax, P
        data[at(*PUZZLE[:2]):at(*PUZZLE[:2]) + 7] = bytes([0xB8, a.puzzle, 0, 0x90, 0x90, 0x90, 0x90])
    data[at(*CLIP[:2]):at(*CLIP[:2]) + len(CLIP_NOW)] = CLIP_NOW
    out = run / "MALLSKIP.EXE"
    out.write_bytes(data)
    forced = ", ".join(f"{k} {v}" for k, v in (("puzzle", a.puzzle), ("difficulty", a.difficulty)) if v is not None)
    print(f"wrote {out}: straight to the level pick" + (f"; every square: {forced}" if forced else ""))


if __name__ == "__main__":
    main()
