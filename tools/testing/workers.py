"""Runs of the original side by side: each worker its own copy of the game
folder (build/scratch/workers/K, as EDISON_RUN) and otvdm.ps1's
OTVDM_WORKER=K (its game by process id, its window in its own tile), the
jobs dealt among the workers as each comes free.

    for name, k, result in deal(names, 4, lambda name, k, env: ...): ...

The job gets the worker's environment (os.environ with EDISON_RUN and
OTVDM_WORKER); it runs the original through a subprocess with it (env=),
never by changing os.environ, which the workers share.
"""
import os
import queue
import shutil
import threading
from pathlib import Path

from testlib import SCRATCH

# otvdm.ps1's tiles: five across, two down (a 3440 x 1440 screen).
MOST = 10


def folders(jobs):
    """The workers' copies of the game folder (EDISON_RUN's), refreshed."""
    main = Path(os.environ.setdefault("EDISON_RUN", "D:/tools/edison-run"))
    out = []
    for k in range(jobs):
        f = SCRATCH / "workers" / str(k)
        shutil.copytree(main, f, dirs_exist_ok=True, ignore=shutil.ignore_patterns("otvdm.pid"))
        out.append(f)
    return out


def environment(k, folder):
    return {**os.environ, "OTVDM_WORKER": str(k), "EDISON_RUN": str(folder)}


def deal(names, jobs, job):
    """job(name, k, env) for each name, JOBS at a time; yields (name, k,
    its result) as each ends (an exception is the result, raised here)."""
    jobs = max(1, min(jobs, MOST, len(names)))
    envs = [environment(k, f) for k, f in enumerate(folders(jobs))]
    todo = queue.Queue()
    for n in names:
        todo.put(n)
    done = queue.Queue()

    def work(k):
        while True:
            try:
                name = todo.get_nowait()
            except queue.Empty:
                return
            try:
                done.put((name, k, job(name, k, envs[k])))
            except BaseException as e:  # (raised in the caller's thread)
                done.put((name, k, e))

    threads = [threading.Thread(target=work, args=(k,), daemon=True) for k in range(jobs)]
    for t in threads:
        t.start()
    for _ in names:
        name, k, result = done.get()
        if isinstance(result, BaseException):
            raise result
        yield name, k, result
