"""The coverage run: the original plays the comparison scenarios with a
tripwire (lcall 0000:0000, tools/reference/tripwire.py) at the start of
every function deadscan.py finds dead, so any of them that runs makes the
original fault there, and winevdm's report (otvdm.ps1's OTVDM_LOG) says
which. Static analysis finds the candidates; this confirms none of them
runs in play.

  python tools/testing/coverage.py GAME [SCENARIO-PREFIX ...] [--classes C ...] [--iterate] [--list] [--also F ...]

GAME: mystery (mmcompare.py's scenarios), rockbach (rbcompare.py's),
science (scenarios.py's). --classes: deadscan's classes to arm (default
unreferenced, dead chain, table only). --iterate: after a hit, disarm that
function and play the scenario again, till it runs clean. --list: write
and print the tripwires only. --also F...: arm live functions too, the
check that a hit is found (science rest-1 --also f27_16ae: HIT).

A function's tripwire (5 bytes) is at its label: one entered through an
entry prologue just before it (a callback's: mov ax, ss; nop) runs into
it too. None on a relocation (the loader would write over it), none on
data nedis took for code (a tripwire there broke Wild Science's type
records). winevdm's report has the faulting call's frame,
"cs:ip=SEL:OFF (call 0000:0000)", OFF 5 past the tripwire; segment n's
selector is the module's handle + 60h + 8 (n - 1) (calibrated on WMAIN:
27:16AE at 12CF, 32:0777 at 12F7, the handle 119F). Not ud2 or int 3:
winevdm steps over those and runs on, so a function could run unseen.

Out: build/scratch/coverage/GAME/: the tripwires (tripwires/EXE.txt), each
scenario's winevdm report (NAME.log, NAME.log.err), hits.txt; a summary
last. The test copies are written again unarmed at the end.

--jobs N: N copies of the original side by side (otvdm.ps1's
OTVDM_WORKER), the scenarios dealt among them: each worker a copy of the
game folder (build/scratch/workers/K, EDISON_RUN), its own tripwires
(tripwiresK), runs (runsK) and hits (hits-K.txt), its window in its own
tile of the screen. A hit doesn't depend on timing; but a slower original
can take a click late, so a scenario can reach less than alone.
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import deadscan as D  # noqa: E402
import progress as P  # noqa: E402,F401 (deadscan's path)
import workers as W  # noqa: E402
from ne import NEFile  # noqa: E402
from testlib import REFERENCE, SCRATCH, say  # noqa: E402

OUT = SCRATCH / "coverage"
TRAP = 5
# A function's first bytes: Borland's prologues (push bp; mov bp, sp / mov
# ax, ss or ds; nop / inc bp; push bp / push ds; pop ax; nop / enter). What
# starts otherwise is data nedis took for code (Wild Science's RTTI records
# and thunk tables, "0A 00 03 ...").
CODE_STARTS = ("558bec", "8cd090", "8cd890", "45558b", "1e5890", "c8")
GAMES = {"mystery": ("MALL.EXE", "mmcompare"), "rockbach": ("WINMAIN.EXE", "rbcompare"), "science": ("WMAIN.EXE", "scenario")}


def run_folder():
    return Path(os.environ.setdefault("EDISON_RUN", "D:/tools/edison-run"))


def tripwires(game, classes, also=()):
    """[(seg, off, name)] to arm, and [(name, why)] left out; also: live
    functions armed too (a check that a hit is found)."""
    exe = GAMES[game][0]
    rows, _ = D.classify(game)
    rows = list(rows) + [("also", n, 99, "", set()) for n in also]
    classes = set(classes) | {"also"}
    ne = NEFile(str(D.EXE_DIR / exe))
    sites = {}
    for s in ne.segments:
        if not s["data"]:
            covered = set()
            for r in ne.relocations(s["index"]):
                for site in r["sites"]:
                    covered.update(range(site, site + 4))
            sites[s["index"]] = covered
    out, skipped = [], []
    for cls, name, size, _, _ in rows:
        if cls not in classes:
            continue
        seg, off = int(name[1:3]), int(name[4:], 16)
        code = ne.segment_bytes(seg)
        if not code[off:off + 3].hex().startswith(CODE_STARTS):
            skipped.append((name, f"not code ({code[off:off + 3].hex()}: data)"))
        elif size < TRAP or off + TRAP > len(code):
            skipped.append((name, "too short for a tripwire"))
        elif set(range(off, off + TRAP)) & sites.get(seg, set()):
            skipped.append((name, f"{seg:02d}:{off:04X} is a relocation"))
        else:
            out.append((seg, off, name))
    return out, skipped


def write_list(folder, exe, armed):
    folder.mkdir(parents=True, exist_ok=True)
    (folder / f"{exe}.txt").write_text("".join(f"{s:02d}:{o:04X} {n}\n" for s, o, n in armed), encoding="utf-8")


def module(exe):
    """The executable's module name, as winevdm's report lists it (the
    resident names' first: Rock and Bach's WINMAIN.EXE is MAIN)."""
    ne = NEFile(str(D.EXE_DIR / exe))
    pos = ne.ne + int.from_bytes(ne.data[ne.ne + 0x26:ne.ne + 0x28], "little")
    return ne.data[pos + 1:pos + 1 + ne.data[pos]].decode("latin-1")


def base(text, exe):
    """Segment 1's selector in the report: the module's handle + 60h."""
    m = re.search(rf"^\s*([0-9a-f]{{4}})\s+[0-9a-f]{{4}}\s+{re.escape(module(exe))}\s", text, re.M)
    return int(m.group(1), 16) + 0x60 if m else None


def hit(log, armed, exe):
    """The armed function the report names, or None (no fault), or "?" (a
    fault at nothing armed)."""
    # (winevdm writes its report to its standard error: OTVDM_LOG.err.)
    text = "".join(p.read_text(encoding="utf-8", errors="replace")
                   for p in (log, Path(f"{log}.err")) if p.exists())
    if "cs:ip=" not in text:
        return None
    first = base(text, exe)
    by_place = {(s, o): n for s, o, n in armed}
    for sel, off in re.findall(r"cs:ip=([0-9a-f]{4}):([0-9a-f]{4})[^\n]*\(call 0000:0000\)", text):
        sel, off = int(sel, 16), int(off, 16) - TRAP
        if first is not None and (sel - first) % 8 == 0:
            name = by_place.get(((sel - first) // 8 + 1, off))
            if name:
                return name
        # The base not found: the offset alone, if only one has it.
        names = {n for (s, o), n in by_place.items() if o == off}
        if len(names) == 1:
            return names.pop()
    return "?"


def worker():
    """This process's worker number (otvdm.ps1's OTVDM_WORKER), or None."""
    w = os.environ.get("OTVDM_WORKER")
    return int(w) if w else None


def kill():
    """The game killed: a worker's own (otvdm.ps1's otvdm.pid), else every
    otvdmw.exe."""
    if worker() is None:
        subprocess.run(["taskkill", "/F", "/IM", "otvdmw.exe"], capture_output=True)
        return
    pid = run_folder() / "otvdm.pid"
    if pid.exists():
        subprocess.run(["taskkill", "/F", "/PID", pid.read_text().strip()], capture_output=True)


def play(game, name, s, log):
    """Plays one scenario in the original with OTVDM_LOG set, its shots in
    coverage's own folder: the runner's play_orig empties NAME/orig under
    its OUT first, and there they are the comparisons' references (the
    first coverage run lost six of Wild Science's)."""
    os.environ["OTVDM_LOG"] = str(log)
    module = __import__(GAMES[game][1])
    kept = module.OUT
    # (A worker's own: scenario.py keeps the save files in OUT/saves.)
    module.OUT = OUT / game / f"runs{'' if worker() is None else worker()}"
    try:
        if game == "science":
            # origrun.py starts WMAINSKP.EXE as it is: armed here.
            subprocess.run([sys.executable, str(REFERENCE / "wmain_skip.py")], check=True, stdout=subprocess.DEVNULL)
        module.play_orig(name, s)
    finally:
        module.OUT = kept
        os.environ.pop("OTVDM_LOG", None)
        # A run that faulted can outlive otvdm.ps1's stop (winevdm still
        # holding the copy open, which then can't be written again).
        kill()


def scenarios(game):
    if game == "science":
        from scenarios import SCENARIOS
        return SCENARIOS
    return __import__(GAMES[game][1]).SCENARIOS


def restore():
    """The test copies written again without tripwires."""
    os.environ.pop("EDISON_TRIPWIRES", None)
    # (Rock and Bach's and Mystery's runners write their copy for each
    # scenario: written here once more, for whoever starts one by hand.)
    for script in (["wmain_skip.py"], ["free_mouse.py"], ["winmain_skip.py", "2"], ["mall_skip.py"]):
        subprocess.run([sys.executable, str(REFERENCE / script[0]), *script[1:]], check=True, stdout=subprocess.DEVNULL)


def side_by_side(opts, names, jobs, out):
    """The scenarios dealt among JOBS workers (workers.py) as each comes
    free: a child coverage.py a scenario (--only), with the worker's
    environment. [(name, function)], [name]."""
    results = out / "results"
    shutil.rmtree(results, ignore_errors=True)
    results.mkdir(parents=True)

    def job(name, k, env):
        cmd = [sys.executable, __file__, opts.game, "--only", name, "--classes", *opts.classes]
        if opts.also:
            cmd += ["--also", *opts.also]
        if opts.iterate:
            cmd.append("--iterate")
        return subprocess.run(cmd, env=env, capture_output=True, text=True, errors="replace")

    for name, k, p in W.deal(names, jobs, job):
        lines = [l for l in p.stdout.splitlines() if l.startswith(name + ":")]
        say(f"[{k}] " + ("\n    ".join(lines) or f"{name}: no result (exit {p.returncode})"))
        if not lines:
            (out / f"{name}.worker.txt").write_text(p.stdout + p.stderr, encoding="utf-8")
    hits, unknown = [], []
    for n in names:
        r = results / f"{n}.txt"
        if not r.exists():
            unknown.append(n)
            continue
        for line in r.read_text(encoding="utf-8").splitlines():
            what, _, f = line.partition(" ")
            if what == "hit":
                hits.append((n, f))
            elif what == "unknown":
                unknown.append(n)
    (out / "hits.txt").write_text("".join(f"{n} {f}\n" for n, f in hits), encoding="utf-8")
    return hits, unknown


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("game", choices=list(GAMES))
    ap.add_argument("names", nargs="*", help="scenario name prefixes (default: all)")
    ap.add_argument("--classes", nargs="+", default=["unreferenced", "dead chain", "table only"])
    ap.add_argument("--iterate", action="store_true")
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--also", nargs="+", default=[], metavar="FUNCTION",
                    help="arm these live functions too: the check that a hit is found")
    ap.add_argument("--jobs", type=int, default=1, help="copies of the original side by side")
    ap.add_argument("--only", help=argparse.SUPPRESS)  # (a worker's one scenario)
    opts = ap.parse_args()
    exe = GAMES[opts.game][0]
    out = OUT / opts.game
    w = worker()
    folder = out / f"tripwires{'' if w is None else w}"
    armed, skipped = tripwires(opts.game, set(opts.classes), opts.also)
    write_list(folder, exe, armed)
    if not opts.only:
        say(f"{opts.game}: {len(armed)} functions armed, {len(skipped)} left out"
            + "".join(f"\n  left out {n}: {why}" for n, why in skipped))
    if opts.list:
        for s, o, n in armed:
            say(f"  {s:02d}:{o:04X} {n}")
        return 0
    os.environ.setdefault("OTVDM", "D:/tools/otvdm/otvdm-v0.9.0/otvdmw.exe")
    run_folder()
    every = scenarios(opts.game)
    if opts.only:
        names = [opts.only]
    else:
        names = [n for n in every if not opts.names or any(n.startswith(p) for p in opts.names)]
    if opts.jobs > 1 and not opts.only:
        hits, unknown = side_by_side(opts, names, opts.jobs, out)
    else:
        os.environ["EDISON_TRIPWIRES"] = str(folder)
        hits, unknown = [], []
        try:
            for name in names:
                while True:
                    log = out / f"{name}.log"
                    log.unlink(missing_ok=True)
                    Path(f"{log}.err").unlink(missing_ok=True)
                    play(opts.game, name, every[name], log)
                    h = hit(log, armed, exe)
                    if h is None:
                        say(f"{name}: clean")
                        break
                    if h == "?":
                        say(f"{name}: a fault at nothing armed (see {log}.err)")
                        unknown.append(name)
                        break
                    say(f"{name}: HIT {h}")
                    hits.append((name, h))
                    if not opts.only:
                        (out / "hits.txt").write_text("".join(f"{n} {f}\n" for n, f in hits), encoding="utf-8")
                    if not opts.iterate:
                        break
                    armed = [a for a in armed if a[2] != h]
                    write_list(folder, exe, armed)
        finally:
            if opts.only:
                # (A worker's result, for side_by_side; its folder a scratch copy.)
                (out / "results" / f"{opts.only}.txt").write_text(
                    "".join(f"hit {f}\n" for _, f in hits) + ("unknown\n" if unknown else "") + "done\n",
                    encoding="utf-8")
            else:
                restore()
        if opts.only:
            return 1 if hits or unknown else 0
    say(f"coverage {opts.game}: {len(names)} scenarios; {len(hits)} hits"
        + (f" ({', '.join(sorted({f for _, f in hits}))})" if hits else "")
        + (f"; faults at nothing armed in {', '.join(unknown)}" if unknown else ""))
    return 1 if hits or unknown else 0


if __name__ == "__main__":
    sys.exit(main())
