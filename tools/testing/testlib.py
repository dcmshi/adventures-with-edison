"""Shared helpers for the port's test scripts: running edison.exe muted,
hidden and on the virtual clock, each run limited in time (a run still
going at its limit is killed and reported HUNG with its log's last lines),
and progress printed as runs finish."""
import os
import re
import shutil
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SCRATCH = ROOT / "build" / "scratch"
REFERENCE = ROOT / "tools" / "reference"


def edison(out_dir):
    """A copy of the build's edison.exe in out_dir (so the build can go on
    meanwhile); E=path tests another build."""
    src = Path(os.environ.get("E", ROOT / "build" / "engine" / "edison.exe"))
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    exe = out_dir / "edison.exe"
    shutil.copy2(src, exe)
    return exe


def quit_after(args):
    """The --quit-after in args (ms), or None."""
    for i, a in enumerate(args[:-1]):
        if a == "--quit-after":
            return int(args[i + 1])
    return None


def limit_for(args, default_ms=30000):
    """Seconds a run may take: its --quit-after (virtual ms, which runs at
    least as fast as wall time) plus 20."""
    return (quit_after(args) or default_ms) // 1000 + 20


def log_tail(log, n=3, skip=(" sprite ", "bodies ")):
    """The log's last n lines, leaving out the per-tick noise."""
    try:
        lines = Path(log).read_text(errors="replace").splitlines()
    except OSError:
        return ["(no log)"]
    lines = [l for l in lines if not any(s in l for s in skip)]
    return lines[-n:] or ["(empty log)"]


class Run:
    """One finished run: hung, its exit code, how long it took, its log."""

    def __init__(self, hung, code, seconds, log, limit):
        self.hung, self.code, self.seconds, self.log, self.limit = hung, code, seconds, log, limit

    def hung_report(self, name):
        tail = "\n".join("    " + l for l in log_tail(self.log))
        return f"{name}: HUNG (killed after {self.limit} s); its log ends:\n{tail}"


def run_game(exe, args, log, env=None, limit=None, hidden=True):
    """Runs exe with args (test flags added), its log to `log` (removed
    first: EDISON_LOG appends)."""
    log = Path(log)
    log.parent.mkdir(parents=True, exist_ok=True)
    log.unlink(missing_ok=True)
    # (SCI_TRUECOLOR: no colour cycles, as the original under winevdm,
    # which the tests compare with.)
    full_env = dict(os.environ, EDISON_LOG=str(log), SCI_TRUECOLOR="1")
    full_env.update(env or {})
    flags = ["-A", "--hidden", "--virtual-clock"] if hidden else []
    limit = limit or limit_for(args)
    start = time.monotonic()
    try:
        code = subprocess.run([str(exe), *flags, *args], env=full_env, cwd=ROOT,
                              stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                              timeout=limit).returncode
        hung = False
    except subprocess.TimeoutExpired:
        code, hung = None, True
    return Run(hung, code, time.monotonic() - start, log, limit)


def parallel(jobs, workers=None):
    """Runs (name, fn) jobs at once, yielding (name, result) as each ends."""
    workers = workers or max(1, min(len(jobs), os.cpu_count() or 4))
    with ThreadPoolExecutor(workers) as pool:
        futures = {pool.submit(fn): name for name, fn in jobs}
        for f in as_completed(futures):
            yield futures[f], f.result()


def python(script, *args):
    """Output of one of the reference tools (tools/reference/<script>)."""
    r = subprocess.run([sys.executable, str(REFERENCE / script), *map(str, args)],
                       capture_output=True, text=True, cwd=ROOT)
    return r.stdout


def say(*parts):
    print(*parts, flush=True)
