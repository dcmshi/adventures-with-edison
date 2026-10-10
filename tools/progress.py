"""Draws how much of the three games' code the port covers, as two
pictures (after PascalPixel/alchemy's):

  progress/PROGRESS.png        each game's code as a treemap: a box per
                               segment, one per function, sized by its
                               bytes, coloured by what the port has of it
  progress/PROGRESS_CHART.png  the share ported, commit by commit

A function is "ported" when the port's source for its game cites it (by
name, f30_306a / g30_3527, or by an address inside it, 30:3527),
"documented" when only the game's notes (docs/MYSTERY.md, ROCKBACH.md,
SCIENCE.md) do (Windows and the CD, the platform layer, code nothing
reaches), else "not yet". The shared code (engine/src/artech) counts for
Mystery's MALL.EXE unless its line names WMAIN or WINMAIN / Rock and Bach.
The functions and their sizes come from tools/nedis.py's disassembly
(extracted/disasm/*.asm: run it first); the chart re-reads the citations
at each commit (git grep), the sizes being today's.

  python tools/progress.py [--no-history]
"""
import argparse
import datetime
import re
import subprocess
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
DISASM = ROOT / "extracted" / "disasm"
OUT = ROOT / "progress"

GAMES = [
    # key, title, executable, source directory, notes, colour on the chart
    ("mystery", "Mystery at the Museums", "MALL.EXE", "engine/src/mystery", "docs/MYSTERY.md", (240, 196, 90)),
    ("rockbach", "Rock and Bach Studio", "WINMAIN.EXE", "engine/src/rockbach", "docs/ROCKBACH.md", (120, 190, 240)),
    ("science", "Wild Science Arcade", "WMAIN.EXE", "engine/src/science", "docs/SCIENCE.md", (150, 220, 140)),
]
SHARED = "engine/src/artech"

NAME = re.compile(r"\b[fg]([0-9]{2})_([0-9a-f]{4})\b")
ADDRESS = re.compile(r"\b([0-9]{2}):([0-9a-f]{4})\b")

# Colours: the status, the frames, the text.
PORTED = (86, 176, 120)
DOCUMENTED = (222, 178, 84)
NOT_YET = (92, 104, 112)
BACK = (24, 36, 44)
FRAME = (14, 22, 28)
HEAD = (40, 58, 70)
TEXT = (230, 236, 240)
DIM = (150, 165, 175)


# --- the functions ----------------------------------------------------------------

def functions(exe):
    """{segment: [(offset, size, name)]} from the disassembly."""
    path = DISASM / (Path(exe).stem.lower() + ".asm")
    if not path.exists():
        sys.exit(f"{path} not found: run tools/nedis.py on the game's {exe} first")
    segs, sizes = {}, {}
    seg = None
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        m = re.match(r";+ segment (\d+) \((0x[0-9a-f]+) bytes\)", line)
        if m:
            seg = int(m.group(1))
            sizes[seg] = int(m.group(2), 16)
            segs.setdefault(seg, [])
            continue
        m = re.match(r"([fg])([0-9]{2})_([0-9a-f]{4}):$", line)
        if m and seg is not None:
            segs[seg].append(int(m.group(3), 16))
    out = {}
    for seg, starts in segs.items():
        starts = sorted(set(starts))
        if not starts:
            continue
        ends = starts[1:] + [sizes[seg]]
        out[seg] = [(s, max(e - s, 1), f"f{seg:02d}_{s:04x}") for s, e in zip(starts, ends)]
    return out


def locate(funcs):
    """A finder: (segment, offset) to the function holding it."""
    import bisect
    index = {seg: [f[0] for f in fs] for seg, fs in funcs.items()}

    def find(seg, off):
        starts = index.get(seg)
        if not starts:
            return None
        i = bisect.bisect_right(starts, off) - 1
        if i < 0:
            return None
        s, size, name = funcs[seg][i]
        return name if off < s + size else None
    return find


def cited(lines, find):
    """The functions the lines cite, by name or by an address inside."""
    out = set()
    for line in lines:
        for m in NAME.finditer(line):
            f = find(int(m.group(1)), int(m.group(2), 16))
            if f:
                out.add(f)
        for m in ADDRESS.finditer(line):
            f = find(int(m.group(1)), int(m.group(2), 16))
            if f:
                out.add(f)
    return out


def shared_game(line):
    """Which game a line of the shared code means."""
    if "WMAIN" in line:
        return "science"
    if "WINMAIN" in line or "Rock" in line:
        return "rockbach"
    return "mystery"


def source_lines(files):
    """{game: [lines]} for the port's source, from (path, line) pairs."""
    out = {g[0]: [] for g in GAMES}
    for path, line in files:
        p = path.replace("\\", "/")
        if p.startswith(SHARED + "/"):
            out[shared_game(line)].append(line)
            continue
        for key, _, _, src, _, _ in GAMES:
            if p.startswith(src + "/"):
                out[key].append(line)
    return out


def worktree_lines():
    pairs = []
    for d in [SHARED] + [g[3] for g in GAMES]:
        for f in sorted((ROOT / d).rglob("*")):
            if f.suffix in (".cpp", ".h") and f.is_file():
                rel = f.relative_to(ROOT).as_posix()
                pairs += [(rel, line) for line in f.read_text(encoding="utf-8", errors="replace").splitlines()]
    return pairs


def git_lines(rev):
    """(path, line) for the cited lines of the port's source at a commit."""
    paths = [SHARED] + [g[3] for g in GAMES]
    r = subprocess.run(["git", "grep", "-E", r"[fg][0-9]{2}_[0-9a-f]{4}|[0-9]{2}:[0-9a-f]{4}", rev, "--", *paths],
                       cwd=ROOT, capture_output=True, text=True, encoding="utf-8", errors="replace")
    pairs = []
    for row in r.stdout.splitlines():
        parts = row.split(":", 2)
        if len(parts) == 3:
            pairs.append((parts[1], parts[2]))
    return pairs


# --- the treemap --------------------------------------------------------------------

def squarify(items, x, y, w, h):
    """Bruls et al.'s squarified layout: [(value, item)] (largest first)
    into rectangles, [(item, (x, y, w, h))]."""
    total = sum(v for v, _ in items)
    if total <= 0 or w <= 0 or h <= 0:
        return []
    scale = w * h / total
    items = [(v * scale, it) for v, it in items if v > 0]
    out = []
    while items:
        short = min(w, h)
        row, rest = [items[0]], items[1:]

        def worst(r):
            s = sum(a for a, _ in r)
            return max(max(short * short * a / (s * s), s * s / (short * short * a)) for a, _ in r)
        while rest and worst(row + [rest[0]]) <= worst(row):
            row.append(rest.pop(0))
        s = sum(a for a, _ in row)
        if w >= h:  # a column on the left
            cw = s / h
            cy = y
            for a, it in row:
                ch = a / cw
                out.append((it, (x, cy, cw, ch)))
                cy += ch
            x, w = x + cw, w - cw
        else:  # a row along the top
            rh = s / w
            cx = x
            for a, it in row:
                rw = a / rh
                out.append((it, (cx, y, rw, rh)))
                cx += rw
            y, h = y + rh, h - rh
        items = rest
    return out


def font(size):
    for name in ("DejaVuSans.ttf", "arial.ttf", "segoeui.ttf"):
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            continue
    return ImageFont.load_default()


def box(draw, r, fill, outline=FRAME):
    x, y, w, h = r
    x0, y0, x1, y1 = round(x), round(y), round(x + w) - 1, round(y + h) - 1
    if x1 < x0 or y1 < y0:
        return
    draw.rectangle([x0, y0, x1, y1], fill=fill, outline=outline)


def share(funcs, status):
    total = sum(size for fs in funcs.values() for _, size, _ in fs)
    by = {"ported": 0, "documented": 0, "not yet": 0}
    for fs in funcs.values():
        for _, size, name in fs:
            by[status.get(name, "not yet")] += size
    return total, by


def treemap(games, path):
    W, H = 1600, 2000
    img = Image.new("RGB", (W, H), BACK)
    d = ImageDraw.Draw(img)
    title, small, tiny = font(30), font(18), font(13)
    d.text((24, 18), "Adventures with Edison: the port's coverage of the original code", font=title, fill=TEXT)
    stamp = datetime.date.today().isoformat()
    d.text((W - 24, 26), stamp, font=small, fill=DIM, anchor="ra")
    top, bottom = 70, H - 60
    sizes = [sum(s for fs in g["funcs"].values() for _, s, _ in fs) for g in games]
    y = top
    span = bottom - top - 12 * (len(games) - 1)
    colours = {"ported": PORTED, "documented": DOCUMENTED, "not yet": NOT_YET}
    for g, size in zip(games, sizes):
        gh = span * size / sum(sizes)
        total, by = share(g["funcs"], g["status"])
        box(d, (16, y, W - 32, gh), HEAD)
        label = (f"{g['title']} ({g['exe']}): {100 * by['ported'] / total:.1f}% ported, "
                 f"{100 * by['documented'] / total:.1f}% documented, {total:,} bytes")
        d.text((26, y + 6), label, font=small, fill=TEXT)
        inner = (20, y + 32, W - 40, gh - 36)
        segs = sorted(((sum(s for _, s, _ in fs), seg) for seg, fs in g["funcs"].items()), reverse=True)
        for seg, r in squarify(segs, *inner):
            box(d, r, HEAD)
            sx, sy, sw, sh = r
            head = 16 if sh > 40 and sw > 40 else 0
            fs = sorted(((s, (n, g["status"].get(n, "not yet"))) for _, s, n in g["funcs"][seg]), reverse=True)
            for (name, st), fr in squarify(fs, sx + 2, sy + head + 1, sw - 4, sh - head - 3):
                box(d, fr, colours[st])
            if head:
                d.text((sx + 4, sy + 1), f"segment {seg}", font=tiny, fill=TEXT)
        y += gh + 12
    # The legend.
    x = 24
    for name, c in (("ported", PORTED), ("documented only", DOCUMENTED), ("not yet", NOT_YET)):
        d.rectangle([x, H - 40, x + 18, H - 22], fill=c, outline=FRAME)
        d.text((x + 26, H - 42), name, font=small, fill=TEXT)
        x += 60 + d.textlength(name, font=small)
    d.text((W - 24, H - 42), "a box a function, sized by its bytes", font=small, fill=DIM, anchor="ra")
    img.save(path, optimize=True)


# --- the chart ------------------------------------------------------------------------

def history(games, finders):
    """[(date, {game: ported share})] for each commit, oldest first."""
    revs = subprocess.run(["git", "log", "--reverse", "--format=%H %ct", "HEAD"], cwd=ROOT,
                          capture_output=True, text=True).stdout.split("\n")
    out = []
    for row in filter(None, revs):
        rev, t = row.split()
        lines = source_lines(git_lines(rev))
        point = {}
        for g in games:
            ported = cited(lines[g["key"]], finders[g["key"]])
            total = sum(s for fs in g["funcs"].values() for _, s, _ in fs)
            done = sum(s for fs in g["funcs"].values() for _, s, n in fs if n in ported)
            point[g["key"]] = 100 * done / total
        out.append((datetime.datetime.fromtimestamp(int(t)), point))
    return out


def chart(games, points, path):
    W, H = 1600, 900
    img = Image.new("RGB", (W, H), BACK)
    d = ImageDraw.Draw(img)
    title, small = font(30), font(18)
    d.text((24, 18), "Adventures with Edison: the code ported, commit by commit", font=title, fill=TEXT)
    left, right, top, bottom = 90, W - 150, 80, H - 110
    d.rectangle([left, top, right, bottom], fill=HEAD)
    for pct in range(0, 101, 25):
        y = bottom - (bottom - top) * pct / 100
        d.line([left, y, right, y], fill=BACK)
        d.text((left - 10, y), f"{pct}%", font=small, fill=DIM, anchor="rm")
    t0, t1 = points[0][0], points[-1][0]
    span = max((t1 - t0).total_seconds(), 1)

    def px(t):
        return left + (right - left) * (t - t0).total_seconds() / span
    day = datetime.datetime(t0.year, t0.month, t0.day) + datetime.timedelta(days=1)
    while day <= t1:
        if day.weekday() == 0 or (t1 - t0).days < 15:
            d.line([px(day), bottom, px(day), bottom + 6], fill=DIM)
            d.text((px(day), bottom + 10), day.strftime("%b %d"), font=small, fill=DIM, anchor="ma")
        day += datetime.timedelta(days=1)
    ends = []
    for g in games:
        xy, last = [], None
        for t, p in points:
            y = bottom - (bottom - top) * p[g["key"]] / 100
            if last is not None:
                xy.append((px(t), last))  # steps: level till the next commit
            xy.append((px(t), y))
            last = y
        d.line(xy, fill=g["colour"], width=3)
        ends.append([last, g])
    # The last values beside the lines' ends, kept 22 pixels apart.
    ends.sort(key=lambda e: e[0])
    for i in range(1, len(ends)):
        ends[i][0] = max(ends[i][0], ends[i - 1][0] + 22)
    for y, g in ends:
        d.text((right + 10, y), f"{points[-1][1][g['key']]:.1f}%", font=small, fill=g["colour"], anchor="lm")
    x = left
    for g in games:
        d.rectangle([x, H - 50, x + 18, H - 32], fill=g["colour"])
        d.text((x + 26, H - 52), g["title"], font=small, fill=TEXT)
        x += 70 + d.textlength(g["title"], font=small)
    img.save(path, optimize=True)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--no-history", action="store_true", help="the treemap only")
    opts = ap.parse_args()
    OUT.mkdir(exist_ok=True)
    games, finders = [], {}
    src = source_lines(worktree_lines())
    for key, title, exe, _, notes, colour in GAMES:
        funcs = functions(exe)
        find = locate(funcs)
        finders[key] = find
        ported = cited(src[key], find)
        documented = cited((ROOT / notes).read_text(encoding="utf-8").splitlines(), find) - ported
        status = {n: "ported" for n in ported}
        status.update({n: "documented" for n in documented})
        games.append(dict(key=key, title=title, exe=exe, funcs=funcs, status=status, colour=colour))
        total, by = share(funcs, status)
        count = sum(len(fs) for fs in funcs.values())
        print(f"{title}: {count} functions, {total:,} bytes: {100 * by['ported'] / total:.1f}% ported, "
              f"{100 * by['documented'] / total:.1f}% documented only")
    treemap(games, OUT / "PROGRESS.png")
    print(f"wrote {OUT / 'PROGRESS.png'}")
    if not opts.no_history:
        points = history(games, finders)
        chart(games, points, OUT / "PROGRESS_CHART.png")
        print(f"wrote {OUT / 'PROGRESS_CHART.png'} ({len(points)} commits)")


if __name__ == "__main__":
    main()
