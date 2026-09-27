"""Whole-program disassembler for the game's NE executables.

Unlike disasm.py (one segment, built for the sound DLLs), this walks every
code segment, starting from the program entry point, the exports and every
far pointer the relocation table points at, and annotates:

  - far calls with their target: "USER.CreateWindow" for imports (ordinals
    named from Wine's Win16 .spec files, or from the export table of a DLL
    shipped on the CD), "s05:0123" for calls into another segment;
  - immediate values that point at a string in the auto data segment;
  - Borland switch tables (cmp reg, N / ja / shl / jmp cs:[bx+table]).

It also writes a summary: one line per function with the API calls, far
calls and strings it references, which is the quickest way to find things.

Output: extracted/disasm/<exe>.asm and <exe>.funcs.txt (git-ignored: derived
from game code). Wine's spec files are downloaded to extracted/cache/ once.

Usage: python tools/nedis.py FILE [FILE ...]
"""
import re
import struct
import sys
import urllib.request
from pathlib import Path

from capstone import CS_ARCH_X86, CS_MODE_16, Cs

from ne import NEFile

ROOT = Path(__file__).resolve().parent.parent
CACHE = ROOT / "extracted" / "cache"
OUT = ROOT / "extracted" / "disasm"
WINE = "https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/"
SPECS = {"KERNEL": "krnl386.exe16/krnl386.exe16.spec", "USER": "user.exe16/user.exe16.spec",
         "GDI": "gdi.exe16/gdi.exe16.spec", "MMSYSTEM": "mmsystem.dll16/mmsystem.dll16.spec",
         "WIN87EM": "win87em.dll16/win87em.dll16.spec"}
_SPEC_LINE = re.compile(r"^(\d+)\s+\S+\s+(?:-\S+\s+)*([A-Za-z_]\w*)")
_FLOW_END = {"ret", "retf", "iret", "jmp", "ljmp"}
_IMM = re.compile(r"0x([0-9a-f]{3,4})\b")


def import_names(module, search_dirs):
    """ordinal -> name for an imported module."""
    spec = SPECS.get(module.upper())
    if spec:
        path = CACHE / Path(spec).name
        if not path.exists():
            CACHE.mkdir(parents=True, exist_ok=True)
            urllib.request.urlretrieve(WINE + spec, path)
        names = {}
        for line in path.read_text(encoding="utf-8").splitlines():
            m = _SPEC_LINE.match(line)
            if m:
                names[int(m.group(1))] = m.group(2)
        return names
    for d in search_dirs:
        for f in d.glob("*"):
            if f.stem.upper() == module.upper() and f.suffix.upper() in (".DLL", ".EXE", ".DRV"):
                return NEFile(f).names
    return {}


class Program:
    def __init__(self, path):
        self.path = Path(path)
        self.ne = ne = NEFile(path)
        search = [self.path.parent, *self.path.parent.parent.glob("DSK*")]
        self.api = {m: import_names(m, search) for m in ne.imports}
        (self.dgroup,) = struct.unpack_from("<H", ne.data, ne.ne + 0x0E)
        self.data = ne.segment_bytes(self.dgroup) if self.dgroup else b""
        (ip, cs) = struct.unpack_from("<HH", ne.data, ne.ne + 0x14)
        self.code = {s["index"]: ne.segment_bytes(s["index"]) for s in ne.segments if not s["data"]}
        # Relocation site -> description, per segment.
        self.fixups = {i: {} for i in self.code}
        self.seg_fixups = {i: {} for i in self.code}  # site -> segment, for selector-only fixups
        self.far_targets = set()
        for i in self.code:
            for r in ne.relocations(i):
                if r["kind"] == "internal":
                    seg, off = r["target"]
                    desc = f"s{seg:02d}:{off:04x}"
                    if r["addr_type"] == 3 and seg in self.code:
                        self.far_targets.add((seg, off))
                elif r["kind"] == "import":
                    mod, ordinal = r["target"]
                    name = ordinal if isinstance(ordinal, str) else self.api[mod].get(ordinal, f"@{ordinal}")
                    desc = f"{mod}.{name}"
                else:
                    continue
                if r["addr_type"] == 2:
                    desc = "seg " + desc.split(":")[0]
                for site in r["sites"]:
                    self.fixups[i][site] = desc
                    if r["kind"] == "internal" and r["addr_type"] == 2:
                        self.seg_fixups[i][site] = r["target"][0]
        self.entry = (cs, ip)
        self.exports = {(s, o): ne.names.get(n, f"ord{n}") for n, (s, o) in ne.entries.items()}

    def string_at(self, off):
        """The NUL-terminated string starting at DS:off, if there is one."""
        d = self.data
        printable = lambda c: 32 <= c < 127 or c in (9, 10, 13)
        if not 0 < off < len(d) or printable(d[off - 1]):
            return None  # not the start of a string
        end = off
        while end < len(d) and printable(d[end]):
            end += 1
        if end - off < 3 or end >= len(d) or d[end] != 0:
            return None
        return d[off:end].decode("latin-1")

    # Borland C++ function prologues, for code only reached through pointers
    # (window procedures, callbacks, virtual functions).
    _PROLOGUES = re.compile(b"|".join(re.escape(bytes.fromhex(h)) for h in (
        "8cd8904555 8bec",   # mov ax, ds / nop / inc bp / push bp / mov bp, sp (exported far)
        "1e58904555 8bec",   # push ds / pop ax / nop / inc bp / ...
        "45558bec 1e8ed8",   # inc bp / push bp / mov bp, sp / push ds / mov ds, ax (far)
        "558bec",            # push bp / mov bp, sp (near)
    )))

    def disassemble(self):
        self.insns = {i: {} for i in self.code}
        self.labels = {i: {} for i in self.code}
        self.funcs = set()
        self._walk([self.entry, *self.exports, *self.far_targets])
        while True:  # then functions found by prologue in the gaps, until none are left
            found = []
            for seg, code in self.code.items():
                covered = set()
                for i in self.insns[seg].values():
                    covered.update(range(i.address, i.address + i.size))
                for m in self._PROLOGUES.finditer(code):
                    if m.start() not in covered:
                        found.append((seg, m.start()))
            found = [(s, o) for s, o in found if o not in self.insns[s]]
            if not found:
                break
            self._walk(found, guessed=True)

    def _walk(self, todo, guessed=False):
        md = Cs(CS_ARCH_X86, CS_MODE_16)
        for seg, off in todo:
            if seg in self.code:
                self.funcs.add((seg, off))
                default = f"{'g' if guessed else 'f'}{seg:02d}_{off:04x}"
                self.labels[seg].setdefault(off, self.exports.get((seg, off), default))
        while todo:
            seg, pc = todo.pop()
            if seg not in self.code:
                continue
            code, insns, labels = self.code[seg], self.insns[seg], self.labels[seg]
            recent = []
            while 0 <= pc < len(code) and pc not in insns:
                i = next(md.disasm(code[pc:pc + 16], pc), None)
                if i is None:
                    break
                insns[pc] = i
                recent = (recent + [i])[-5:]
                ops = i.op_str
                is_branch = i.mnemonic.startswith("j") or i.mnemonic in ("call", "loop", "loope", "loopne")
                if is_branch and re.fullmatch(r"0x[0-9a-f]+", ops):
                    t = int(ops, 16)
                    if i.mnemonic == "call":
                        self.funcs.add((seg, t))
                        labels.setdefault(t, f"f{seg:02d}_{t:04x}")
                    else:
                        labels.setdefault(t, f"l{seg:02d}_{t:04x}")
                    todo.append((seg, t))
                elif i.mnemonic in ("lcall", "ljmp") and i.bytes[0] in (0x9A, 0xEA)                         and pc + 3 in self.seg_fixups[seg]:
                    # far call within the program: offset in the instruction,
                    # segment filled in by a selector fixup
                    tseg = self.seg_fixups[seg][pc + 3]
                    (toff,) = struct.unpack_from("<H", i.bytes, 1)
                    if tseg in self.code:
                        self.funcs.add((tseg, toff))
                        self.labels[tseg].setdefault(toff, f"f{tseg:02d}_{toff:04x}")
                        self.fixups[seg][pc + 3] = self.labels[tseg][toff]
                        todo.append((tseg, toff))
                elif i.mnemonic == "jmp" and "cs:[bx + 0x" in ops:
                    todo += [(seg, t) for t in self._switch(seg, i, recent)]
                if i.mnemonic in _FLOW_END:
                    break
                pc += i.size

    def _switch(self, seg, i, recent):
        """Borland switch: cmp bx, N / ja default / shl bx, 1 / jmp cs:[bx + table]."""
        count = None
        for p in reversed(recent[:-1]):
            m = re.fullmatch(r"(bx|ax|cx|dx|si|di), 0x([0-9a-f]+)|(bx|ax|cx|dx|si|di), (\d+)", p.op_str)
            if p.mnemonic == "cmp" and m:
                count = int(m.group(2), 16) + 1 if m.group(2) else int(m.group(4)) + 1
                break
        if count is None or count > 256:
            return []
        table = int(re.search(r"cs:\[bx \+ 0x([0-9a-f]+)\]", i.op_str).group(1), 16)
        code = self.code[seg]
        targets = []
        for k in range(count):
            if table + 2 * k + 2 > len(code):
                break
            (t,) = struct.unpack_from("<H", code, table + 2 * k)
            if t >= len(code):
                break
            self.labels[seg].setdefault(t, f"l{seg:02d}_{t:04x}")
            targets.append(t)
        self.tables = getattr(self, "tables", {})
        self.tables[(seg, table)] = len(targets)
        return targets

    def annotate(self, seg, i):
        notes = []
        for site in range(i.address, i.address + i.size):
            if site in self.fixups[seg]:
                notes.append(self.fixups[seg][site])
        if not notes:
            for m in _IMM.finditer(i.op_str):
                s = self.string_at(int(m.group(1), 16))
                if s is not None:
                    notes.append(repr(s[:60]))
        return notes

    def write(self):
        OUT.mkdir(parents=True, exist_ok=True)
        stem = self.path.stem.lower()
        tables = getattr(self, "tables", {})
        summary = []
        with (OUT / f"{stem}.asm").open("w", encoding="utf-8") as f:
            for seg in sorted(self.code):
                code, insns, labels = self.code[seg], self.insns[seg], self.labels[seg]
                f.write(f"\n;;;;;;;;;; segment {seg} ({len(code):#x} bytes)\n")
                pc, func = 0, None
                while pc < len(code):
                    if pc in labels:
                        if (seg, pc) in self.funcs:
                            func = [f"{labels[pc]}", []]
                            summary.append(func)
                            f.write(f"\n; ---------------- function\n")
                        f.write(f"{labels[pc]}:\n")
                    if pc in insns:
                        i = insns[pc]
                        notes = self.annotate(seg, i)
                        ops = i.op_str
                        for m in re.finditer(r"0x([0-9a-f]+)", ops) if i.mnemonic.startswith(("j", "call", "loop")) else []:
                            t = int(m.group(1), 16)
                            if t in labels:
                                ops = ops.replace(m.group(0), labels[t])
                        f.write(f"{seg:02d}:{pc:04x}: {i.bytes.hex():<14} {i.mnemonic} {ops}"
                                f"{'  ; ' + ', '.join(notes) if notes else ''}\n")
                        if func is not None:
                            func[1] += [n for n in notes if n not in func[1]]
                        pc += i.size
                    elif (seg, pc) in tables:
                        n = tables[(seg, pc)]
                        for k in range(n):
                            (t,) = struct.unpack_from("<H", code, pc + 2 * k)
                            f.write(f"{seg:02d}:{pc + 2 * k:04x}: dw {labels.get(t, hex(t))}  ; case {k}\n")
                        pc += 2 * n
                    else:
                        end = pc + 1
                        while end < len(code) and end not in insns and end not in labels and end - pc < 16:
                            end += 1
                        f.write(f"{seg:02d}:{pc:04x}: db {code[pc:end].hex(' ')}\n")
                        pc = end
        with (OUT / f"{stem}.funcs.txt").open("w", encoding="utf-8") as f:
            for name, notes in summary:
                f.write(f"{name}: {'  '.join(notes)}\n")
        n = sum(len(v) for v in self.insns.values())
        total = sum(len(c) for c in self.code.values())
        covered = sum(i.size for v in self.insns.values() for i in v.values())
        print(f"{self.path.name}: {len(self.funcs)} functions, {n} instructions, "
              f"{100 * covered / total:.0f}% of code bytes reached -> {OUT / stem}.asm")


def main():
    for path in sys.argv[1:]:
        p = Program(path)
        p.disassemble()
        p.write()


if __name__ == "__main__":
    main()
