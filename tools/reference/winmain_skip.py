"""Writes WINMSKIP.EXE next to WINMAIN.EXE: Rock and Bach going straight
into one activity (a hallway result: 2 the jukebox, 3 the Drum Clinic, 4
the Music Library, 6 Harmony Hall, 7 the Instrument Room, 8 Sound FX, 9
the Studio), with the mouse left free (as free_mouse.py's WINMFREE.EXE).

- In the top level (f33_0422) the intro's flag ([bp-6], 33:0439) starts
  at 0, so "Corel presents" and the logo are skipped (the driver is still
  loaded).
- The hallway (f24_1d4a), the first time, reads Edison's look (ed.yyy,
  f24_0e94) and puts its colours in (f24_0cd8), then returns N
  (24:1EB2: `jmp 1EBA`, and there `mov ax, N; jmp 246B`, its end): the
  greeting, the player's name and the look question are skipped. After
  the activity the hallway is the usual one.

It's for comparisons only: the CD's file is left as it is.

Usage: python tools/reference/winmain_skip.py N [RUN DIR]   (default $EDISON_RUN)
then:  tools/reference/otvdm.ps1 start WINMSKIP.EXE
"""
import os
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from free_mouse import CLIP_AT, CLIP_NOW, CLIP_WAS  # noqa: E402
from ne import NEFile  # noqa: E402

ACTIVITIES = {2: "the jukebox", 3: "the Drum Clinic", 4: "the Music Library", 6: "Harmony Hall",
              7: "the Instrument Room", 8: "Sound FX", 9: "the Studio"}
# 33:0439 `mov word ptr [bp-6], 1`: the intro's flag.
INTRO_AT, INTRO_WAS = 0x439, bytes.fromhex("c746fa0100")
# 24:1EB2 `add sp, 2` (after f24_0cd8(0)), then f27_01c4's far call (its
# pointer patched by the loader, so left alone), then at 1EBA
# `lea ax, [bp-2Ah]; push ax; push 0`: room for `mov ax, N; jmp 246B`.
HALL_AT, HALL_WAS = 0x1EB2, bytes.fromhex("83c4029ac401431d8d46d6506a00")


def main():
    if len(sys.argv) < 2 or not sys.argv[1].isdigit() or int(sys.argv[1]) not in ACTIVITIES:
        sys.exit("give the activity: " + ", ".join(f"{n} {a}" for n, a in ACTIVITIES.items()))
    n = int(sys.argv[1])
    if len(sys.argv) > 2:
        run = Path(sys.argv[2])
    elif os.environ.get("EDISON_RUN"):
        run = Path(os.environ["EDISON_RUN"])
    else:
        sys.exit("give the folder with the game files, or set EDISON_RUN")
    exe = NEFile(run / "WINMAIN.EXE")
    data = bytearray(exe.data)
    intro = exe.segments[33 - 1]["offset"] + INTRO_AT
    hall = exe.segments[24 - 1]["offset"] + HALL_AT
    clip = exe.segments[47 - 1]["offset"] + CLIP_AT
    for at, was, what in ((intro, INTRO_WAS, "the intro's flag"), (hall, HALL_WAS, "the hallway's looks"),
                          (clip, CLIP_WAS, "the ClipCursor call's pushes")):
        # (The far call's pointer is the loader's: only its opcode is checked.)
        if any(data[at + i] != b for i, b in enumerate(was) if not (what == "the hallway's looks" and 4 <= i < 8)):
            sys.exit(f"WINMAIN.EXE: {what} isn't where expected (another version?)")
    data[intro + 3] = 0
    data[hall:hall + 2] = bytes([0xEB, 0x06])                     # jmp 1EBA
    end = 0x246B - (HALL_AT + 8 + 6)                               # jmp 246B, from 1EC0
    data[hall + 8:hall + 14] = bytes([0xB8, n, 0x00, 0xE9, end & 0xFF, end >> 8])  # mov ax, n
    data[clip:clip + len(CLIP_NOW)] = CLIP_NOW
    out = run / "WINMSKIP.EXE"
    out.write_bytes(data)
    print(f"wrote {out}: straight into {ACTIVITIES[n]}")


if __name__ == "__main__":
    main()
