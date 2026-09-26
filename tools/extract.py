"""Unpack Artech archives (*.D01, GRAFX.DAT) into extracted/<archive>/.

Archive layout (little-endian):
    [optional ASCII title, e.g. "BITWIT Data File\\r\\n"] 0x1A
    u16 count
    count x { u16 id; u32 offset; u32 packed_size; u32 unpacked_size }
    entry data, each a PKWARE DCL implode stream

The high byte of id appears to be a resource group, the low byte an index.

Usage: python tools/extract.py [archive ...]
"""
import struct
import sys
from collections import Counter
from pathlib import Path

from dcl import DCLError, explode

ROOT = Path(__file__).resolve().parent.parent
CD = ROOT / "original" / "cd" / "DSK3"
OUT = ROOT / "extracted"
DEFAULT = ["SHELL.D01", "RB.D01", "MYSTERY.D01", "GRAFX.DAT"]


def read_directory(data):
    pos = data.index(b"\x1a") + 1
    (count,) = struct.unpack_from("<H", data, pos)
    pos += 2
    return [struct.unpack_from("<HIII", data, pos + i * 14) for i in range(count)]


def sniff(blob):
    if blob[:2] == b"BM":
        return "bmp"
    if blob[:4] == b"RIFF":
        return "wav"
    return "bin"


def extract(path):
    data = path.read_bytes()
    entries = read_directory(data)
    outdir = OUT / path.stem.lower()
    outdir.mkdir(parents=True, exist_ok=True)
    kinds, bad = Counter(), []
    for key, offset, packed, unpacked in entries:
        raw = data[offset:offset + packed]
        try:
            blob = explode(raw)
        except DCLError:
            blob = raw  # a handful of entries may be stored uncompressed
        if len(blob) != unpacked:
            bad.append((key, len(blob), unpacked))
            continue
        ext = sniff(blob)
        kinds[ext] += 1
        (outdir / f"{key:04x}.{ext}").write_bytes(blob)
    print(f"{path.name}: {len(entries)} entries, {len(bad)} failed, types {dict(kinds)}")
    for key, got, want in bad[:10]:
        print(f"  !! {key:04x}: got {got} bytes, expected {want}")
    return not bad


def main():
    names = sys.argv[1:] or DEFAULT
    ok = all([extract(CD / n) for n in names])
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
