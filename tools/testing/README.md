# Testing helpers

Wild Science Arcade checks against the original under winevdm
(`tools/reference/otvdm.ps1`, `memwatch.py`, `tracecmp.py`). Their
outputs go to `build/scratch/` (ignored). They read the game's files from
`$EDISON_RUN` (as `otvdm.ps1` does; `WMAINSKP.EXE` from
`tools/reference/wmain_skip.py` goes there too), and run winevdm from
`$OTVDM` (its `otvdmw.exe`, unless it's on the PATH).

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
- `roompics.py [--cpp | --config]`: every table room's pictures (its
  method 4) and its builder's settings, from Ghidra's output and a dump of
  the original's data segment (`--dump`, default `build/scratch/dg1.bin`).

The port's test switches (environment): `SCI_DEBUG=1` logs the ball each
tick (and holes, targets); `SCI_RUNNER=1` the panel's walking figure
(for `tracecmp.py --port-pattern`); `SCI_TICKSHOTS=DIR` saves the display
after every tick (`DIR/t<tick>.bmp`: no frame missed, whatever the load);
`SCI_SHOOT_WHEN=cx,cy,cz,vx,vy,vz` holds a shot till the ball's in that
state; `SCI_HOLE=n` has the first room's hole to room n take the ball at
once (room 1: 504 EXIT's question, 508 / 509 the warp codes; 501-503 the
lab, high scores, credits), for what follows without a measured shot; `SCI_DIALOGPIC=k` gives the
framed boxes picture `1359` + k (the original picks one at random).
