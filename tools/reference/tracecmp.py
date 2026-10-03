"""Compares a trace of the original's state (from memwatch.py watch) with
the port's (its log), state by state, ignoring time (the two don't run in
step): each state the original showed must be one the port went through,
in the same order.

    tracecmp.py ORIGINAL.txt PORT.log [--port-pattern REGEX]

ORIGINAL.txt: memwatch.py's lines ("ms v1 v2 ..."). PORT.log: lines with the
same values, picked out by the pattern's groups (by default the Wild
Science Arcade's ball, as `SCI_DEBUG=1` logs it: " c x,y,z v vx,vy,vz").

memwatch reads while the game runs, so it can catch a state in the middle
of an update (the ball's velocity is stored before it moves, and changed
again by a bounce); such a read counts as matching when its first half
(here the centre) is that of one of the next port states. It can also see
a state the port only passed through within a tick (so never logged): one
the next state goes on from, nearby in the port's, counts too.
"""
import argparse
import re


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("original")
    ap.add_argument("port")
    ap.add_argument("--port-pattern", default=r" c (-?\d+),(-?\d+),(-?\d+) v (-?\d+),(-?\d+),(-?\d+)")
    ap.add_argument("--split", type=int, default=3, help="values in the first half (default 3)")
    args = ap.parse_args()

    original = []
    for line in open(args.original):
        if line[:1].isdigit():
            state = tuple(int(x) for x in line.split()[1:])
            if not original or original[-1] != state:
                original.append(state)
    pattern = re.compile(args.port_pattern)
    port = []
    for line in open(args.port):
        m = pattern.search(line)
        if m:
            state = tuple(int(g) for g in m.groups())
            if not port or port[-1] != state:
                port.append(state)
    print(f"original states {len(original)}, port states {len(port)}")
    h = args.split
    j = 0
    bad = 0
    window = 64
    passing = 0
    for i, state in enumerate(original):
        near = port[j:j + window]
        if state in near:
            j = port.index(state, j)
            continue
        # Caught mid-update: the same first half as a port state just
        # ahead, its second half already (or not yet) changed.
        if any(port[k][:h] == state[:h] for k in range(j, min(j + 3, len(port)))):
            continue
        # A state the port went through within a tick (so never logged),
        # seen in passing: the next one follows on nearby.
        if i + 1 < len(original) and original[i + 1] in near:
            passing += 1
            if passing <= 10:
                print(f"original #{i} {state}: taken as passed through in a tick")
            continue
        if state in port[j:]:
            j = port.index(state, j)
            continue
        bad += 1
        if bad <= 10:
            print(f"original #{i} {state}: not in the port after #{j} ({port[j:j + 2]})")
    print(f"unmatched {bad} of {len(original)}" + (f" ({passing} taken as passed through)" if passing else ""))


if __name__ == "__main__":
    main()
