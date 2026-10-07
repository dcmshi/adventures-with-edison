"""calltrace.py NAME...: a straight-line reading of a function's far calls
with their arguments resolved (constants, [bp-N] words set before, the
address of a [bp-N] pair as &(x, y), earlier calls' results as #n), for
transcribing builders: the rooms' own objects and box looks. Branches are
read straight through (both sides)."""
import re
import sys

import wm


def trace(name):
    seg, off = wm.parse(name)
    words, stack, ax, n = {}, [], None, 0
    out = []
    for line in wm.disassemble(seg, off):
        asm = line.split(None, 2)[2] if len(line.split(None, 2)) > 2 else ""
        if m := re.match(r"mov word \[bp-(0x[0-9a-f]+)\],(0x[0-9a-f]+)$", asm):
            words[-int(m.group(1), 16)] = int(m.group(2), 16)
        elif m := re.match(r"mov \[bp-(0x[0-9a-f]+)\],ax$", asm):
            words[-int(m.group(1), 16)] = ax
        elif m := re.match(r"mov ax,\[bp-(0x[0-9a-f]+)\]$", asm):
            ax = words.get(-int(m.group(1), 16), "?")
        elif m := re.match(r"mov ax,(0x[0-9a-f]+)$", asm):
            ax = int(m.group(1), 16)
        elif m := re.match(r"lea ax,\[bp-(0x[0-9a-f]+)\]$", asm):
            k = -int(m.group(1), 16)
            ax = ("&", words.get(k, "?"), words.get(k + 2, "?"))
        elif asm == "push ax":
            stack.append(ax)
        elif m := re.match(r"push word (0x[0-9a-f]+)$", asm):
            stack.append(int(m.group(1), 16) & 0xFFFF)
        elif m := re.match(r"push word \[bp-(0x[0-9a-f]+)\]$", asm):
            stack.append(words.get(-int(m.group(1), 16), "?"))
        elif m := re.match(r"push word \[bp\+0x6\]$", asm):
            stack.append("this")
        elif m := re.match(r"push word \[bx\+(0x[0-9a-f]+)\]$", asm):
            stack.append(f"this+{m.group(1)}")
        elif m := re.match(r"call (?:far )?(f\d+_[0-9a-f]+)$", asm):
            n += 1
            args = list(reversed(stack))
            stack.clear()
            fmt = []
            for a in args:
                if isinstance(a, tuple):
                    fmt.append(f"&({fx(a[1])}, {fx(a[2])})")
                else:
                    fmt.append(fx(a))
            out.append(f"#{n} = {m.group(1)}({', '.join(fmt)})")
            ax = f"#{n}"
        elif m := re.match(r"mov \[bx\+(0x[0-9a-f]+)\],ax$", asm):
            out.append(f"   [obj+{m.group(1)}] = {fx(ax)}")
        elif m := re.match(r"mov \[bx\+(0x[0-9a-f]+)\],(0x[0-9a-f]+)$", asm):
            pass
    return out


def fx(v):
    if isinstance(v, int):
        v &= 0xFFFF
        v = v - 0x10000 if v >= 0x8000 else v
        return str(v) if -10 < v < 10 else (hex(v) if v >= 0 else str(v))
    return str(v)


if __name__ == "__main__":
    for name in sys.argv[1:]:
        print(f"== {name}")
        print("\n".join(trace(name)))
