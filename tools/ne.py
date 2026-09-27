"""Minimal parser for 16-bit Windows NE executables (EXE/DLL).

Lists segments and exported entry points, and can return the raw bytes of
a segment so data tables compiled into Artech's DLLs can be inspected.

Usage: python tools/ne.py FILE [FILE ...]
"""
import struct
import sys
from pathlib import Path


class NEFile:
    def __init__(self, path):
        self.path = Path(path)
        self.data = d = self.path.read_bytes()
        if d[:2] != b"MZ":
            raise ValueError("not an MZ executable")
        (self.ne,) = struct.unpack_from("<I", d, 0x3C)
        if d[self.ne:self.ne + 2] != b"NE":
            raise ValueError("not an NE executable")
        h = self.ne
        (self.entry_off, self.entry_len) = struct.unpack_from("<HH", d, h + 0x04)
        (self.seg_count, self.modref_count, self.nonres_len) = struct.unpack_from("<HHH", d, h + 0x1C)
        (self.seg_off, self.rsrc_off, self.resname_off, self.modref_off,
         self.imp_off) = struct.unpack_from("<HHHHH", d, h + 0x22)
        (self.nonres_abs,) = struct.unpack_from("<I", d, h + 0x2C)
        (self.align,) = struct.unpack_from("<H", d, h + 0x32)
        self.segments = self._segments()
        self.names = self._names(h + self.resname_off) | self._names(self.nonres_abs)
        self.entries = self._entries()
        self.imports = self._imports()

    def _segments(self):
        segs = []
        for i in range(self.seg_count):
            off, size, flags, minalloc = struct.unpack_from("<HHHH", self.data, self.ne + self.seg_off + i * 8)
            segs.append({
                "index": i + 1,
                "offset": off << self.align,
                "size": size or (0x10000 if off else 0),
                "flags": flags,
                "data": bool(flags & 1),
                "reloc": bool(flags & 0x100),
                "minalloc": minalloc or 0x10000,
            })
        return segs

    def _names(self, pos):
        """Name tables: length-prefixed strings, each followed by a u16 ordinal."""
        names = {}
        first = True
        while self.data[pos]:
            n = self.data[pos]
            name = self.data[pos + 1:pos + 1 + n].decode("latin-1")
            (ordinal,) = struct.unpack_from("<H", self.data, pos + 1 + n)
            if not first:  # first entry is the module name/description
                names[ordinal] = name
            first = False
            pos += n + 3
        return names

    def _entries(self):
        d, pos, end = self.data, self.ne + self.entry_off, self.ne + self.entry_off + self.entry_len
        entries, ordinal = {}, 1
        while pos < end:
            count, seg = d[pos], d[pos + 1]
            pos += 2
            if count == 0:
                break
            for _ in range(count):
                if seg == 0:
                    pass  # unused ordinal
                elif seg == 0xFF:  # movable: flags, INT 3Fh, seg, offset
                    entries[ordinal] = (d[pos + 3], struct.unpack_from("<H", d, pos + 4)[0])
                    pos += 6 - 3
                else:  # fixed: flags, offset
                    entries[ordinal] = (seg, struct.unpack_from("<H", d, pos + 1)[0])
                pos += 3
                ordinal += 1
        return entries

    def _imports(self):
        mods = []
        for i in range(self.modref_count):
            (o,) = struct.unpack_from("<H", self.data, self.ne + self.modref_off + i * 2)
            p = self.ne + self.imp_off + o
            mods.append(self.data[p + 1:p + 1 + self.data[p]].decode("latin-1"))
        return mods

    def segment_bytes(self, index):
        s = self.segments[index - 1]
        return self.data[s["offset"]:s["offset"] + s["size"]]

    def relocations(self, index):
        """Relocations of a segment as dicts with the patch sites resolved.

        addr_type: 2 = segment selector, 3 = far pointer, 5 = offset
        kind:      "internal" (target = (segment, offset)),
                   "import"   (target = (module name, ordinal or imported name)) or
                   "osfixup"  (target = fixup type: floating-point emulation patches,
                               which the loader applies only without an FPU)
        sites:     offsets in the segment to patch (chains already walked)
        """
        s = self.segments[index - 1]
        if not s["reloc"]:
            return []
        seg = self.segment_bytes(index)
        pos = s["offset"] + s["size"]
        (count,) = struct.unpack_from("<H", self.data, pos)
        out = []
        for i in range(count):
            addr_type, rtype, site, a, b = struct.unpack_from("<BBHHH", self.data, pos + 2 + i * 8)
            kind = rtype & 3
            if kind == 0:
                target = ("internal", (a, b) if a != 0xFF else self.entries[b])
            elif kind == 1:
                target = ("import", (self.imports[a - 1], b))
            elif kind == 2:
                p = self.ne + self.imp_off + b
                target = ("import", (self.imports[a - 1], self.data[p + 1:p + 1 + self.data[p]].decode("latin-1")))
            else:
                target = ("osfixup", a)
            sites = [site]
            if not rtype & 4 and kind != 3:  # non-additive: sites form a linked chain
                while True:
                    (nxt,) = struct.unpack_from("<H", seg, sites[-1])
                    if nxt == 0xFFFF:
                        break
                    sites.append(nxt)
            out.append({"addr_type": addr_type, "kind": target[0], "target": target[1],
                        "additive": bool(rtype & 4), "sites": sites})
        return out

    def export(self, name):
        for ordinal, n in self.names.items():
            if n.upper() == name.upper() and ordinal in self.entries:
                return self.entries[ordinal]
        raise KeyError(name)


def main():
    for path in sys.argv[1:]:
        ne = NEFile(path)
        print(f"== {ne.path.name}  imports: {', '.join(ne.imports)}")
        for s in ne.segments:
            kind = "DATA" if s["data"] else "CODE"
            print(f"  seg {s['index']:2} {kind} file@{s['offset']:#07x} size {s['size']:#06x}"
                  f" alloc {s['minalloc']:#06x}{' reloc' if s['reloc'] else ''}")
        for ordinal in sorted(ne.entries):
            seg, off = ne.entries[ordinal]
            print(f"  @{ordinal:<4} {seg}:{off:04x}  {ne.names.get(ordinal, '')}")


if __name__ == "__main__":
    main()
