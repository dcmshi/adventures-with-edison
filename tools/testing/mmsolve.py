"""Working out moves for mmcompare.py's playPPdD scenarios. Both games
draw the same random numbers (docs/MYSTERY.md), so a puzzle's deal can be
read from the port's draws (EDISON_RNGLOG) and solved here.

  python tools/testing/mmsolve.py rng PUZZLE LEVEL
      the port's draws for the puzzle's opening (puzzle_start), to
      build/scratch/mmcompare/rng-PUZZLE-LEVEL.txt; printed as n:result
  python tools/testing/mmsolve.py answers PUZZLE LEVEL BEFORE ANSWER BUTTONS [--dismiss] [--gap S]
      each round's answer: the draw random(ANSWER) right after a
      random(BEFORE); found round by round, the port playing the answers so
      far (BUTTONS: a list in mmcompare.py, the button for each answer)
  python tools/testing/mmsolve.py board switch|slide|arrow|dig LEVEL
      the picture puzzles' deals and the moves that solve them (the Slide
      Puzzle up to 3 x 3, the Arrow Puzzle 2 x 2: a breadth-first search);
      the Dig's dug-out tiles and their places
  python tools/testing/mmsolve.py next LEVEL
      What Comes Next's right answers, probed: a click on each answer of a
      row in turn; the right one puts it in place of the '?'

Needs EDISON_RUN, as mmcompare.py.
"""
import argparse
import os
import sys
from collections import deque
from pathlib import Path

from PIL import Image, ImageChops

import mmcompare as m

PUZZLE = {"switch": 12, "slide": 10, "arrow": 13, "dig": 15}


def rng_log(puzzle, level, events=(), shots=None):
    """The port played to a scenario's end with EDISON_RNGLOG: [(n, result)]."""
    log = m.OUT / f"rng-{puzzle}-{level}.txt"
    log.parent.mkdir(parents=True, exist_ok=True)
    os.environ["EDISON_RNGLOG"] = str(log.resolve())
    try:
        end = max([13] + [e[0] + 1 for e in events])
        s = m.puzzle_play(puzzle, level, list(events), shots or [(end, "x")])
        m.play_port(f"rng-{puzzle}-{level}", s)
    finally:
        del os.environ["EDISON_RNGLOG"]
    return [tuple(map(int, line.split()[:2])) for line in log.read_text().splitlines()]


def answers(puzzle, level, before, answer, buttons, dismiss=False, gap=4.0, rounds=5):
    picks = []
    while len(picks) < rounds:
        events = []
        for i, k in enumerate(picks):
            events.append(m.click(13 + gap * i, *buttons[k]))
            if dismiss:
                events.append(m.click(13 + gap * i + gap / 2, 320, 200))
        d = rng_log(puzzle, level, events, [(13 + gap * len(picks) + 1, "x")])
        found = [d[i + 1][1] for i in range(len(d) - 1) if d[i][0] == before and d[i + 1][0] == answer]
        if len(found) <= len(picks):
            sys.exit(f"round {len(picks) + 1}: no random({answer}) after a random({before}) ({found})")
        picks = found[:len(picks) + 1]
        print(picks, flush=True)
    return picks


# --- the picture puzzles (segments 12-14) ----------------------------------

def switch(level, d):
    n = level + 2
    for start in range(len(d)):
        board, i, j = [-1] * (n * n), 0, start
        while j < len(d) and d[j][0] == n * n and i < n * n:
            if board[d[j][1]] == -1:
                board[d[j][1]] = i
                i += 1
            j += 1
        if i == n * n and j < len(d) and d[j][0] == 17:  # then the picture
            return board
    sys.exit("no deal found")


def slide_to(b, n, blank, r, c):
    """g14_0000: a click on (r, c) in the blank's row or column."""
    br, bc = blank
    if r >= n or c >= n or b[r][c] is None:
        return blank
    if r == br:
        step = -1 if c < bc else 1
        for k in range(bc, c, step):
            b[r][k] = b[r][k + step]
    elif c == bc:
        step = -1 if r < br else 1
        for k in range(br, r, step):
            b[k][c] = b[k + step][c]
    else:
        return blank
    b[r][c] = None
    return (r, c)


def slide(level, d):
    """The mix (random(20) + 20 slides, each random(n) and random(500)) and
    the shortest way back: the cells to click."""
    n = level + 2
    start = next(i for i in range(len(d) - 1) if d[i][0] == 20 and d[i + 1][0] == n)
    board = [[r * n + c for c in range(n)] for r in range(n)]
    board[n - 1][n - 1] = None
    blank = (n - 1, n - 1)
    goal = tuple(tuple(row) for row in board)
    j = start + 1
    for _ in range(d[start][1] + 20):
        k, coin = d[j][1], d[j + 1][1]
        j += 2
        blank = slide_to(board, n, blank, blank[0], k) if coin < 250 else slide_to(board, n, blank, k, blank[1])
    first = tuple(tuple(row) for row in board)
    seen, queue = {first: None}, deque([(first, blank)])
    while queue:
        state, bl = queue.popleft()
        if state == goal:
            break
        for dr, dc in ((0, 1), (0, -1), (1, 0), (-1, 0)):
            r, c = bl[0] + dr, bl[1] + dc
            if 0 <= r < n and 0 <= c < n:
                b = [list(row) for row in state]
                nb = slide_to(b, n, bl, r, c)
                t = tuple(tuple(row) for row in b)
                if t not in seen:
                    seen[t] = (state, (r, c))
                    queue.append((t, nb))
    path, s = [], goal
    while seen[s] is not None:
        s, at = seen[s]
        path.append(at)
    return first, path[::-1]


def arrow_shove(b, size, arrow):
    """g12_0846, on a tuple board (None for a blank); None if nothing moves."""
    b = [list(r) for r in b]
    last = size - 1
    g, k = divmod(arrow, 8)
    if g in (0, 2):  # towards the far end: into the last blank from there
        get = (lambda i: b[i][k]) if g == 0 else (lambda i: b[k][i])

        def put(i, v):
            if g == 0:
                b[i][k] = v
            else:
                b[k][i] = v
        i = last
        while i >= 1 and get(i) is not None:
            i -= 1
        if i < 1:
            return None
        for i in range(i, 0, -1):
            put(i, get(i - 1))
        put(0, None)
    else:
        get = (lambda i: b[i][k]) if g == 1 else (lambda i: b[k][i])

        def put(i, v):
            if g == 1:
                b[i][k] = v
            else:
                b[k][i] = v
        for i in range(last):
            if get(i) is None:
                for i in range(i, last):
                    put(i, get(i + 1))
                put(last, None)
                break
        else:
            return None
    return tuple(tuple(r) for r in b)


def arrow_solved(b, n, size):
    first = next(((r, c) for r in range(size) for c in range(size) if b[r][c] is not None), None)
    if first is None:
        return False
    want = 0
    for r in range(first[0], first[0] + n):
        for c in range(first[1], first[1] + n):
            if r >= size or c >= size or b[r][c] != want:
                return False
            want += 1
    return True


def arrow(level, d, limit=2_000_000):
    n, size = level + 2, level + 4
    blanks, cells = n * 4 + 4, size * size
    for start in range(len(d)):
        board, i, j = [-1] * cells, 0, start
        while j < len(d) and d[j][0] == cells and i < n * n + blanks:
            if board[d[j][1]] == -1:
                board[d[j][1]] = None if i < blanks else i - blanks
                i += 1
            j += 1
        if i == n * n + blanks and j < len(d) and d[j][0] == 17:
            break
    else:
        sys.exit("no deal found")
    first = tuple(tuple(board[r * size:(r + 1) * size]) for r in range(size))
    seen, queue, goal = {first: None}, deque([first]), None
    while queue and len(seen) < limit:
        s = queue.popleft()
        if arrow_solved(s, n, size):
            goal = s
            break
        for a in [g * 8 + k for g in range(4) for k in range(size)]:
            t = arrow_shove(s, size, a)
            if t is not None and t not in seen:
                seen[t] = (s, a)
                queue.append(t)
    if goal is None:
        sys.exit(f"not solved within {len(seen)} states")
    path, s = [], goal
    while seen[s] is not None:
        s, a = seen[s]
        path.append(a)
    return first, path[::-1]


def dig(level, d):
    """g21_02a8: the wall dealt, then (level + 1) * 2 tiles dug out: each
    belt tile and its place (column, row)."""
    for start in range(len(d)):
        j, wall, i = start, [[-1] * 4 for _ in range(5)], 0
        while i < 20 and j + 1 < len(d) and d[j][0] == 5 and d[j + 1][0] == 4:
            a, b = d[j][1], d[j + 1][1]
            j += 2
            if wall[a][b] >= 0:
                continue
            if [x[0] for x in d[j:j + 4]] != [3] * 4:
                break
            wall[a][b] = i
            j += 4
            i += 1
        if i < 20:
            continue
        belt = []
        while len(belt) < (level + 1) * 2:
            a, b = d[j][1], d[j + 1][1]
            j += 2
            if wall[a][b] >= 0:
                belt.append((wall[a][b], a, b))
                wall[a][b] = -1
        return belt
    sys.exit("no deal found")


def next_answers(level):
    """What Comes Next: a click on each answer of a row in turn."""
    right = []
    for i in range(5):
        for k in range(4):
            name = f"next-{level}-{i}{k}"
            m.play_port(name, m.puzzle_play(14, level, [m.click(13, *m.next_answer(i, k))], [(13.6, "x")]))
            frames = m.OUT / name / "port"
            before = Image.open(frames / f"{int((13 + m.LEAD) * 1000) - 99}.bmp").convert("RGB")
            after = Image.open(frames / f"{int((13.5 + m.LEAD) * 1000) + 1}.bmp").convert("RGB")
            y = i * 0x37 + 0x3C - 0x18
            box = (0x5F + 0x99, y, 0x5F + 0x99 + 0x36, y + 0x31)
            if ImageChops.difference(before.crop(box), after.crop(box)).getbbox():
                right.append(k)
                break
        else:
            right.append(None)
        print(right, flush=True)
    return right


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    r = sub.add_parser("rng")
    r.add_argument("puzzle", type=int)
    r.add_argument("level", type=int)
    a = sub.add_parser("answers")
    for name in ("puzzle", "level", "before", "answer"):
        a.add_argument(name, type=int)
    a.add_argument("buttons")
    a.add_argument("--dismiss", action="store_true", help="a click after each answer (a box to put away)")
    a.add_argument("--gap", type=float, default=4.0)
    b = sub.add_parser("board")
    b.add_argument("kind", choices=sorted(PUZZLE))
    b.add_argument("level", type=int)
    n = sub.add_parser("next")
    n.add_argument("level", type=int)
    args = ap.parse_args()
    if args.cmd == "rng":
        print(" ".join(f"{k}:{v}" for k, v in rng_log(args.puzzle, args.level)))
    elif args.cmd == "answers":
        print("answers", answers(args.puzzle, args.level, args.before, args.answer, getattr(m, args.buttons),
                                 args.dismiss, args.gap))
    elif args.cmd == "board":
        d = rng_log(PUZZLE[args.kind], args.level)
        if args.kind == "switch":
            print("board", switch(args.level, d))
        elif args.kind == "slide":
            board, path = slide(args.level, d)
            print("board", board)
            print("cells", path)
        elif args.kind == "arrow":
            board, path = arrow(args.level, d)
            print("board", board)
            print("arrows", path)
        else:
            print("tile, column, row:", dig(args.level, d))
    else:
        print("answers", next_answers(args.level))


if __name__ == "__main__":
    main()
