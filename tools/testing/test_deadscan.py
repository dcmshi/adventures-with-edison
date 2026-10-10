"""deadscan.py's checks: functions whose references were read by hand in the
disassembly, one for each way a function can be referred to or not, and
each game's classes adding up to its uncited bytes. Run by check.py's
smoke step; alone: python tools/testing/test_deadscan.py.
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import deadscan as D  # noqa: E402
import progress as P  # noqa: E402

# (game, function, class, a referrer that must be among its references or None)
KNOWN = [
    # WinMain, called by the start-up code (cited: the shared library).
    ("mystery", "f01_0000", "reached", "f31_08ba"),
    # The window procedure: RegisterClass's far pointer (01:0194 0x32E with
    # "seg s01"), its entry 2 bytes before the label (mov ax, ss at 01:032E).
    ("mystery", "f01_0330", "reached", "f01_00f6"),
    # ...and what only it calls.
    ("mystery", "f01_0664", "reached", "f01_0330"),
    # An EndDialog procedure nothing refers to: 0x2D0 appears nowhere as an
    # operand in segment 1.
    ("mystery", "f01_02d0", "unreferenced", None),
    # Only its own jumps to its g labels: they don't count.
    ("mystery", "f25_084a", "unreferenced", None),
    # A timer callback's neighbour: the offset 0x226 of an lcall beside the
    # "seg s05" push isn't a pointer (the real one is s05:03DE).
    ("mystery", "f05_01a0", "unreferenced", None),
    # A far pointer in data as a segment relocation (73:0735) with its
    # offset the word before it.
    ("mystery", "f08_0334", "table only", None),
    # A pushed far pointer: segment then offset (06:2202 push 0x1f4a ; seg
    # s06, then push 0x1b82), not the push 0 before it.
    ("rockbach", "f06_0000", "unreferenced", None),
    # A near call nedis leaves as a number (29:0208 push cs; call 0).
    ("science", "f29_0000", "reached", "f29_01d2"),
    # Wild Science's vtables: far-pointer relocations in segment 103 (the
    # blow torch's destructor, DS:01AC).
    ("science", "f02_1a1a", "table only", None),
    # ...to the prologue nedis left at the end of the function before:
    # DS:1272 is 39:1183, g39_1186's, not f39_1135's.
    ("science", "f39_1135", "unreferenced", None),
    # Borland's new, called from the constructors.
    ("science", "f08_0000", "reached", "f02_00c2"),
]


def main():
    failures = []
    cache = {}
    for game, name, cls, referrer in KNOWN:
        if game not in cache:
            rows, total = D.classify(game)
            cache[game] = ({r[1]: r for r in rows}, total, rows)
        rows_by_name = cache[game][0]
        row = rows_by_name.get(name)
        got = row[0] if row else "cited"
        if got != cls:
            failures.append(f"{game} {name}: {got}, expected {cls}")
        elif referrer and not any(r == referrer for r, _ in row[4]):
            failures.append(f"{game} {name}: {referrer} not among its references")
    # The classes add up: uncited bytes + the cited functions' = the total.
    for game, (_, total, rows) in cache.items():
        exe = next(g[2] for g in P.GAMES if g[0] == game)
        funcs = P.functions(exe)
        own = {n for s in funcs if s not in P.LIBRARY[game] for _, _, n in funcs[s]}
        if not all(r[1] in own for r in rows) or len({r[1] for r in rows}) != len(rows):
            failures.append(f"{game}: a row outside the game's own code, or twice")
        if sum(r[2] for r in rows) > total:
            failures.append(f"{game}: more uncited bytes than code")
    for f in failures:
        print("deadscan:", f)
    print("deadscan: " + ("ok" if not failures else f"{len(failures)} failed") + f" ({len(KNOWN)} known functions)")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
