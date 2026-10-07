"""Reading WMAIN.EXE (the Wild Science Arcade) for the port: one tool,
functions named as in the docs (f28_15a1; 28_15a1 and Ghidra's 10d8_15a1
work too).

  wm.py fn NAME...          Ghidra's decompilation (extracted/ghidra/wmain.c);
                            a function Ghidra missed is disassembled instead
  wm.py dis NAME [LINES]    ndisasm from there, far calls and jumps resolved
                            (relocations: f07_11b1, USER.ClipCursor), to the
                            first retf unless LINES is given
  wm.py xref NAME           who refers to it: the functions calling it
                            (Ghidra's output) and every far pointer to it in
                            the code and in the data segment (method tables)
  wm.py vtable OFF [N] [OFF2]  a method table in the data segment (N entries,
                            default 16); with OFF2 the two side by side, the
                            entries that differ marked; thunks resolved
  wm.py thunk NAME...       where a method-table thunk jumps, and what it
                            adds to `this`
  wm.py ds OFF [N]          N words (default 16) of the data segment at OFF,
                            and the string there if it's text
  wm.py calls NAME          the far calls a function makes, in order

WMAIN.EXE is read from $EDISON_RUN (default D:/tools/edison-run) or $WMAIN.
"""
import os
import re
import subprocess
import sys
import tempfile
from functools import lru_cache
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ne import NEFile  # noqa: E402

GHIDRA = ROOT / "extracted" / "ghidra" / "wmain.c"
DGROUP = 103


def selector(seg):
    return 0x1000 + (seg - 1) * 8


def segment_of(sel):
    return (sel - 0x1000) // 8 + 1


def parse(name):
    """(segment, offset) from f28_15a1, 28_15a1, 28:15a1 or 10d8_15a1."""
    m = re.fullmatch(r"[fgs]?([0-9a-fA-F]+)[_:]([0-9a-fA-F]+)", name)
    if not m:
        sys.exit(f"not a function name: {name}")
    a, off = m.group(1), int(m.group(2), 16)
    seg = segment_of(int(a, 16)) if len(a) == 4 else int(a)
    return seg, off


def fname(seg, off):
    return f"f{seg:02d}_{off:04x}"


@lru_cache(None)
def exe():
    path = os.environ.get("WMAIN") or os.path.join(os.environ.get("EDISON_RUN", "D:/tools/edison-run"), "WMAIN.EXE")
    return NEFile(path)


@lru_cache(None)
def targets(seg):
    """Relocation sites of a segment: site -> what it points at."""
    out = {}
    for r in exe().relocations(seg):
        t = r["target"]
        if r["kind"] == "internal":
            what = fname(*t) if isinstance(t[0], int) else str(t)
        elif r["kind"] == "import":
            what = f"{t[0]}.{t[1]}"
        else:
            what = f"osfixup {t}"
        for s in r["sites"]:
            out[s] = (what, r["addr_type"])
    return out


@lru_cache(None)
def ghidra():
    return GHIDRA.read_text(errors="replace") if GHIDRA.exists() else ""


def ghidra_fn(seg, off):
    text = ghidra()
    head = f"// ==== FUN_{selector(seg):04x}_{off:04x} "
    i = text.find(head)
    if i < 0:
        return None
    j = text.find("\n// ==== ", i + 1)
    return text[i:j if j > 0 else len(text)]


def disassemble(seg, off, lines=None):
    code = exe().segment_bytes(seg)
    with tempfile.NamedTemporaryFile(delete=False, suffix=".bin") as f:
        f.write(code[off:off + (lines or 400) * 8])
        tmp = f.name
    try:
        out = subprocess.run(["ndisasm", "-b", "16", "-o", hex(off), tmp], capture_output=True, text=True).stdout
    finally:
        os.unlink(tmp)
    sites = targets(seg)
    result, reach = [], off
    for line in out.splitlines():
        parts = line.split(None, 2)
        if len(parts) < 3:
            continue
        addr, raw, asm = int(parts[0], 16), parts[1], parts[2]
        # A far call or jump (9A / EA): its pointer at addr + 1.
        if raw[:2].upper() in ("9A", "EA") and addr + 1 in sites:
            asm = f"{'call' if raw[:2].upper() == '9A' else 'jmp'} far {sites[addr + 1][0]}"
        elif raw[:2].upper() == "E8":
            m = re.search(r"call (0x[0-9a-f]+)", asm)
            if m:
                asm = f"call {fname(seg, int(m.group(1), 16))}"
        result.append(f"{seg}:{addr:04x}  {raw:<14} {asm}")
        # The function's end: a retf past every forward jump seen (an
        # earlier retf is an early return).
        m = re.match(r"j\w+ (?:short |near )?(0x[0-9a-f]+)$", asm)
        if m:
            reach = max(reach, int(m.group(1), 16))
        if lines is None and raw.upper()[:2] in ("CB", "CA") and addr >= reach:
            break
        if lines is not None and len(result) >= lines:
            break
    return result


def cmd_fn(names):
    for n in names:
        seg, off = parse(n)
        text = ghidra_fn(seg, off)
        if text:
            print(text.rstrip())
        else:
            print(f"// {fname(seg, off)}: not in Ghidra's output; disassembled")
            print("\n".join(disassemble(seg, off)))


def cmd_dis(name, lines=None):
    seg, off = parse(name)
    print("\n".join(disassemble(seg, off, int(lines) if lines else None)))


def cmd_calls(name):
    seg, off = parse(name)
    for line in disassemble(seg, off):
        if " call " in line or " jmp far " in line:
            print(line)


def cmd_xref(name):
    seg, off = parse(name)
    me = fname(seg, off)
    ghidra_name = f"FUN_{selector(seg):04x}_{off:04x}"
    # Callers in Ghidra's output (the functions whose body names it).
    callers = []
    current = None
    for line in ghidra().splitlines():
        if line.startswith("// ==== FUN_"):
            current = line.split()[2]
        elif ghidra_name in line and current and current != ghidra_name:
            sel, o = current[4:].split("_")
            c = fname(segment_of(int(sel, 16)), int(o, 16))
            if c not in callers:
                callers.append(c)
    print(f"{me}: called in Ghidra's output by {', '.join(callers) or 'none'}")
    # Far pointers to it: code (far calls) and the data segment (tables).
    for s in range(1, len(exe().segments) + 1):
        try:
            sites = targets(s)
        except Exception:
            continue
        hits = sorted(site for site, (what, _) in sites.items() if what == me)
        if hits:
            where = "DS" if s == DGROUP else f"segment {s}"
            print(f"  far pointers in {where} at: {', '.join(f'{h:04x}' for h in hits)}")


def thunk(seg, off):
    """A method-table thunk (mov bx, sp; add word [ss:bx+4], N; jmp far X):
    "X (this +N)", or None."""
    code = exe().segment_bytes(seg)
    if code[off:off + 6] != bytes.fromhex("8bdc36834704") or code[off + 7] != 0xEA:
        return None
    n = code[off + 6] - 256 if code[off + 6] > 127 else code[off + 6]
    t = targets(seg).get(off + 8)
    return f"{t[0] if t else '?'} (this {n:+d})"


def vtable(off, count):
    sites = targets(DGROUP)
    data = exe().segment_bytes(DGROUP)
    out = []
    for i in range(count):
        a = off + 4 * i
        t = sites.get(a)
        text = t[0] if t else f"{int.from_bytes(data[a + 2:a + 4], 'little'):04x}:{int.from_bytes(data[a:a + 2], 'little'):04x}"
        if t and re.fullmatch(r"f\d\d_[0-9a-f]{4}", t[0]):
            th = thunk(*parse(t[0]))
            if th:
                text += f" -> {th}"
        out.append(text)
    return out


def cmd_vtable(off, count="16", other=None):
    a = vtable(int(off, 16), int(count))
    if other is None:
        for i, m in enumerate(a):
            print(f"{i:2d} +{4 * i:02X} {m}")
        return
    b = vtable(int(other, 16), int(count))

    def key(m):  # (a thunk by where it goes)
        return m.split(" -> ")[-1].split(" (")[0]
    print(f"         {off.upper():<36} {other.upper()}")
    for i, (x, y) in enumerate(zip(a, b)):
        print(f"{i:2d} +{4 * i:02X} {x:<36} {y:<36}{'' if key(x) == key(y) else '  <- differs'}")


def cmd_thunk(names):
    for n in names:
        seg, off = parse(n)
        print(f"{fname(seg, off)} -> {thunk(seg, off) or 'not a thunk'}")


def cmd_ds(off, count="16"):
    data = exe().segment_bytes(DGROUP)
    o, n = int(off, 16), int(count)
    words = [int.from_bytes(data[o + 2 * i:o + 2 * i + 2], "little") for i in range(n)]
    for i in range(0, n, 8):
        print(f"DS:{o + 2 * i:04X}  " + " ".join(f"{w:04X}" for w in words[i:i + 8]))
    end = data.find(b"\0", o)
    s = data[o:end] if end > o else b""
    if len(s) >= 3 and all(32 <= c < 127 for c in s):
        print(f'text: "{s.decode()}"')


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 1
    cmd, args = sys.argv[1], sys.argv[2:]
    {"fn": lambda: cmd_fn(args), "dis": lambda: cmd_dis(*args), "calls": lambda: cmd_calls(*args),
     "xref": lambda: cmd_xref(*args), "vtable": lambda: cmd_vtable(*args), "thunk": lambda: cmd_thunk(args), "ds": lambda: cmd_ds(*args)
     }.get(cmd, lambda: print(__doc__))()
    return 0


if __name__ == "__main__":
    sys.exit(main())
