"""Writes copies of the games that leave the mouse free: MALLFREE.EXE
(Mystery), WINMFREE.EXE (Rock and Bach) and EDISFREE.EXE (the menu), next
to the originals. The Artech library's f42_0000 in MALL, f47_0000 in
WINMAIN and f22_0000 in EDISON (the same function) confine the cursor to
an area with USER.ClipCursor, which under winevdm pulls the real pointer
into the game's window and keeps it from the terminal. Here each jumps
over the call (the call's own bytes are patched by the loader, so they're
left alone); the area is still kept for the game's own checks. The menu's
copy still starts the unpatched games. Wild Science's copy is
`wmain_skip.py`'s WMAINSKP.EXE. They're for comparisons only: the CD's
files are left as they are.

Usage: python tools/reference/free_mouse.py [RUN DIR]   (default $EDISON_RUN)
then:  tools/reference/otvdm.ps1 start MALLFREE.EXE   (or WINMFREE.EXE)
"""
import os
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from ne import NEFile  # noqa: E402
import tripwire  # noqa: E402 (tools/testing/coverage.py's, when EDISON_TRIPWIRES is set)

# At seg:0052 `lea ax, [bp-0Eh]; mov dx, ss; push dx; push ax` before the
# `lcall USER.ClipCursor` at seg:0059; `jmp 005E` (EB 0A) in their place.
CLIP_AT = 0x52
CLIP_WAS, CLIP_NOW = bytes.fromhex("8d46f28cd25250"), bytes.fromhex("eb0a9090909090")
GAMES = [("MALL.EXE", 42, "MALLFREE.EXE"), ("WINMAIN.EXE", 47, "WINMFREE.EXE"),
         ("EDISON.EXE", 22, "EDISFREE.EXE")]


def main():
    if len(sys.argv) > 1:
        run = Path(sys.argv[1])
    elif os.environ.get("EDISON_RUN"):
        run = Path(os.environ["EDISON_RUN"])
    else:
        sys.exit("give the folder with the game files, or set EDISON_RUN")
    for name, seg, out in GAMES:
        exe = NEFile(run / name)
        data = bytearray(exe.data)
        clip = exe.segments[seg - 1]["offset"] + CLIP_AT
        if data[clip:clip + len(CLIP_WAS)] != CLIP_WAS:
            sys.exit(f"{name}: {seg}:{CLIP_AT:04X} isn't the ClipCursor call's pushes (another version?)")
        data[clip:clip + len(CLIP_NOW)] = CLIP_NOW
        tripwire.arm(data, exe, name)
        (run / out).write_bytes(data)
        print(f"wrote {run / out}")


if __name__ == "__main__":
    main()
