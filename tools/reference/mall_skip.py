"""Writes MALLSKIP.EXE next to MALL.EXE: Mystery at the Museums starting at
"Please pick a level", with the mouse left free (as free_mouse.py's
MALLFREE.EXE), and optionally every square of the floor running one
puzzle at one difficulty.

- The game loop (f02_00ba) first calls the setup (f08_232c) with mode 0x0F
  (as after a game) instead of 1, so the players' names and looks aren't
  reset.
- The setup in mode 0x0F jumps from its start (08:2339) over the title
  (f08_2284) to Edison's and Smitty's colours (f06_1ce0 with the looks
  [C65C], [C130], [92AC], [C76E]: 0, the defaults), which only mode 1
  sets. Not after a game (mode 0): f06_1ce0 turns its table's 6-bit
  colours into 8-bit ones where they are, so a second time spoils them
  (Edison's colours garbled in the next setup). (Play again from the
  office's quit box is mode 0x0F too: its setup still does it again.)
- The setup's "after a game" flag ([B786], 08:245D) stays 0, as on the
  first setup: the map shows the Director's letter and the first greeting,
  as the port's --level does.
- After its first step (the courtyard scene, which draws Edison), mode
  0x0F's next step is 11, the level pick, instead of 3 (08:250E: `add
  word [bp-4], 2` becomes 10; 1 more follows): the name, the player's
  file, the looks and the saved game's question are skipped. The player is "SKIP" ([B465]) with the file SKIP.INF ([C3F2],
  which the skipped step 2 would have built: g08_0154), so the run
  folder's own players are left alone; the game writes SKIP.INF there.
  The record is a new player's, as step 2 makes it without a file
  (f08_01ee: no saved game [B58C] or custom level [B4E7], FF; the 29
  squares of each FF FF FF 0, the 16 objects FF FF FF), not the data
  segment's zeros (a saved game at level 0, asked about after a game).
- --puzzle P (0-15, names at DS:1A9C; docs/MYSTERY.md) and --difficulty D
  (0-8, below the game's count at DS:197C): the floor (f10_0708) reads the square's puzzle at 10:09A4 and its
  difficulty at 10:098A; here they're constants, so any square clicked
  (a museum on the map goes straight into one) runs that puzzle.
- --floor: the first visit to the office goes straight into the Museum.
  The entrance (f09_15f8) starts its steps at the table's end (09:17D9:
  [bp-4] is 11, or -1, by the first-visit flag [bp+6], before the loop's
  `add [bp-4], 1`; the four-tick wait before it stays), so Edison, Smitty
  and the Director's letter don't come on; the first visit's objects and
  "Check the map and good luck!" (f09_1dd8 at 09:29D4, only reached on
  the first visit) become `mov [0E74], 1` (the table map's button, g09_0822)
  and a jump to 09:2D5B (the help panel, the timers), so the loop goes
  into the Museum at once. Later visits are as they were.
- --time S: the countdown starts at S seconds, whatever the level (the
  level's switch in f09's new board, 09:24B5-25B4, sets [9314] by `mov
  word [9314], N` for each of the 8 levels; each N becomes S; the clock's
  total [9312] is copied from it, 09:25FD): for reaching the end of a
  game out of time.
- --found: every object starts found (09:27DD: `mov byte [bx+2], 0`, the
  new object's flag, becomes 1), so the first map of the office ends the
  game (the dance, the final quiz, the maze, the happy ending).

It's for comparisons only: the CD's file is left as it is.

Usage: python tools/reference/mall_skip.py [--puzzle P] [--difficulty D] [--floor] [--time S] [--found] [RUN DIR]   (default $EDISON_RUN)
then:  tools/reference/otvdm.ps1 start MALLSKIP.EXE
"""
import argparse
import os
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from free_mouse import CLIP_AT, CLIP_NOW, CLIP_WAS  # noqa: E402
from ne import NEFile  # noqa: E402
import tripwire  # noqa: E402 (tools/testing/coverage.py's, when EDISON_TRIPWIRES is set)

# (segment, offset, the bytes there, what they are)
MODE = (2, 0xD8, bytes.fromhex("c646fa01"), "the first setup's mode (mov byte [bp-6], 1)")
LOOKS = (8, 0x2339, bytes.fromhex("8a460625ff003d01007403"),
         "the setup's mode test (mov al, [bp+6]; and ax, 0FFh; cmp ax, 1; je 2347)")
AGAIN = (8, 0x245D, bytes.fromhex("c60686b701"), "the setup's [B786] = 1 (mov byte [B786], 1)")
STEP = (8, 0x250E, bytes.fromhex("8346fc02"), "mode 0x0F's next step (add word [bp-4], 2)")
DIFFICULTY = (10, 0x98A, bytes.fromhex("8a470125ff00"), "the square's difficulty (mov al, [bx+1]; and ax, 0FFh)")
PUZZLE = (10, 0x9A4, bytes.fromhex("8a871c9325ff00"), "the square's puzzle (mov al, [bx+931Ch]; and ax, 0FFh)")
ENTRANCE = (9, 0x17C6, bytes.fromhex("c706b2920400833eb292007503e90300e9f3ffc746fc0000e90400"),
            "the entrance's wait and its steps' start (mov word [92B2], 4 ... jmp 17E5)")
FOUND = (9, 0x27DD, bytes.fromhex("c6470200"), "the new object's found flag (mov byte [bx+2], 0)")
TIMES = (9, 0x24B5, 0x2600)  # the level's switch: mov word [9314], N
FIRST = (9, 0x29D4, bytes.fromhex("68b40068d8006a5c"), "the first visit's objects (push 0B4h; push 0D8h; push 5Ch)")
CLIP = (42, CLIP_AT, CLIP_WAS, "the ClipCursor call's pushes")
DGROUP = 73
NAME, NAME_AT, PATH_AT = b"SKIP", 0xB465, 0xC3F2  # (zeros there in the file)
# A new player's record (f08_01ee without a file): (address, bytes).
NEW_PLAYER = ([(0xB473 + 4 * k, b"\xff\xff\xff\x00") for k in range(29)]  # the custom level's squares
              + [(0xB4E7, b"\xff")]                                       # no custom level
              + [(0xB4E8 + 4 * k, b"\xff\xff\xff\x00") for k in range(29)]  # the saved game's squares
              + [(0xB55C + 3 * k, b"\xff\xff\xff") for k in range(16)]      # its objects
              + [(0xB58C, b"\xff")])                                      # no saved game


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("run", nargs="?", help="the folder with the game files (default $EDISON_RUN)")
    ap.add_argument("--puzzle", type=int, choices=range(16), metavar="P")
    ap.add_argument("--difficulty", type=int, choices=range(9), metavar="D")
    ap.add_argument("--floor", action="store_true", help="the first visit to the office goes into the Museum")
    ap.add_argument("--time", type=int, metavar="S", help="the countdown starts at S seconds")
    ap.add_argument("--found", action="store_true", help="every object starts found")
    a = ap.parse_args()
    run = Path(a.run or os.environ.get("EDISON_RUN") or sys.exit("give the folder with the game files, or set EDISON_RUN"))
    exe = NEFile(run / "MALL.EXE")
    data = bytearray(exe.data)

    def at(seg, off):
        return exe.segments[seg - 1]["offset"] + off

    for seg, off, was, what in (MODE, LOOKS, AGAIN, STEP, DIFFICULTY, PUZZLE, ENTRANCE, FIRST, FOUND, CLIP):
        if data[at(seg, off):at(seg, off) + len(was)] != was:
            sys.exit(f"MALL.EXE: {what} isn't at {seg}:{off:04X} (another version?)")
    for where, text in ((NAME_AT, NAME), (PATH_AT, NAME + b".INF")):
        if any(data[at(DGROUP, where):at(DGROUP, where) + len(text) + 1]):
            sys.exit(f"MALL.EXE: DS:{where:04X} isn't empty (another version?)")
        data[at(DGROUP, where):at(DGROUP, where) + len(text)] = text
    for where, text in NEW_PLAYER:
        if any(data[at(DGROUP, where):at(DGROUP, where) + len(text)]):
            sys.exit(f"MALL.EXE: DS:{where:04X} isn't empty (another version?)")
        data[at(DGROUP, where):at(DGROUP, where) + len(text)] = text
    data[at(*MODE[:2]) + 3] = 0x0F
    # mov al, [bp+6]; cmp al, 0Fh; je 234C; cmp al, 1 (then je 2347 as it was)
    data[at(*LOOKS[:2]):at(*LOOKS[:2]) + 9] = bytes.fromhex("8a46063c0f740c3c01")
    data[at(*AGAIN[:2]) + 4] = 0
    data[at(*STEP[:2]) + 3] = 10
    if a.difficulty is not None:  # mov ax, D
        data[at(*DIFFICULTY[:2]):at(*DIFFICULTY[:2]) + 6] = bytes([0xB8, a.difficulty, 0, 0x90, 0x90, 0x90])
    if a.puzzle is not None:  # mov ax, P
        data[at(*PUZZLE[:2]):at(*PUZZLE[:2]) + 7] = bytes([0xB8, a.puzzle, 0, 0x90, 0x90, 0x90, 0x90])
    if a.floor:
        entrance = bytes.fromhex(
            "c706b2920400"  # 17C6 mov word [92B2], 4
            "833eb29200"    # 17CC cmp word [92B2], 0
            "75f9"          # 17D1 jne 17CC
            "837e0601"      # 17D3 cmp word [bp+6], 1  (CF: not the first visit)
            "1bc0"          # 17D7 sbb ax, ax          (-1 later, 0 the first time)
            "83c80b"        # 17D9 or ax, 0Bh          (-1, or 11)
            "8946fc"        # 17DC mov [bp-4], ax
            "9090")         # 17DF; 17E1 add word [bp-4], 1: step 0, or 12 (the end)
        assert len(entrance) == len(ENTRANCE[2])
        data[at(*ENTRANCE[:2]):at(*ENTRANCE[:2]) + len(entrance)] = entrance
        jmp = 0x2D5B - (0x29DA + 3)
        first = bytes.fromhex("c706740e0100") + bytes([0xE9, jmp & 0xFF, jmp >> 8])  # mov word [0E74], 1; jmp 2D5B
        data[at(*FIRST[:2]):at(*FIRST[:2]) + len(first)] = first
    if a.time is not None:
        start, end = at(TIMES[0], TIMES[1]), at(TIMES[0], TIMES[2])
        sites = [i for i in range(start, end) if data[i:i + 4] == bytes.fromhex("c7061493")]
        if len(sites) != 8:
            sys.exit(f"MALL.EXE: {len(sites)} countdown starts in the level's switch, not 8 (another version?)")
        for i in sites:
            data[i + 4:i + 6] = a.time.to_bytes(2, "little")
    if a.found:
        data[at(*FOUND[:2]) + 3] = 1
    data[at(*CLIP[:2]):at(*CLIP[:2]) + len(CLIP_NOW)] = CLIP_NOW
    out = run / "MALLSKIP.EXE"
    tripwire.arm(data, exe, "MALL.EXE")
    out.write_bytes(data)
    forced = ", ".join(f"{k} {v}" for k, v in (("puzzle", a.puzzle), ("difficulty", a.difficulty)) if v is not None)
    print(f"wrote {out}: straight to the level pick" + (", then into the Museum" if a.floor else "")
          + (f"; every square: {forced}" if forced else "")
          + (f"; the countdown from {a.time} s" if a.time is not None else "") + ("; every object found" if a.found else ""))


if __name__ == "__main__":
    main()
