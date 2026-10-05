"""vtable.py OFF [COUNT]: the methods of the method table at DS:OFF in WMAIN.EXE's data segment (far pointers, by the relocations)."""
import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
from ne import NEFile
wmain = os.environ.get('WMAIN') or (os.environ.get('EDISON_RUN') and os.path.join(os.environ['EDISON_RUN'], 'WMAIN.EXE'))
if not wmain:
    sys.exit("set EDISON_RUN to the folder with the game files (or WMAIN to WMAIN.EXE)")
e = NEFile(wmain)
site = {}
for r in e.relocations(103):
    for s in r['sites']:
        site[s] = r['target']
data = e.segment_bytes(103)
base = int(sys.argv[1], 16); count = int(sys.argv[2]) if len(sys.argv) > 2 else 16
for i in range(count):
    a = base + 4 * i
    t = site.get(a)
    text = 'f%02d_%04x' % t if t and isinstance(t[0], int) else '%04x:%04x' % (int.from_bytes(data[a + 2:a + 4], 'little'), int.from_bytes(data[a:a + 2], 'little'))
    print('%2d +%02X %s' % (i, 4 * i, text))
