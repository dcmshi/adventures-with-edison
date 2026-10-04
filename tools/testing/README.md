# Testing helpers

Wild Science Arcade checks against the original under winevdm
(`tools/reference/otvdm.ps1`, `memwatch.py`, `tracecmp.py`). Their
outputs go to `build/scratch/` (ignored). They read the game's files from
`$EDISON_RUN` (as `otvdm.ps1` does; `WMAINSKP.EXE` from
`tools/reference/wmain_skip.py` goes there too).

- `trace.sh NAME "ms x y hold;..." [ms]`: the same presses (times from the
  room's start) in the port (`SCI_DEBUG` log) and in the original
  (`WMAINSKP.EXE`, traced by `memwatch.py`), then `tracecmp.py`.
- `regress.sh`: room 1 at rest, sliders, aims, ball types and a shot
  against the original's reference shots (`build/scratch/orig_*`, taken
  with `otvdm.ps1 run`; not in the repository).
- `cmp.py PORTDIR ORIGDIR MS:NAME...`: view and panel differences of the
  port's capture at a time against an original screenshot.
- `bestframe.py PORTDIR SHOT.png [x0 y0 x1 y1]`: the port's capture nearest
  an original screenshot (capture every 10 ms: 20 can skip a tick).
- `dis.sh SEG OFF [LINES] [BITS]`, `fn.sh SEL_OFF...` (a function from
  Ghidra's `extracted/ghidra/wmain.c`), `thunks.py SEG OFF...` (where
  method-table thunks jump): reading WMAIN.EXE.
