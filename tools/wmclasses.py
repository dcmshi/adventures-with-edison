"""WMAIN.EXE's C++ classes (Wild Science Arcade), from its own code.

Each function that stores a vtable (a run of far pointers in the data
segment, 103) into its object (mov word [reg+off], VT, reg loaded from
this, [bp+6], or the virtual base's pointer at this+0, [this]: stores into
members built in place don't count) is a
constructor or a destructor; the vtables it stores first at an offset are
its bases' (their constructors inlined), the last its own, and the
constructors it calls are its bases' too. A class's name is its
Borland RTTI record's (the far pointer 8 bytes before its first vtable: a
record in a code segment whose word at +4 is the offset of the name). The tree is printed
with each class's vtables (by their offset in the object) and the slots it
sets itself ('.' for one inherited from its first base). Slots that are
Borland's adjustor thunks (mov bx,sp / add word ss:[bx+4],n / jmp far: this
moved by n, for a base further into the object) show their target.

Needs extracted/disasm/wmain.asm (tools/nedis.py).
Usage: python tools/wmclasses.py [--rooms]   (--rooms: the 110 rooms' classes too)
Out: extracted/disasm/wmain.classes.txt (git-ignored: derived from game code).
"""
import re
import struct
import sys
from collections import defaultdict
from pathlib import Path

from ne import NEFile

ROOT = Path(__file__).resolve().parent.parent
EXE = ROOT / "original" / "cd" / "DSK3" / "WMAIN.EXE"
ASM = ROOT / "extracted" / "disasm" / "wmain.asm"
OUT = ROOT / "extracted" / "disasm" / "wmain.classes.txt"
DATA = 103
ROOM_SEGMENTS = range(41, 61)  # the rooms' own classes (two vtables each, +5E and +70)
# Stores that only look like a vtable's: the dialog box's +10 is the number 80h.
NOT_VTABLES = {"24_0003"}


def far_pointers(ne, seg):
    """{site: (segment, offset)} of the segment's far-pointer relocations."""
    out = {}
    for r in ne.relocations(seg):
        if r["kind"] == "internal" and r["addr_type"] == 3:
            for site in r["sites"]:
                out[site] = r["target"]
    return out


def rtti_name(ne, target, code):
    """The class name in the RTTI record at target, or None."""
    seg, off = target
    if seg not in code:
        code[seg] = ne.segment_bytes(seg)
    c = code[seg]
    if off + 8 > len(c):
        return None
    at = struct.unpack_from("<H", c, off + 4)[0]
    m = re.match(rb"([A-Za-z_][A-Za-z0-9_]{2,})\x00", c[off + at:off + at + 40]) if 8 <= at < 0x80 else None
    return m.group(1).decode() if m else None


def main():
    rooms = "--rooms" in sys.argv
    ne = NEFile(EXE)
    far = far_pointers(ne, DATA)
    code_far = {}
    code = {}

    def vtable(start):
        # (It ends where the next class's RTTI pointer is.)
        out, a = [], start
        while a in far and a not in rtti:
            out.append(far[a])
            a += 4
        return out

    def slot_name(target):
        seg, off = target
        if seg not in code:
            code[seg] = ne.segment_bytes(seg)
        if seg not in code_far:
            code_far[seg] = far_pointers(ne, seg)
        c = code[seg]
        name = f"{seg:02d}_{off:04x}"
        # Borland's adjustor thunk: 8B DC 36 83 47 04 ib / EA far
        if c[off:off + 6] == b"\x8b\xdc\x36\x83\x47\x04" and c[off + 7] == 0xEA:
            n = struct.unpack_from("b", c, off + 6)[0]
            t = code_far[seg].get(off + 8)
            if t:
                return f"{t[0]:02d}_{t[1]:04x}({n:+d})"
        return name

    installs = defaultdict(list)
    calls = defaultdict(list)
    func = None
    this = {}  # the registers holding this ("") or the virtual base ("v")
    for line in ASM.read_text(encoding="utf-8").splitlines():
        m = re.match(r"^([fg]\d\d_[0-9a-f]{4}):", line)
        if m:
            func, this = m.group(1)[1:], {}
            continue
        if not func:
            continue
        m = re.search(r"mov word ptr \[(bx|si|di)(?: \+ (0x[0-9a-f]+))?\], (0x[0-9a-f]+)$", line)
        if m:
            vt = int(m.group(3), 16)
            # A vtable starts a run (the word before isn't a far pointer).
            if m.group(1) in this and vt in far and vt - 4 not in far:
                off = int(m.group(2) or "0", 16)
                # (The virtual base's vtables as offsets past 1000h.)
                installs[func].append((off + (0x1000 if this[m.group(1)] else 0), vt))
            continue
        m = re.search(r"\b(mov|lea|pop|add|sub|xchg|les|lds|xor|inc|dec) (bx|si|di)\b(.*)$", line)
        if m:
            reg, src = m.group(2), m.group(3).strip()
            if m.group(1) == "mov" and src == ", word ptr [bp + 6]":
                this[reg] = ""
            elif m.group(1) == "mov" and re.fullmatch(r", word ptr \[(bx|si|di)\]", src) and this.get(src[12:14]) == "":
                this[reg] = "v"
            else:
                this.pop(reg, None)
        m = re.search(r"lcall 0, 0xffff  ; s(\d\d):([0-9a-f]{4})", line)
        if m:
            calls[func].append(f"{m.group(1)}_{m.group(2)}")
        m = re.search(r"\bcall [fg](\d\d_[0-9a-f]{4})", line)
        if m:
            calls[func].append(m.group(1))

    for f in NOT_VTABLES:
        installs.pop(f, None)
    # At each offset a constructor stores its bases' vtables (their
    # constructors inlined) and then its own. Offsets of 100h and over are
    # members' (the room's lists, ...).
    final, inline = {}, defaultdict(set)
    for f, ins in installs.items():
        last = {}
        for off, vt in ins:
            if off in last and last[off] != vt:
                inline[f].add(last[off])
            last[off] = vt
        own = {o: v for o, v in last.items() if o < 0x100 or o >= 0x1000} or last
        final[f] = (own, {o: v for o, v in last.items() if o not in own})
    classes = defaultdict(list)  # main vtable -> functions
    for f, (own, _) in final.items():
        classes[own[min(own)]].append(f)
    vts = {k: {} for k in classes}
    for k, fs in classes.items():
        for f in fs:
            vts[k].update(final[f][0])
    by_vt = {}
    for k in classes:
        for v in vts[k].values():
            by_vt.setdefault(v, k)
    for k in classes:  # a class's own main vtable wins
        by_vt[k] = k
    owner = {f: k for k, fs in classes.items() for f in fs}
    parents = {}
    for k, fs in classes.items():
        ps = []
        for f in fs:
            for v in sorted(inline[f]):
                p = by_vt.get(v)
                if p and p != k and p not in ps:
                    ps.append(p)
            for callee in calls[f]:
                p = owner.get(callee)
                if p and p != k and p not in ps:
                    ps.append(p)
        # The primary base: one with a vtable where this class has its main one.
        main_off = min(vts[k])
        ps.sort(key=lambda p: 0 if main_off in vts[p] else 1)
        parents[k] = ps

    rtti = {}
    for site, t in far.items():
        n = rtti_name(ne, t, code)
        if n:
            rtti[site] = n

    def class_name(k):
        # The record before the class's main vtable, with no other class's
        # main vtable between them.
        below = [a for a in rtti if a < k and k - a <= 0x100]
        if not below or any(max(below) < c < k for c in classes):
            return "?"
        return rtti[max(below)]

    def is_room(k):
        return all(int(f[:2]) in ROOM_SEGMENTS for f in classes[k])

    def label(k):
        return "/".join(sorted(classes[k]))

    children = defaultdict(list)
    roots = []
    for k in classes:
        (children[parents[k][0]] if parents[k] else roots).append(k)

    lines = []

    def where(o):
        return f"v+{o - 0x1000:X}" if o >= 0x1000 else f"+{o:X}"

    def show(k, depth, seen):
        if k in seen or (is_room(k) and not rooms):
            return
        seen.add(k)
        pad = "  " * depth
        tables = " ".join(f"{where(o)}=DS:{v:04X}({len(vtable(v))})" for o, v in sorted(vts[k].items()))
        also = (" (also " + ", ".join(class_name(p) for p in parents[k][1:]) + ")") if len(parents[k]) > 1 else ""
        lines.append(f"{pad}{class_name(k)} ({label(k)}): {tables}{also}")
        for o, v in sorted(vts[k].items()):
            # (A virtual base's vtable is the base's own at the same offset.)
            same = [o, o - 0x1000 if o >= 0x1000 else o + 0x1000]
            base = next((vtable(vts[p][x]) for p in parents[k] for x in same if x in vts[p]), None)
            cells = []
            for i, t in enumerate(vtable(v)):
                inherited = base is not None and i < len(base) and base[i] == t
                cells.append("." if inherited else f"{i}:{slot_name(t)}")
            lines.append(f"{pad}    {where(o)}: {' '.join(cells)}")
        rest = [c for c in children[k] if is_room(c)]
        for ch in sorted(children[k], key=label):
            show(ch, depth + 1, seen)
        if rest and not rooms:
            lines.append(f"{pad}  ({len(rest)} room classes: --rooms)")

    seen = set()
    for r in sorted(roots, key=label):
        show(r, 0, seen)
    OUT.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"{len(classes)} classes -> {OUT}")


if __name__ == "__main__":
    main()
