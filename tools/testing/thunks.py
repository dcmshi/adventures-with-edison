"""thunks.py SEG OFF...: where the method-table thunks at those offsets of WMAIN.EXE segment SEG jump (by the relocations)."""
import sys; sys.path.insert(0,__import__('os').path.join(__import__('os').path.dirname(__import__('os').path.abspath(__file__)),'..')); from ne import NEFile
e=NEFile(__import__('os').environ.get('WMAIN', 'D:/tools/edison-run/WMAIN.EXE'))
seg=int(sys.argv[1]); offs=[int(x,16) for x in sys.argv[2:]]
site={}
for r in e.relocations(seg):
    for s in r['sites']: site[s]=r['target']
data=e.segment_bytes(seg)
for o in offs:
    # thunk: 8b dc 36 81/83 47 04 imm ea <off> <seg>
    i=data.index(b'\xea',o)
    t=site.get(i+1)
    print('%04x -> f%02d_%04x' % (o, t[0], t[1]) if t and isinstance(t[0],int) else (hex(o), t))
