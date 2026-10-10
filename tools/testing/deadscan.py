"""The coverage check of each game's own code (not its library and run time,
tools/progress.py's LIBRARY): every function the port's source doesn't cite,
put in one of four classes by what refers to it, to show that what is left
is dead or deliberately not ported.

  python tools/testing/deadscan.py [mystery|rockbach|science ...] [--list CLASS ...]

What counts as a reference to a function (its start, or the Borland
prologue 3 bytes before it), from the disassembly (tools/nedis.py) and the
executable's relocations:
  - a call or jump to it, near or far (nedis names the target);
  - a far pointer to it in code (nedis's "; sNN:OOOO" or "; seg sNN" with
    the offset beside it) or in a data segment (a far-pointer relocation:
    Wild Science's vtables and callbacks; Mystery's and Rock and Bach's
    data segments have none);
  - an immediate equal to its offset in the same segment (a near pointer:
    taken whether or not it is one, so this side errs towards "referenced");
  - an export of the executable (the entry table).

The classes (a function in the first that applies):
  unreferenced  nothing refers to it: dead (data nedis took for code,
                RTTI records, functions nothing calls)
  dead chain    referred to only by unreferenced or dead-chain functions,
                never by the port's (cited) code or a root
  table only    referred to only from data (a vtable slot, a callback
                table): live only if that table's class or caller is
  reached       called or pointed to from cited code or a root: each needs
                its reason in the notes (replaced by the platform layer,
                no effect, ...)
Roots: the cited functions, the exports. A function the notes document is
marked "documented" in the list; one neither cited nor documented,
"UNCITED".

Out: per game the counts and bytes of each class; --list prints the
functions of the classes named (all four: --list all), each with its
references.
"""
import argparse
import re
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import progress as P  # noqa: E402
from ne import NEFile  # noqa: E402

EXE_DIR = P.ROOT / "original" / "cd" / "DSK3"
LABEL = re.compile(r"([fg])([0-9]{2})_([0-9a-f]{4}):$")
NAMED = re.compile(r"\b[fgl]([0-9]{2})_([0-9a-f]{4})\b")
FAR = re.compile(r"; s([0-9]{2}):([0-9a-f]{4})")
SEG = re.compile(r"; seg s([0-9]{2})\b")
IMM = re.compile(r"(?:,|push(?: word)?)\s+(0x[0-9a-f]+|[0-9]+)\s*(?:;|$)")
SITE = re.compile(r"^([0-9]{2}):([0-9a-f]{4}): ")
CLASSES = ("unreferenced", "dead chain", "table only", "reached")


def starts(funcs):
    """{(seg, off): name} of every function's start, and the 1-3 bytes of
    prologue before it that nedis leaves to the function before (Borland's
    "mov ax, ss; nop" for a far function, an exported callback's entry
    2 bytes before its label: Mystery's window procedure at 01:032E)."""
    out = {}
    for seg, rows in funcs.items():
        for off, _, name in rows:
            out[(seg, off)] = name
            for back in (1, 2, 3):
                out.setdefault((seg, off - back), name)
    return out


def references(key, funcs):
    """{name: {(referrer or None, how)}}: None for data and exports."""
    exe = next(g[2] for g in P.GAMES if g[0] == key)
    find = P.locate(funcs)
    begin = starts(funcs)
    refs = defaultdict(set)

    def add(target, referrer, how):
        if target and target != referrer:
            refs[target].add((referrer, how))

    lines = (P.DISASM / (Path(exe).stem.lower() + ".asm")).read_text(encoding="utf-8", errors="replace").splitlines()
    current = None
    for i, line in enumerate(lines):
        m = LABEL.match(line)
        if m:
            # (A g label is a place inside a function: the function's.)
            current = find(int(m.group(2)), int(m.group(3), 16))
            continue
        site = SITE.match(line)
        here = int(site.group(1)) if site else None
        code = line.split(";", 1)[0]
        for m in NAMED.finditer(line):
            how = "far call" if "lcall" in code else "jump" if re.search(r"\bj[a-z]+\b", code) else "call" if "call" in code else "named"
            add(find(int(m.group(1)), int(m.group(2), 16)), current, how)
        for m in FAR.finditer(line):
            add(find(int(m.group(1)), int(m.group(2), 16)), current, "far pointer")
        m = SEG.search(line)
        if m:
            seg = int(m.group(1))
            for near in lines[max(0, i - 3):i + 4]:
                for v in IMM.findall(near.split(";", 1)[0]):
                    # (Beside a segment, an offset: a start, or inside.)
                    name = begin.get((seg, int(v, 0))) or find(seg, int(v, 0))
                    if name:
                        add(name, current, "far pointer")
        if here is not None:
            for v in IMM.findall(code):
                name = begin.get((here, int(v, 0)))
                if name:
                    add(name, current, "near pointer")
    ne = NEFile(str(EXE_DIR / exe))
    code_segs = {s["index"] for s in ne.segments if not s["data"]}
    for s in ne.segments:
        if not s["data"]:
            continue
        data = ne.segment_bytes(s["index"])
        for r in ne.relocations(s["index"]):
            if r["kind"] != "internal":
                continue
            if r["addr_type"] == 3:
                add(find(*r["target"]), None, f"data s{s['index']}")
            elif r["addr_type"] == 2 and r["target"][0] in code_segs:
                # A far pointer as the segment's relocation, its offset the
                # plain word before it (Mystery's panels' callbacks: 64 in
                # MALL's data, 8 in WINMAIN's).
                for site in r["sites"]:
                    if site >= 2:
                        off = int.from_bytes(data[site - 2:site], "little")
                        seg = r["target"][0]
                        add(begin.get((seg, off)) or find(seg, off), None, f"data s{s['index']}")
    for seg, off in ne.entries.values():
        add(find(seg, off), None, "export")
    return refs


def classify(key):
    exe = next(g[2] for g in P.GAMES if g[0] == key)
    notes = next(g[4] for g in P.GAMES if g[0] == key)
    funcs = P.functions(exe)
    find = P.locate(funcs)
    ported = P.cited(P.source_lines(P.worktree_lines())[key], find)
    documented = P.cited((P.ROOT / notes).read_text(encoding="utf-8").splitlines(), find)
    refs = references(key, funcs)
    calls = defaultdict(set)
    for t, rs in refs.items():
        for r, _ in rs:
            if r:
                calls[r].add(t)
    # Reached: from the cited functions and the exports, through code.
    roots = set(ported) | {t for t, rs in refs.items() if any(h == "export" for _, h in rs)}
    reached, stack = set(), list(roots)
    while stack:
        f = stack.pop()
        if f not in reached:
            reached.add(f)
            stack += calls.get(f, ())
    lib = P.LIBRARY[key]
    rows = []
    for seg in sorted(s for s in funcs if s not in lib):
        for _, size, name in funcs[seg]:
            if name in ported:
                continue
            rs = refs.get(name, set())
            if name in reached:
                cls = "reached"
            elif not rs:
                cls = "unreferenced"
            else:
                # (Not reached: no reached function refers to it.)
                cls = "table only" if any(r is None for r, _ in rs) else "dead chain"
            rows.append((cls, name, size, "documented" if name in documented else "UNCITED", rs))
    total = sum(size for seg in funcs if seg not in lib for _, size, _ in funcs[seg])
    return rows, total


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("games", nargs="*", default=[g[0] for g in P.GAMES])
    ap.add_argument("--list", nargs="+", default=[], metavar="CLASS",
                    help="the functions of these classes: unreferenced, chain, table, reached, all")
    opts = ap.parse_args()
    wanted = set()
    for w in opts.list:
        wanted |= set(CLASSES) if w == "all" else {c for c in CLASSES if c.startswith(w)}
    for key in opts.games:
        rows, total = classify(key)
        title = next(g[1] for g in P.GAMES if g[0] == key)
        left = sum(r[2] for r in rows)
        print(f"{title}: {len(rows)} functions not cited, {left} of {total} bytes ({100 * left / total:.1f}%)")
        for cls in CLASSES:
            mine = [r for r in rows if r[0] == cls]
            unc = [r for r in mine if r[3] == "UNCITED"]
            print(f"  {cls:13} {len(mine):4} functions {sum(r[2] for r in mine):6} bytes"
                  + (f" ({len(unc)} uncited, {sum(r[2] for r in unc)} bytes)" if unc else ""))
        for cls, name, size, doc, rs in rows:
            if cls in wanted:
                how = sorted({f"{h} {r}" if r else h for r, h in rs})
                print(f"    {cls}: {name} {size} bytes {doc}: {', '.join(how[:5]) or 'no references'}"
                      + (f" (+{len(how) - 5})" if len(how) > 5 else ""))


if __name__ == "__main__":
    main()
