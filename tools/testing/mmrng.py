"""Mystery at the Museums' random numbers against the original's: a
scenario of mmcompare.py played in both, the generator's six words
(DS:7638-7643, segment 46) followed in the original by memwatch.py and
logged by the port (EDISON_RNGLOG), each state placed in the generator's
one sequence (it's never seeded: docs/MYSTERY.md) by stepping it.

  python tools/testing/mmrng.py NAME ... [--compare-only]

For each scenario, both played on past its end (the port 8 s, the
original 9 s): the number of draws (rand16 steps) each side had made at
its last state, and whether every state the original showed comes in the
port's sequence of draws, in order (the port logs random(n)'s draws and
the state after a screen's stir, so one seen inside a stir is between
two of its). The same count means both drew the same numbers, including
draws nothing shows yet (an unused colour, a stir). Out:
build/scratch/mmrng/NAME/ (watch.txt, rng.log). Needs what mmcompare.py
does.
"""
import argparse
import os
import shutil
import sys

import mmcompare
from testlib import SCRATCH, say

OUT = SCRATCH / "mmrng"
WORDS = ["u:7638", "u:763a", "u:763c", "u:763e", "u:7640", "u:7642"]
LIMIT = 3_000_000  # steps searched for a state


def step(st):
    """f46_0000: the sum with carries of the first five becomes the sixth."""
    total, carry = 0, 0
    for i in range(5):
        total += st[i] + carry
        carry, total = total >> 16, total & 0xFFFF
    return st[1:] + (total,)


def port_states(log):
    """The port's states after each draw, in order."""
    out = []
    for line in log.read_text().splitlines():
        parts = line.split()
        if len(parts) == 8:
            out.append(tuple(int(w, 16) for w in parts[2:]))
    return out


def orig_states(watch):
    """The original's states as memwatch saw them change, in order."""
    out = []
    for line in watch.read_text().splitlines():
        parts = line.split()
        if len(parts) == 7 and parts[0].isdigit():
            st = tuple(int(w) & 0xFFFF for w in parts[1:])
            if not out or out[-1] != st:
                out.append(st)
    return out


def positions(start, wanted):
    """Each wanted state's step count from start (None past LIMIT)."""
    left = set(wanted)
    found, st = {}, start
    if st in left:
        found[st] = 0
        left.discard(st)
    for n in range(1, LIMIT + 1):
        if not left:
            break
        st = step(st)
        if st in left:
            found[st] = n
            left.discard(st)
    return found


def check(name):
    d = OUT / name
    port, orig = port_states(d / "rng.log"), orig_states(d / "watch.txt")
    if not port or not orig:
        return f"{name}: no states ({len(port)} port, {len(orig)} original)"
    # Counted from the port's first draw (an original state before it isn't
    # found, and counts as "not in the sequence").
    pos = positions(port[0], set(port) | set(orig))
    p_last = pos.get(port[-1])
    o_seen = [pos.get(s) for s in orig]
    o_last = next((p for p in reversed(o_seen) if p is not None), None)
    lost = sum(p is None for p in o_seen)
    port_pos = sorted(pos[s] for s in port if s in pos)
    port_set = set(port_pos)
    # The port logs only random(n)'s draws (not a stir's inner ones), so an
    # original state between two of its logged ones is in order but not in
    # the set.
    in_order = all(a <= b for a, b in zip([p for p in o_seen if p is not None], [p for p in o_seen if p is not None][1:]))
    unlogged = sum(1 for p in o_seen if p is not None and p not in port_set)
    verdict = "same" if p_last == o_last else "DIFFERENT"
    return (f"{name}: {verdict}: port {p_last} draws, original {o_last} "
            f"({len(orig)} states seen, {lost} not in the sequence, {unlogged} between the port's logged draws, "
            f"{'in order' if in_order else 'OUT OF ORDER'})")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("names", nargs="+")
    ap.add_argument("--compare-only", action="store_true")
    opts = ap.parse_args()
    for name in opts.names:
        s = dict(mmcompare.SCENARIOS[name])
        # Both played on past the scenario's end, so each side's last state
        # is one at rest (a screen's stir comes as it changes): the port
        # quits 2 s after its last shot, at 8 s; the original closes after
        # its last, at 9 s.
        end = max([t for t, _ in s["shots"]] + [e[0] for e in s["events"]])
        port_s = dict(s, shots=s["shots"] + [(end + 6, "rngend")])
        s = dict(s, shots=s["shots"] + [(end + 9, "rngend")])
        d = OUT / name
        if not opts.compare_only:
            d.mkdir(parents=True, exist_ok=True)
            log = d / "rng.log"
            log.unlink(missing_ok=True)
            os.environ["EDISON_RNGLOG"] = str(log)
            try:
                mmcompare.play_port(name, port_s)
            finally:
                del os.environ["EDISON_RNGLOG"]
            mmcompare.play_orig(name, s, watch=WORDS)
            shutil.copy2(mmcompare.OUT / name / "orig" / "watch.txt", d / "watch.txt")
        say(check(name))
    return 0


if __name__ == "__main__":
    sys.exit(main())
