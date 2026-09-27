"""Decompiles the menu's scripts (archive group 03 in SHELL.D01).

EDISON.EXE runs these with a small cooperative interpreter (see
docs/GAME.md). File layout:

    u16 count
    u16 offset[count]              script records, from the start of the file
    record:
        u16 number                 scripts are keyed (archive id, number)
        u16 statements
        u16 code_size
        u16 statement_offset[statements]   from the start of the file
        code[code_size]
    statement:
        u16 command                index into the command table (COMMANDS)
        u16 present                bit k set: argument slot k was given
        u16 slot[...]              slot k at +4 + 2k

Usage: python tools/scripts.py [extracted/shell/03xx.bin ...]
"""
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# Command table in EDISON.EXE (DS:0848): name and ReadArgs-style template.
COMMANDS = [
    ("SHOWLOGO", "XPOS/N/A,YPOS/N/A,LOGONAME/A"),
    ("SHOWCLOGO", "XPOS/N/A,YPOS/N/A,LOGONAME/A"),
    ("CALL", "ASYNC/S,SCRIPT/K/A,DUMMY/S"),
    ("SHOWFSCREEN", "SCREEN/A,SETPALETTE/S"),
    ("STARTANIM", "ANIMNAME/K/A,XPOS/N,YPOS/N,WAIT/S,SOUND/K"),
    ("STOPANIM", "ANIMNAME/K,ALL/S"),
    ("ADDBUTTONS", "BUTTFILE/K/A"),
    ("KILLSCRIPT", "SCRIPT/K/A"),
    ("SETTEXTFONT", "FONT/A"),
    ("STARTSOUND", "SOUNDTAG/K/A"),
    ("DRAWLINE", "X1/N/A,Y1/N/A,X2/N/A,Y2/N/A,PEN/N"),
    ("DRAWPOINT", "XPOS/N/A,YPOS/N/A,PEN/N"),
    ("SETDRAWPEN", "PEN/N/A"),
    ("SETTEXTPEN", "PEN/N/A"),
    ("COPYFSCREEN", "SRC/N/A,DEST/N/A"),
    ("WORKSCREEN", "SCRNUM/N/A"),
    ("SLEEP", "TICKS/N/A"),
    ("RESTART", ""),
    ("COPYAREA", "SRCSCR/N/A,DESTSCR/N/A,LEFT/N/A,TOP/N/A,WIDTH/N/A,HEIGHT/N/A"),
    ("CLEARBUTTONS", ""),
    ("DRAWTEXT", "XPOS/N/A,YPOS/N/A,TEXT/K/A"),
    ("SETPALETTE", "FROMSCREEN/N/A"),
    ("FADE", "OUT/S,IN/S,SCREEN/N,STEP/N"),
    ("UNIQUE", ""),
    ("MOUSE", "ON/S,OFF/S"),
    ("TELL", "NUMBER/N"),
]


def parse(data):
    """Yields (number, [(statement offset, command, present mask, bytes)])."""
    (count,) = struct.unpack_from("<H", data, 0)
    for k in range(count):
        (rec,) = struct.unpack_from("<H", data, 2 + 2 * k)
        number, n, size = struct.unpack_from("<HHH", data, rec)
        offsets = struct.unpack_from(f"<{n}H", data, rec + 6)
        code_end = rec + 6 + 2 * n + size
        bounds = sorted(offsets) + [code_end]
        statements = []
        for off in offsets:
            end = bounds[bounds.index(off) + 1]
            cmd, present = struct.unpack_from("<HH", data, off)
            statements.append((off, cmd, present, data[off + 4:end]))
        yield number, statements


def describe(cmd, present, args):
    name, template = COMMANDS[cmd] if cmd < len(COMMANDS) else (f"CMD{cmd}", "")
    names = [t.split("/")[0] for t in template.split(",") if t]
    out = []
    for k in range(16):
        if not present & (1 << k):
            continue
        label = names[k] if k < len(names) else f"slot{k}"
        if 4 + 2 * k > len(args) + 2:
            out.append(f"{label}=?")
            continue
        (v,) = struct.unpack_from("<H", args + b"\0\0", 2 * k)
        out.append(f"{label}={v:#06x}" if v > 9 else f"{label}={v}")
    extra = args[16:] if len(args) > 16 else b""
    tail = f"  ; +{extra.hex(' ')}" if extra else ""
    return f"{name:<12} {' '.join(out)}  [{args.hex(' ')}]{tail}"


def main():
    files = [Path(a) for a in sys.argv[1:]] or sorted((ROOT / "extracted" / "shell").glob("03*.bin"))
    for f in files:
        data = f.read_bytes()
        for number, statements in parse(data):
            print(f"== {f.stem}.{number}")
            for off, cmd, present, args in statements:
                print(f"  {off:04x}: {describe(cmd, present, args)}")


if __name__ == "__main__":
    main()
