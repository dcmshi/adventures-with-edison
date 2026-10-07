"""Writes WMAINSKP.EXE next to WMAIN.EXE: the original with the intro flag
[26CE] (DGROUP, segment 103) cleared, so it starts in the arcade's menu
(room 1). It skips the title, the story, the lab and the professor's first
lesson (f31_0025, f32_0319 and the start in f31's method 0, which sends
"go to room 1" instead of 501). And it leaves the mouse free: f74_0000
(at the window's creation and its activation) confines the cursor to the
game's window with USER.ClipCursor; here it jumps over the call (the
call's own bytes are patched by the loader, so they're left alone). It's
for comparisons only: the CD's file is left as it is.

Usage: python tools/reference/wmain_skip.py [RUN DIR]   (default $EDISON_RUN)
then:  tools/reference/otvdm.ps1 start "WMAINSKP.EXE -A"
"""
import os
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from ne import NEFile  # noqa: E402

FLAG = 0x26CE
# f74_0000: at 74:0059 `push ss; lea ax, [bp-0Ah]; push ax` before the
# `lcall USER.ClipCursor` at 74:005E; `jmp 0063` (EB 08) in their place.
CLIP_SEG, CLIP_AT, CLIP_WAS, CLIP_NOW = 74, 0x59, bytes.fromhex("168d46f650"), bytes.fromhex("eb08909090")


def main():
    if len(sys.argv) > 1:
        run = Path(sys.argv[1])
    elif os.environ.get("EDISON_RUN"):
        run = Path(os.environ["EDISON_RUN"])
    else:
        sys.exit("give the folder with the game files, or set EDISON_RUN")
    exe = NEFile(run / "WMAIN.EXE")
    seg = exe.segments[103 - 1]
    at = seg["offset"] + FLAG
    data = bytearray(exe.data)
    (value,) = struct.unpack_from("<H", data, at)
    if value != 1:
        sys.exit(f"WMAIN.EXE: [26CE] is {value}, expected 1 (another version?)")
    struct.pack_into("<H", data, at, 0)
    clip = exe.segments[CLIP_SEG - 1]["offset"] + CLIP_AT
    if data[clip:clip + 5] != CLIP_WAS:
        sys.exit(f"WMAIN.EXE: 74:{CLIP_AT:04X} isn't the ClipCursor call's pushes (another version?)")
    data[clip:clip + 5] = CLIP_NOW
    out = run / "WMAINSKP.EXE"
    out.write_bytes(data)
    print(f"wrote {out}")


if __name__ == "__main__":
    main()
