"""The stack of a coverage run's hit: winevdm's report (coverage.py's
NAME.log.err) read as WMAIN's, MALL's or WINMAIN's functions.

  python tools/testing/covstack.py GAME SCENARIO

Each frame's return address (cs:ip, the selector the module's handle +
60h + 8 (n - 1), coverage.py's) as segment:offset and the function that
holds it; the first frame is the tripwire's (5 past its start). The
function's direct caller isn't there: the tripwire runs before its
prologue pushes bp, so the walk's next frame is the caller's caller.
"""
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import coverage as C  # noqa: E402

P = C.P


def main():
    game, scenario = sys.argv[1], sys.argv[2]
    exe = C.GAMES[game][0]
    text = (C.OUT / game / f"{scenario}.log.err").read_text(encoding="utf-8", errors="replace")
    base = C.base(text, exe)
    if base is None:
        sys.exit(f"no {C.module(exe)} ({exe}) in the report")
    find = P.locate(P.functions(exe))
    for sel, off in re.findall(r"cs:ip=([0-9a-f]{4}):([0-9a-f]{4})", text):
        sel, off = int(sel, 16), int(off, 16)
        if sel < base or (sel - base) % 8:
            print(f"  {sel:04X}:{off:04X} (not {exe})")
            continue
        seg = (sel - base) // 8 + 1
        print(f"  {seg:02d}:{off:04X} {find(seg, off) or '?'}")


if __name__ == "__main__":
    main()
