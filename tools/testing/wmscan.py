"""Wild Science's coverage scan (as Mystery's was, docs/MYSTERY.md): each
function of WMAIN.EXE's own code (not its library and run time, see
tools/progress.py's LIBRARY) that neither the port's source nor
docs/SCIENCE.md cites, with its size, what refers to it (near and far
calls, far pointers, the classes' vtables: tools/wmclasses.py) and
whether anything cited reaches it.

  python tools/testing/wmscan.py [--segment N] [--unreached] [--all]

Out: one line a function, "f05_0244 810 bytes reached by f05_0189
(vtable magnetpole +18/7)", grouped by segment, with each segment's
uncited bytes; a summary last. Needs tools/nedis.py's and wmclasses.py's
output (extracted/disasm/wmain.*).
"""
import argparse
import re
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import progress as P  # noqa: E402

ASM = P.DISASM / "wmain.asm"
CLASSES = P.DISASM / "wmain.classes.txt"


def references(funcs, find):
    """{function: {(referrer, how)}} from the disassembly and the classes."""
    refs = defaultdict(set)
    current = None
    for line in ASM.read_text(encoding="utf-8", errors="replace").splitlines():
        m = re.match(r"([fg])([0-9]{2})_([0-9a-f]{4}):$", line)
        if m:
            current = find(int(m.group(2)), int(m.group(3), 16))
            continue
        for m in re.finditer(r"\bcall [fgl]([0-9]{2})_([0-9a-f]{4})\b", line):
            t = find(int(m.group(1)), int(m.group(2), 16))
            if t and t != current:
                refs[t].add((current, "call"))
        m = re.search(r"; s([0-9]{2}):([0-9a-f]{4})", line)
        if m:
            t = find(int(m.group(1)), int(m.group(2), 16))
            if t and t != current:
                refs[t].add((current, "far call" if "lcall" in line else "far pointer"))
    cls = None
    for line in CLASSES.read_text(encoding="utf-8", errors="replace").splitlines():
        m = re.match(r"\s*(\w+) \(", line)
        if m and ":" in line and "=" in line:
            cls = m.group(1)
        for m in re.finditer(r"\b([0-9]{2})_([0-9a-f]{4})\b", line):
            t = find(int(m.group(1)), int(m.group(2), 16))
            if t:
                refs[t].add((None, f"class {cls}"))
    return refs


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--segment", type=int, action="append", help="only these segments")
    ap.add_argument("--unreached", action="store_true", help="only the functions nothing reaches")
    ap.add_argument("--all", action="store_true", help="the cited ones too")
    opts = ap.parse_args()
    funcs = P.functions("WMAIN.EXE")
    find = P.locate(funcs)
    src = P.source_lines(P.worktree_lines())["science"]
    ported = P.cited(src, find)
    documented = P.cited((P.ROOT / "docs/SCIENCE.md").read_text(encoding="utf-8").splitlines(), find)
    refs = references(funcs, find)
    lib = P.LIBRARY["science"]
    own = sorted(s for s in funcs if s not in lib)
    # Reached: from the cited functions and the classes, through references
    # (a function reaches what it calls; a class reaches its methods).
    reached = set()
    stack = [n for n in ported | documented]
    stack += [t for t, rs in refs.items() if any(r is None for r, _ in rs)]
    calls = defaultdict(set)
    for t, rs in refs.items():
        for r, _ in rs:
            if r:
                calls[r].add(t)
    while stack:
        f = stack.pop()
        if f in reached:
            continue
        reached.add(f)
        stack += calls.get(f, ())
    total_bytes = left_bytes = left_reached = 0
    counts = defaultdict(int)
    for seg in own:
        if opts.segment and seg not in opts.segment:
            continue
        rows = []
        for _, size, name in funcs[seg]:
            total_bytes += size
            status = "ported" if name in ported else "documented" if name in documented else None
            if status and not opts.all:
                continue
            if not status:
                left_bytes += size
                counts["uncited"] += 1
                if name in reached:
                    left_reached += size
            is_reached = name in reached
            if opts.unreached and is_reached:
                continue
            how = sorted({f"{h} {r}" if r else h for r, h in refs.get(name, ())})
            rows.append(f"  {name} {size} bytes{' ' + status if status else ''}"
                        f"{'' if is_reached else ' UNREACHED'}: {', '.join(how[:6]) or 'no references'}")
        if rows:
            seg_left = sum(s for _, s, n in funcs[seg] if n not in ported and n not in documented)
            print(f"segment {seg}: {seg_left} bytes uncited of {sum(s for _, s, _ in funcs[seg])}")
            print("\n".join(rows))
    print(f"\n{counts['uncited']} functions uncited, {left_bytes} bytes of {total_bytes} "
          f"({100 * left_bytes / max(total_bytes, 1):.1f}%); {left_reached} bytes of them reached")


if __name__ == "__main__":
    main()
