"""deadscan.py checked against Ghidra's own references (a second,
independent reading of the code): for each function deadscan finds dead
(unreferenced, a dead chain, table only), every reference Ghidra's analysis
found to it from outside it, and whether that comes from live code (cited,
or reached in deadscan's reading). Any such is a disagreement to look at.

  python tools/testing/ghidrarefs.py [mystery|rockbach|science ...] [--all]

Needs extracted/ghidra/NAME.refs.txt, written from the Ghidra projects by
tools/ghidra/ExportRefs.java (see tools/testing/README.md). Ghidra's
segment S is the executable's (S - 0x1000) / 8 + 1, its offsets the same.
--all also lists the functions both find dead with Ghidra's references from
dead code, and the count of reached ones Ghidra finds no reference to.
"""
import argparse
import re
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import deadscan as D  # noqa: E402
import progress as P  # noqa: E402

ADDR = re.compile(r"([0-9a-f]{4}):([0-9a-f]{4})")
DEAD = ("unreferenced", "dead chain", "table only")


def ghidra_refs(key, funcs):
    """{function: [(referrer function or address, type)]}, outside itself."""
    exe = next(g[2] for g in P.GAMES if g[0] == key)
    path = P.ROOT / "extracted" / "ghidra" / (Path(exe).stem.lower() + ".refs.txt")
    find = P.locate(funcs)
    begin = D.starts(funcs)
    out = defaultdict(list)
    for line in path.read_text(encoding="utf-8").splitlines():
        parts = line.split()
        if len(parts) < 3:
            continue
        to, frm = ADDR.fullmatch(parts[0]), ADDR.fullmatch(parts[1])
        if not to:
            continue
        seg, off = (int(to.group(1), 16) - 0x1000) // 8 + 1, int(to.group(2), 16)
        target = begin.get((seg, off)) or find(seg, off)
        if not target:
            continue
        source = None
        if frm:
            fseg = (int(frm.group(1), 16) - 0x1000) // 8 + 1
            source = find(fseg, int(frm.group(2), 16)) or f"{fseg:02d}:{frm.group(2)}"
        if source != target:
            out[target].append((source or parts[1], parts[2]))
    return out, path


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("games", nargs="*", default=[g[0] for g in P.GAMES])
    ap.add_argument("--all", action="store_true")
    opts = ap.parse_args()
    disagree = 0
    for key in opts.games:
        exe = next(g[2] for g in P.GAMES if g[0] == key)
        funcs = P.functions(exe)
        rows, _ = D.classify(key)
        cls = {r[1]: r[0] for r in rows}
        refs, path = ghidra_refs(key, funcs)
        live = lambda n: n not in cls or cls[n] == "reached"  # noqa: E731 (cited, or reached)
        title = next(g[1] for g in P.GAMES if g[0] == key)
        bad, dead_refs = [], []
        for name, c in cls.items():
            if c not in DEAD:
                continue
            g = refs.get(name, [])
            from_live = [(s, t) for s, t in g if isinstance(s, str) and re.fullmatch(r"[fg]\d\d_[0-9a-f]{4}", s) and live(s)]
            if from_live:
                bad.append((name, c, from_live))
            elif g:
                dead_refs.append((name, c, g))
        quiet = [n for n, c in cls.items() if c == "reached" and not refs.get(n)]
        n_dead = sum(1 for c in cls.values() if c in DEAD)
        print(f"{title} ({path.name}): {n_dead} dead in deadscan; Ghidra refers to {len(bad)} of them from live code"
              f", {len(dead_refs)} only from dead code or data; {len(quiet)} of deadscan's {sum(1 for c in cls.values() if c == 'reached')} reached have no Ghidra reference")
        for name, c, g in bad:
            print(f"  DISAGREE {name} ({c}): " + ", ".join(f"{t} from {s}" for s, t in g[:5]))
        if opts.all:
            for name, c, g in dead_refs:
                print(f"  both dead {name} ({c}): " + ", ".join(f"{t} from {s}" for s, t in g[:4]))
            if quiet:
                print("  reached, no Ghidra reference: " + " ".join(quiet))
        disagree += len(bad)
    return 1 if disagree else 0


if __name__ == "__main__":
    sys.exit(main())
