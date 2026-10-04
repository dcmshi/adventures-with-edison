"""tools/testing/roompics.py [--cpp | --config] [--dump DGROUP.bin]: each table room's method 4 (its pictures): from the dispatcher f31_0783's
switch, the builder, the method table it sets at +5E, its +10 entry (from
the memory dump), then show_Clogo calls (FUN_1068_1179) with constants."""
import re, struct, sys

src = open('extracted/ghidra/wmain.c', encoding='utf-8', errors='replace').read()
funcs = {}
for m in re.finditer(r'^// ==== FUN_([0-9a-f]{4})_([0-9a-f]{4}) @', src, re.M):
    funcs[(m.group(1), m.group(2))] = m.start()
starts = sorted(funcs.values())

def body(sel, off):
    a = funcs.get((sel, off))
    if a is None:
        return None
    i = starts.index(a)
    return src[a:starts[i + 1] if i + 1 < len(starts) else len(src)]

disp = body('10f0', '0786')
cases = re.findall(r'case (0x[0-9a-f]+|\d+):\s*\n\s*uVar2 = FUN_([0-9a-f]{4})_([0-9a-f]{4})\(', disp)
# A dump of the original's DGROUP (memwatch.py dump) for the method tables'
# far pointers, resolved at run time.
dump = open(sys.argv[sys.argv.index('--dump') + 1] if '--dump' in sys.argv else 'build/scratch/dg1.bin', 'rb').read()
out = {}
for c, sel, off in cases:
    room = int(c, 0)
    b = body(sel, off)
    vt = re.search(r'\+ 0x5e\) = (0x[0-9a-f]+);', b or '')
    if not vt:
        print(room, 'no vtable', sel, off); continue
    t = int(vt.group(1), 16)
    o, s = struct.unpack_from('<HH', dump, t + 0x10)
    seg = (s - 0x11f7) // 8
    msel = '%04x' % (0x1000 + 8 * (seg - 1))
    # Ghidra may start the function 3 bytes later (the prologue).
    mb = body(msel, '%04x' % o) or body(msel, '%04x' % (o + 3))
    if mb is None:
        print(room, 'method 4 f%02d_%04x not found' % (seg, o)); continue
    vals = {}
    pics = []
    other = []
    for line in mb.split('\n'):
        a = re.match(r'\s*(\w+) = (0x[0-9a-f]+|-?\d+);', line)
        if a:
            vals[a.group(1)] = int(a.group(2), 0)
            continue
        cl = re.search(r'FUN_1068_1179\(0x[0-9a-f]+,&(\w+),(0x[0-9a-f]+|\d+)\)', line)
        cs = re.search(r'FUN_1068_1179\(0x[0-9a-f]+,&(\w+),\(char \*\)s_\w+_1330_([0-9a-f]+)(?: \+ (0x[0-9a-f]+|\d+))?\)', line)
        if cs:
            v = cs.group(1)
            n = int(re.search(r'_([0-9a-f]+)$', v).group(1), 16)
            y = vals.get(re.sub(r'_[0-9a-f]+$', '_%x' % (n - 2), v))
            pics.append((vals.get(v), y, int(cs.group(2), 16) + int(cs.group(3) or '0', 0)))
            continue
        if cl:
            v = cl.group(1)
            n = int(re.search(r'_([0-9a-f]+)$', v).group(1), 16)
            y = vals.get(re.sub(r'_[0-9a-f]+$', '_%x' % (n - 2), v))
            pics.append((vals.get(v), y, int(cl.group(2), 0)))
            continue
        f = re.findall(r'FUN_([0-9a-f]{4}_[0-9a-f]{4})', line)
        for x in f:
            if x not in ('10d0_0e5b', '1068_0af1', '1068_1179'):
                other.append(x)
    out[room] = (pics, other, 'f%02d_%04x' % (seg, o))
if '--cpp' in sys.argv:
    print("// Generated from each room's method 4 (tools/testing/roompics.py): its")
    print('// pictures (x, y, id) on screen 3.')
    for r in sorted(out):
        pics, other, name = out[r]
        if pics:
            print('    {%d, {%s}},  // %s' % (r, ', '.join('{%d, %d, 0x%04X}' % p for p in pics), name))
elif '--config' not in sys.argv:
    for r in sorted(out):
        pics, other, name = out[r]
        bad = [p for p in pics if None in p]
        print(r, name, pics, 'BAD' if bad else '')

# Each builder's settings: [30C] (the targets' shot bonus on), +F7B (the
# completion bonus), the +F8D and +F87 tables (from a shot count on).
if '--config' in sys.argv:
    print('// Generated from each room\'s builder (tools/testing/roompics.py):')
    print('// the shot bonus on ([30C]), the completion bonus (+F7B), the')
    print('// targets\' bonus by shots (+F8D) and the completion\'s (+F87).')
    for c, sel, off in cases:
        room = int(c, 0)
        b = body(sel, off) or ''
        on = 1 if re.search(r'\*\(undefined2 \*\)0x30c = 1;', b) else 0
        m7 = re.search(r'\+ 0xf7b\) = (0x[0-9a-f]+|\d+);', b)
        f7b = int(m7.group(1), 0) if m7 else 1500
        f8d = [128] * 6
        f87 = [128] * 6
        for fn, v, fr in re.findall(r'FUN_10d0_(088b|0859)\([^,]+,param_2,(0x[0-9a-f]+|\d+),(\d+)\)', b):
            t = f8d if fn == '088b' else f87
            for k in range(int(fr), 6):
                t[k] = int(v, 0)
        if on or f7b != 1500 or f8d != [128] * 6 or f87 != [128] * 6:
            print('    {%d, %s, %d, {%s}, {%s}},' % (room, 'true' if on else 'false', f7b, ', '.join(map(str, f8d)), ', '.join(map(str, f87))))
