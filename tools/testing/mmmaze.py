"""The bonus maze's wanderers against the original's, pass by pass.

The original runs its maze loop flat out (about 1000 passes a second under
winevdm), so its shots can't be matched in time; the port gives a pass
40 ms. Instead both play mmcompare.py's allfound (every object found, the
maze given up at 36 s): the port at a pass each millisecond of its virtual
clock (EDISON_MAZEPACE=0) writing the wanderers after each pass
(EDISON_MAZELOG), the original read by memwatch.py as it plays (DS:89DE,
11 bytes a wanderer: x, y, direction, step). Each wanderer's states in the
original must come, in order, in the port's. A read can come mid-pass:
the step at 4 before it's reset, or before the first pass; those are
listed as not found.

    mmmaze.py [--port-only | --compare-only]
"""
import os
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import mmcompare as m  # noqa: E402

NAME = "mazetrace"
LOOKAHEAD = 12  # passes: a read torn inside a record can look like a later state


def wanderers(n):
    out = []
    for i in range(n):
        b = 0x89DE + 11 * i
        out += [f"x{i}=w:{b:x}", f"y{i}=w:{b + 2:x}", f"d{i}=b:{b + 8:x}", f"s{i}=b:{b + 10:x}"]
    return out


def play(port_only):
    s = dict(m.SCENARIOS["allfound"], shots=[(30, "m30"), (37, "m37")])
    d = m.OUT / NAME
    d.mkdir(parents=True, exist_ok=True)
    # The port's exit 10 s later: at a pass a millisecond it must cover every
    # pass the original makes.
    late = [e if e[2:4] != m.MAZE_EXIT else (e[0] + 10,) + tuple(e[1:]) for e in s["events"]]
    os.environ["EDISON_MAZELOG"] = str(d / "maze.log")
    os.environ["EDISON_MAZEPACE"] = "0"
    try:
        m.play_port(NAME, dict(s, events=late, shots=[(47, "m47")]))
    finally:
        del os.environ["EDISON_MAZELOG"], os.environ["EDISON_MAZEPACE"]
    if not port_only:
        m.play_orig(NAME, s, wanderers(3))


def compare():
    d = m.OUT / NAME
    port = [tuple(int(v) for v in line.split()) for line in (d / "maze.log").read_text().splitlines()]
    orig = []
    for line in (d / "orig" / "watch.txt").read_text().splitlines():
        v = line.split()
        if len(v) == 13 and all(x.lstrip("-").isdigit() for x in v):
            t = tuple(int(x) for x in v[1:])
            if any(t[k] for k in (0, 4, 8)):
                orig.append((int(v[0]), t))
    print(f"port: {len(port)} passes; original: {len(orig)} reads")
    ok = True
    for k in range(3):
        mine = [p[4 * k:4 * k + 4] for p in port]
        seq = []
        for ms, t in orig:
            if not seq or seq[-1][1] != t[4 * k:4 * k + 4]:
                seq.append((ms, t[4 * k:4 * k + 4]))
        at, found, missed = 0, 0, []
        for ms, t in seq:
            nxt = next((i for i in range(at, min(at + LOOKAHEAD, len(mine))) if mine[i] == t), None)
            if nxt is None:
                missed.append((ms, t))
            else:
                at, found = nxt, found + 1
        # Mid-pass reads: the step at 4, or before the first pass.
        odd = [x for x in missed if x[1][3] != 4 and x[1][2] != 255]
        ok = ok and not odd and found > len(seq) * 0.95
        print(f"wanderer {k}: {found} of {len(seq)} states found in order (to the port's pass {at});"
              f" {len(missed) - len(odd)} mid-pass reads, {len(odd)} others")
        for ms, t in odd[:5]:
            print("  not found:", ms, t)
    print("PASS" if ok else "FAIL")
    return ok


def main():
    if "--compare-only" not in sys.argv:
        play("--port-only" in sys.argv)
    sys.exit(0 if compare() else 1)


if __name__ == "__main__":
    main()
