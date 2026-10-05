"""thunks.py SEG OFF...: where the method-table thunks at those offsets of WMAIN.EXE segment SEG jump (by the relocations)."""
import sys; sys.path.insert(0,__import__('os').path.join(__import__('os').path.dirname(__import__('os').path.abspath(__file__)),'..')); from ne import NEFile
import os
wmain = os.environ.get('WMAIN') or (os.environ.get('EDISON_RUN') and os.path.join(os.environ['EDISON_RUN'], 'WMAIN.EXE'))
if not wmain:
    sys.exit("set EDISON_RUN to the folder with the game files (or WMAIN to WMAIN.EXE)")
e=NEFile(wmain)
seg=int(sys.argv[1]); offs=[int(x,16) for x in sys.argv[2:]]
site={}
for r in e.relocations(seg):
    for s in r['sites']: site[s]=r['target']
data=e.segment_bytes(seg)
for o in offs:
    # thunk: 8b dc 36 81/83 47 04 imm ea <off> <seg> (this += imm, then
    # a far jump); the imm can itself be EAh, so decode by the opcode.
    if data[o:o+3] == b'\x8b\xdc\x36' and data[o+3] in (0x81, 0x83):
        n = 2 if data[o+3] == 0x81 else 1
        adj = int.from_bytes(data[o+6:o+6+n], 'little', signed=True)
        i = o + 6 + n
    else:
        adj = None; i = data.index(b'\xea', o)
    t=site.get(i+1)
    note = '' if adj is None else ' (this %+d)' % adj
    print('%04x -> f%02d_%04x%s' % (o, t[0], t[1], note) if t and isinstance(t[0],int) else (hex(o), t))
