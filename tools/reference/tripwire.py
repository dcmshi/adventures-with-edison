"""Tripwires in the original's test copies, for tools/testing/coverage.py.

When EDISON_TRIPWIRES names a folder, the copies' writers (mall_skip.py,
winmain_skip.py, wmain_skip.py, free_mouse.py) call arm() on the bytes they
write: each address listed in FOLDER/<EXE>.txt ("SS:OOOO name" a line, EXE
the CD's file the copy is made from) gets 9A 00 00 00 00 (lcall 0000:0000),
so the original faults there if it ever runs it ("CALL: Selector is
null") and winevdm reports where (coverage.py reads the report). Not ud2
or int 3: winevdm steps over those and runs on. Unset, nothing changes.
"""
import os
from pathlib import Path

TRAP = bytes.fromhex("9a00000000")


def arm(data, ne, exe):
    """Puts the listed tripwires into data (the copy's bytes, ne the CD
    file's NEFile); returns how many."""
    folder = os.environ.get("EDISON_TRIPWIRES")
    if not folder:
        return 0
    path = Path(folder) / f"{Path(exe).name.upper()}.txt"
    if not path.exists():
        return 0
    n = 0
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line.strip():
            continue
        seg, off = line.split()[0].split(":")
        seg, off = int(seg), int(off, 16)
        s = ne.segments[seg - 1]
        if off + len(TRAP) > s["size"]:
            raise ValueError(f"{line}: past segment {seg}'s {s['size']} bytes")
        at = s["offset"] + off
        data[at:at + len(TRAP)] = TRAP
        n += 1
    print(f"tripwires: {n} in the copy of {exe} ({path})")
    return n
