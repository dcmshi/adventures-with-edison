"""Dump CodeView 4 (NB09) debug symbols left in the shipped executables.

Artech's DLLs still carry their debug info, which names procedures, globals
and songs with their segment:offset. Output: extracted/symbols/<file>.txt
lines "SSSS:OOOO  kind  name" sorted by address, plus module (source) names.
(Git-ignored: derived from game files.)

Usage: python tools/cvsyms.py FILE [FILE ...]
"""
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

SST_MODULE = 0x120
SST_SEGMAP = 0x12D
SYMBOL_SECTIONS = {0x122: "public", 0x123: "publicsym", 0x124: "symbols", 0x125: "alignsym",
                   0x129: "globalsym", 0x12A: "globalpub", 0x134: "staticsym"}

# 16-bit symbol records we care about: type -> (kind, offset of seg:off, offset of name)
S_DATA_LIKE = {0x0101: "ldata", 0x0102: "gdata", 0x0103: "public", 0x0109: "label"}
S_PROC = {0x0104: "lproc", 0x0105: "gproc"}


def pascal(buf, pos):
    n = buf[pos]
    return buf[pos + 1:pos + 1 + n].decode("latin-1")


def parse(path):
    data = Path(path).read_bytes()
    if data[-8:-6] != b"NB":
        raise ValueError("no CodeView signature at end of file")
    sig = data[-8:-4].decode()
    (size,) = struct.unpack("<I", data[-4:])
    base = len(data) - size
    cv = data[base:]
    if cv[:4].decode() != sig or sig != "NB09":
        raise ValueError(f"unsupported CodeView format {sig}")
    (lfo_dir,) = struct.unpack_from("<I", cv, 4)
    cb_hdr, cb_entry, count = struct.unpack_from("<HHI", cv, lfo_dir)
    modules, symbols, segmap = {}, [], {}
    for i in range(count):
        sst, imod, lfo, cb = struct.unpack_from("<HHII", cv, lfo_dir + cb_hdr + i * cb_entry)
        sub = cv[lfo:lfo + cb]
        if sst == SST_MODULE:
            _ovl, _lib, cseg, _style = struct.unpack_from("<HHHH", sub, 0)
            modules[imod] = pascal(sub, 8 + cseg * 12)
        elif sst == SST_SEGMAP:
            # logical segment n -> (physical NE segment, offset within it)
            (cseg,) = struct.unpack_from("<H", sub, 0)
            for n in range(cseg):
                _flags, _ovl, _grp, frame, _nm, _cls, off, _cb = struct.unpack_from("<HHHHHHII", sub, 4 + n * 20)
                segmap[n + 1] = (frame, off)
        elif sst in SYMBOL_SECTIONS:
            start = 4 if sst in (0x124, 0x125) else 0  # module symbol tables begin with a signature
            if sst in (0x129, 0x12A, 0x134):
                start = 8  # global tables: symhash, addrhash, cbSymbol header
            symbols += [(s, modules.get(imod, "")) for s in _records(sub, start)]
    # Translate logical seg:off to physical NE segment:offset.
    phys = []
    for (seg, off, kind, name), module in symbols:
        frame, base = segmap.get(seg, (seg, 0))
        phys.append(((frame, base + off, kind, name), module))
    return sig, modules, phys


def load_symbols(path):
    """{(ne_segment, offset): name} for use by other tools."""
    return {(seg, off): name for (seg, off, _k, name), _m in parse(path)[2]}


def _records(sub, pos):
    while pos + 4 <= len(sub):
        reclen, rectyp = struct.unpack_from("<HH", sub, pos)
        if reclen < 2:
            break
        body = pos + 4
        if rectyp in S_DATA_LIKE:
            off, seg = struct.unpack_from("<HH", sub, body)
            name_at = body + (4 if rectyp == 0x0109 else 6)
            if rectyp == 0x0109:
                name_at += 1  # label flags byte
            yield (seg, off, S_DATA_LIKE[rectyp], pascal(sub, name_at))
        elif rectyp in S_PROC:
            off, seg = struct.unpack_from("<HH", sub, body + 12 + 6)
            yield (seg, off, S_PROC[rectyp], pascal(sub, body + 12 + 6 + 4 + 2 + 1))
        pos += reclen + 2


def main():
    for path in sys.argv[1:]:
        sig, modules, symbols = parse(path)
        uniq = sorted(set(symbols), key=lambda s: (s[0][0], s[0][1], s[0][3]))
        out = ROOT / "extracted" / "symbols" / f"{Path(path).stem.lower()}.txt"
        out.parent.mkdir(parents=True, exist_ok=True)
        with out.open("w") as f:
            f.write(f"# {Path(path).name} ({sig}), modules: {', '.join(sorted(set(modules.values())))}\n")
            for (seg, off, kind, name), module in uniq:
                f.write(f"{seg:04x}:{off:04x}  {kind:<7} {name:<32} {module}\n")
        print(f"{out}  ({len(uniq)} symbols, {len(modules)} modules)")


if __name__ == "__main__":
    main()
