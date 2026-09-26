"""Recursive-descent disassembler for a code segment of an NE executable.

Starts from exported entry points, far-call thunk targets and any extra
addresses given, follows jumps/calls, and prints unreached bytes as data,
so tables embedded in code don't desynchronise the listing.

Output goes to extracted/disasm/ (git-ignored: it is derived from game code).

Usage: python tools/disasm.py FILE [--seg N] [--entry HEX ...] [--table HEX:COUNT ...]
    --table  a near-pointer jump table (u16 entries) whose targets are code
"""
import argparse
import re
import struct
from pathlib import Path

from capstone import CS_ARCH_X86, CS_MODE_16, Cs

from ne import NEFile

try:
    from cvsyms import load_symbols
except ImportError:  # pragma: no cover
    load_symbols = None

_MEMREF = re.compile(r"\[(?:[a-z]{2} \+ )?0x([0-9a-f]+)\]")

ROOT = Path(__file__).resolve().parent.parent
_TARGET = re.compile(r"^0x([0-9a-f]+)$")
_FLOW_END = {"ret", "retf", "iret", "jmp", "ljmp"}


def disassemble(code, entries, tables):
    md = Cs(CS_ARCH_X86, CS_MODE_16)
    insns, labels = {}, {}
    todo = list(entries)
    for base, count in tables:
        for k in range(count):
            (t,) = struct.unpack_from("<H", code, base + 2 * k)
            labels.setdefault(t, f"op_{k:02x}")
            todo.append(t)
    while todo:
        pc = todo.pop()
        while 0 <= pc < len(code) and pc not in insns:
            i = next(md.disasm(code[pc:pc + 16], pc), None)
            if i is None:
                break
            insns[pc] = i
            m = _TARGET.match(i.op_str)
            is_branch = i.mnemonic.startswith("j") or i.mnemonic in ("call", "loop", "loope", "loopne", "jcxz")
            if m and is_branch:
                t = int(m.group(1), 16)
                labels.setdefault(t, f"loc_{t:04x}" if i.mnemonic != "call" else f"sub_{t:04x}")
                todo.append(t)
            elif i.mnemonic == "lcall" and i.op_str.count(",") == 1:
                # intra-module far call: "lcall seg, off" (seg is a fixup chain)
                t = int(i.op_str.split(",")[1], 16)
                if t < len(code):
                    labels.setdefault(t, f"far_{t:04x}")
                    todo.append(t)
            if i.mnemonic in _FLOW_END:
                break
            pc += i.size
    return insns, labels


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("file")
    ap.add_argument("--seg", type=int, default=1)
    ap.add_argument("--entry", nargs="*", default=[])
    ap.add_argument("--table", nargs="*", default=[])
    args = ap.parse_args()

    path = Path(args.file)
    ne = NEFile(path)
    code = ne.segment_bytes(args.seg)
    names = {off: ne.names.get(o, f"ord{o}") for o, (s, off) in ne.entries.items() if s == args.seg}
    entries = list(names) + [int(e, 16) for e in args.entry]
    tables = [(int(b, 16), int(c, 16)) for b, c in (t.split(":") for t in args.table)]
    symbols = {}
    try:
        symbols = load_symbols(path)
    except (ValueError, TypeError):
        pass  # no CodeView debug info in this file
    code_syms = {off: n for (seg, off), n in symbols.items() if seg == args.seg}
    data_syms = {off: n for (seg, off), n in symbols.items() if seg != args.seg}
    entries += list(code_syms)
    insns, labels = disassemble(code, entries, tables)
    labels.update(names)
    labels.update(code_syms)

    def annotate(i):
        refs = [data_syms.get(int(m.group(1), 16)) for m in _MEMREF.finditer(i.op_str)]
        if i.mnemonic == "mov" and i.op_str.startswith(("si, 0x", "bx, 0x", "di, 0x")):
            refs.append(data_syms.get(int(i.op_str.split("0x")[1], 16)))
        refs = [r for r in refs if r]
        return f"  ; {', '.join(refs)}" if refs else ""

    out = ROOT / "extracted" / "disasm" / f"{path.stem.lower()}_seg{args.seg}.asm"
    out.parent.mkdir(parents=True, exist_ok=True)
    with out.open("w") as f:
        pc = 0
        while pc < len(code):
            if pc in labels:
                f.write(f"\n{labels[pc]}:\n")
            if pc in insns:
                i = insns[pc]
                f.write(f"{pc:04x}: {i.bytes.hex():<14} {i.mnemonic} {i.op_str}{annotate(i)}\n")
                pc += i.size
            else:
                end = pc + 1
                while end < len(code) and end not in insns and end not in labels and end - pc < 16:
                    end += 1
                f.write(f"{pc:04x}: db {code[pc:end].hex(' ')}\n")
                pc = end
    print(f"{out}  ({len(insns)} instructions)")


if __name__ == "__main__":
    main()
