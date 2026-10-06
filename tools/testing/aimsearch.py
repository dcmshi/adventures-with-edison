"""SCI_AIMSEARCH split by rows over several copies of edison.exe at once,
muted and hidden, their boxes answered at once (SCI_SKIPDIALOGS). The aims
found, in the grid's order, to stdout; progress to stderr. A process still
running after --limit seconds (a search takes a few) is killed and reported
HUNG with its log's last lines.

Usage: aimsearch.py SPEC [--jobs N] [--limit S] -- EDISON ARGS...
  SPEC: to,power,x0,x1,y0,y1,step[,ball type[,wait]] (see README.md)
Example: aimsearch.py -6,-1,60,620,20,280,8 -- --game science --room 22"""
import argparse
import os
import re
import shutil
import sys

from testlib import SCRATCH, edison, parallel, run_game


def search(spec, game_args, jobs=None, limit=60, progress=None):
    """The aims' log lines ("aim x,y power p: ..."), in the grid's order.
    Raises RuntimeError if a process hangs or the room has no ball."""
    progress = progress or Quiet()
    fields = spec.split(",")
    to, power, x0, x1, y0, y1 = fields[:6]
    step = int(fields[6]) if len(fields) > 6 else 1
    rest = fields[7:]
    y0, y1 = int(y0), int(y1)
    rows = list(range(y0, y1 + 1, step))
    jobs = max(1, min(jobs or (os.cpu_count() or 2) // 2, len(rows)))
    per = -(-len(rows) // jobs)
    chunks = [rows[i:i + per] for i in range(0, len(rows), per)]
    out = SCRATCH / f"aimsearch.{os.getpid()}"
    exe = edison(out)
    print(f"({len(chunks)} processes searching {len(rows)} rows)", file=progress, flush=True)

    def one(j, chunk):
        sub = ",".join([to, power, x0, x1, str(chunk[0]), str(chunk[-1]), str(step), *rest])
        return run_game(exe, game_args, out / f"{j}.log", limit=limit,
                        env={"SCI_SKIPDIALOGS": "1", "SCI_AIMSEARCH": sub})

    found, problems = {}, []
    for j, run in parallel([(j, lambda j=j, c=c: one(j, c)) for j, c in enumerate(chunks)]):
        label = f"rows {chunks[j][0]}-{chunks[j][-1]}"
        if run.hung:
            problems.append(run.hung_report(label))
            print(problems[-1], file=progress, flush=True)
            continue
        text = run.log.read_text(errors="replace")
        found[j] = [l for l in text.splitlines() if l.startswith("aim ")]
        note = re.search(r"SCI_AIMSEARCH: .*(has no ball|isn't a table room).*", text)
        print(f"{label}: {len(found[j])} aims ({run.seconds:.0f} s){': ' + note.group(0) if note else ''}",
              file=progress, flush=True)
        if note:
            problems.append(note.group(0))
    shutil.rmtree(out, ignore_errors=True)
    if problems:
        raise RuntimeError("\n".join(problems))
    return [l for j in sorted(found) for l in found[j]]


class Quiet:
    def write(self, s):
        pass

    def flush(self):
        pass


def main():
    argv = sys.argv[1:]
    game = []
    if "--" in argv:
        i = argv.index("--")
        argv, game = argv[:i], argv[i + 1:]
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("spec")
    ap.add_argument("--jobs", type=int)
    ap.add_argument("--limit", type=int, default=60)
    opts = ap.parse_args(argv)
    try:
        aims = search(opts.spec, game, opts.jobs, opts.limit, progress=sys.stderr)
    except RuntimeError:
        return 1
    print("\n".join(aims))
    return 0


if __name__ == "__main__":
    sys.exit(main())
