"""Disassemble a code segment of an NE executable with exported names as labels.

Output goes to extracted/disasm/ (git-ignored: it is derived from game code).

Usage: python tools/disasm.py original/cd/DSK3/ADLIB.DLL [segment=1]
"""
import sys
from pathlib import Path

from capstone import CS_ARCH_X86, CS_MODE_16, Cs

from ne import NEFile

ROOT = Path(__file__).resolve().parent.parent


def main():
    path = Path(sys.argv[1])
    seg = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    ne = NEFile(path)
    labels = {off: ne.names.get(o, f"ord{o}") for o, (s, off) in ne.entries.items() if s == seg}
    out = ROOT / "extracted" / "disasm" / f"{path.stem.lower()}_seg{seg}.asm"
    out.parent.mkdir(parents=True, exist_ok=True)
    md = Cs(CS_ARCH_X86, CS_MODE_16)
    with out.open("w") as f:
        for i in md.disasm(ne.segment_bytes(seg), 0):
            if i.address in labels:
                f.write(f"\n{labels[i.address]}:\n")
            f.write(f"{i.address:04x}: {i.bytes.hex():<14} {i.mnemonic} {i.op_str}\n")
    print(out)


if __name__ == "__main__":
    main()
