"""PKWARE Data Compression Library (DCL) "implode" decompressor.

Pure-Python port of Mark Adler's blast.c (zlib/contrib/blast). Artech's
engine stores every archive entry in this format.
"""

MAXBITS = 13

# Compact code-length tables from blast.c: each byte is (repeat-1) << 4 | length.
_LITLEN = bytes([
    11, 124, 8, 7, 28, 7, 188, 13, 76, 4, 10, 8, 12, 10, 12, 10, 8, 23, 8,
    9, 7, 6, 7, 8, 7, 6, 55, 8, 23, 24, 12, 11, 7, 9, 11, 12, 6, 7, 22, 5,
    7, 24, 6, 11, 9, 6, 7, 22, 7, 11, 38, 7, 9, 8, 25, 11, 8, 11, 9, 12,
    8, 12, 5, 38, 5, 38, 5, 11, 7, 5, 6, 21, 6, 10, 53, 8, 7, 24, 10, 27,
    44, 253, 253, 253, 252, 252, 252, 13, 12, 45, 12, 45, 12, 61, 12, 45,
    44, 173])
_LENLEN = bytes([2, 35, 36, 53, 38, 23])
_DISTLEN = bytes([2, 20, 53, 230, 247, 151, 248])
_BASE = (3, 2, 4, 5, 6, 7, 8, 9, 10, 12, 16, 24, 40, 72, 136, 264)
_EXTRA = (0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8)


class DCLError(Exception):
    pass


class _Huffman:
    def __init__(self, compact):
        lengths = []
        for b in compact:
            lengths += [b & 15] * ((b >> 4) + 1)
        self.count = [0] * (MAXBITS + 1)
        for n in lengths:
            self.count[n] += 1
        offs = [0] * (MAXBITS + 1)
        for n in range(1, MAXBITS):
            offs[n + 1] = offs[n] + self.count[n]
        self.symbol = [0] * len(lengths)
        for sym, n in enumerate(lengths):
            if n:
                self.symbol[offs[n]] = sym
                offs[n] += 1


_LITCODE = _Huffman(_LITLEN)
_LENCODE = _Huffman(_LENLEN)
_DISTCODE = _Huffman(_DISTLEN)


class _Bits:
    def __init__(self, data, pos):
        self.data = data
        self.pos = pos
        self.buf = 0
        self.cnt = 0

    def bits(self, need):
        while self.cnt < need:
            if self.pos >= len(self.data):
                raise DCLError("unexpected end of input")
            self.buf |= self.data[self.pos] << self.cnt
            self.pos += 1
            self.cnt += 8
        val = self.buf & ((1 << need) - 1)
        self.buf >>= need
        self.cnt -= need
        return val

    def decode(self, h):
        # DCL stores Huffman codes bit-inverted.
        code = first = index = 0
        for n in range(1, MAXBITS + 1):
            code |= self.bits(1) ^ 1
            count = h.count[n]
            if code < first + count:
                return h.symbol[index + code - first]
            index += count
            first = (first + count) << 1
            code <<= 1
        raise DCLError("invalid Huffman code")


def explode(data, pos=0):
    """Decompress one DCL stream starting at data[pos]; returns bytes."""
    s = _Bits(data, pos)
    lit = s.bits(8)
    if lit > 1:
        raise DCLError(f"bad literal flag {lit}")
    dictbits = s.bits(8)
    if not 4 <= dictbits <= 6:
        raise DCLError(f"bad dictionary size {dictbits}")
    out = bytearray()
    while True:
        if s.bits(1):
            sym = s.decode(_LENCODE)
            length = _BASE[sym] + s.bits(_EXTRA[sym])
            if length == 519:  # end-of-stream marker
                return bytes(out)
            shift = 2 if length == 2 else dictbits
            dist = (s.decode(_DISTCODE) << shift) + s.bits(shift) + 1
            if dist > len(out):
                raise DCLError("distance too far back")
            start = len(out) - dist
            for i in range(length):  # byte-wise: copies may overlap
                out.append(out[start + i])
        else:
            out.append(s.decode(_LITCODE) if lit else s.bits(8))
