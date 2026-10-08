# Testing helpers

Wild Science Arcade checks against the original under winevdm
(`tools/reference/otvdm.ps1`, `memwatch.py`, `tracecmp.py`). Their
outputs go to `build/scratch/` (ignored). They read the game's files from
`$EDISON_RUN` (as `otvdm.ps1` does; `WMAINSKP.EXE` from
`tools/reference/wmain_skip.py` goes there too), and run winevdm from
`$OTVDM` (its `otvdmw.exe`, unless it's on the PATH).

- Other rooms in the original: `otvdm.ps1 type S02` (the arcade's S key
  and two digits; `type` holds Shift for capitals), then the shot. The
  ball's remainders for `memwatch.py`: `[[[5ffc+ae]+f77]+2]+68` (`+6a`,
  `+6c`). `memwatch.py` finds the ball when it starts: start it after the
  room is up.
- The original's sound: `otvdm.ps1` scripts mute the game in the Windows
  mixer as soon as it plays a sound (`EDISON_VOLUME`, 0-100, default 0;
  `otvdm.ps1 volume N` while it runs; Windows keeps the level for
  `otvdmw.exe`).
- `EDISON_LOG` appends: remove the file before a run whose log is read.
- `python tools/testing/check.py [unit regress retrace smoke]`: every
  check below, each timed and limited (a step still running at its limit
  is reported HUNG), PASS or FAIL each, exit 1 if any failed; about 10 s.
  Run it before a commit.
  `smoke`: the aim search finds room 22's suckhole, room 96's greeting box
  shows in the heartbeat when unanswered and is skipped with
  `SCI_SKIPDIALOGS`.
- Test runs of the port: `--hidden` (no window, no sound, no drawing but
  the captures) and `--virtual-clock` (1 ms per event pump: the inputs'
  times, the captures and the ticks the same on every run however busy
  the machine, and as fast as it goes; a sample plays for its length).
  The Python scripts below (their helpers in `testlib.py`) run their cases
  at once, each a copy of `edison.exe` (the build can go on), each killed
  at its `--quit-after` plus 20 s and then reported HUNG with its log's
  last lines.
- `retrace.py [--accept] [NAME...]`: replays the shots checked before in
  the port and compares them with the original's traces kept in
  `build/scratch` (holes' spits, the lips, the magnets of rooms 3, 13 and
  33, the levers of rooms 61 and 62 and their powered magnets, room 29's
  bullseye, room 25's electromagnet catching an Iron ball and breaking
  Glass and Rubber ones, the fans of rooms 98 and 25 breaking a Rubber
  ball and melting an Ice one, room 22's suckhole: its spark breaking the
  ball, room 46's smiley breaking a Rubber ball and scoring a Glass one,
  room 71's rack of balls, room 32's hot field burning the ball, room 13 with a type 2 ball added: its copies of the game's folders in `build/scratch/t2`, room 96's pit, room 69's bin, room 2's circuit and gate, room 55's magnetic ball into its pit, room 54's press going down); each result against `retrace.expected`
  (`--accept` writes the new ones there once they're checked).
- Tracing across a change of room: `memwatch.py watch --follow 0.3` (the
  data segment moves as the room is built). Expressions are hex: a list
  entry i is at `[[5ffc+ae]+18e]+` 2i in hex. The room's object list
  (`+18E`, count `+190`) is in the order the objects were made: the ball,
  its shadow and target, then the file's objects.
- `trace.sh NAME "ms x y hold;..." [ms]`: the same presses (times from the
  room's start) in the port (`SCI_DEBUG` log) and in the original
  (`WMAINSKP.EXE`, traced by `memwatch.py`), then `tracecmp.py`.
- `regress.py`: room 1 at rest, sliders, aims, ball types and a shot
  against the original's reference shots (`build/scratch/orig_*`, taken
  with `otvdm.ps1 run`; not in the repository): every frame exact, the
  two whole screens no worse than their known counts. `E=path` tests
  another build (also for `retrace.py`).
- `scenario.py [NAME...] [--orig] [--accept] [--list]`: the scenarios of
  `scenarios.py` (room 1's holes to the lab, the play room and Level 1;
  lessons 6-10 from room 61; the game over in room 32 and won from room
  65), each a timeline of presses, drags, moves and typing from a table
  room, played in the port and (with `--orig`, one at a time, its save
  files put back after) in the original; each of the original's shots
  against the port's nearest frame within 1.5 s, the columns masked, no
  worse than its known count (`build/scratch/scenario/known.json`,
  `--accept` writes it). Both sides start from the original's
  `WSCIENCE.HS` and `wscience.edi`. Without `--orig` it uses the
  original's shots of the last run (none: it only reports), so it runs in
  `check.py`.
- `rbcompare.py [NAME...] [--port-only | --compare-only] [--list]`: Rock
  and Bach's scenarios (the jukebox, the Drum Clinic, the Music Library,
  Harmony Hall, the Instrument Room, Sound FX's list, the Studio's EDIT
  after a video), each a timeline of clicks and holds in an activity,
  played in the port (`--level N`; in real time when the music's events
  matter) and in the original (`WINMSKIP.EXE`, written for each by
  `tools/reference/winmain_skip.py`); each of the original's shots
  against the port's closest frame within 1.5 s, the diffs to
  `build/scratch/rbcompare/`. Needs the CD image mounted (the
  instruments' and effects' WAVs). Not in `check.py`: it only reports
  (`docs/ROCKBACH.md` lists what always differs).
- `aimsearch.py SPEC [--jobs N] [--limit S] -- ARGS...`: `SCI_AIMSEARCH`
  split by rows over N processes (default half the cores), dialogs
  skipped; progress per process to stderr, the aims found to stdout; a
  process past S seconds (default 60) is reported HUNG with its log's
  last lines; a room with no ball, or not a table room, fails at once.
  `--power`, `--gravity`, `--friction` take values to search as well
  ("-16,-8,0" or "-16:4:4"), each combination in turn, the aims found
  prefixed with them (a slider the room locks is left, with a warning:
  `SCI_AIMSEARCH_GRAVITY` / `_FRICTION`); `SCI_AIMSEARCH_SWITCHES=i,j`
  turns those objects of the room (switches) over before each shot.
- `cmp.py PORTDIR ORIGDIR MS:NAME...`: view and panel differences of the
  port's capture at a time against an original screenshot.
- `bestframe.py PORTDIR SHOT.png [x0 y0 x1 y1]`: the port's capture nearest
  an original screenshot (capture every 10 ms: 20 can skip a tick).
- `calltrace.py NAME...`: a function's far calls with their arguments
  resolved (constants, `[bp-N]` words, `&(x, y)` pairs, earlier results
  as `#n`), read straight through: for transcribing the rooms' builders.
- `wm.py` reads WMAIN.EXE, functions named as in the docs (`f28_15a1`):
  `fn NAME...` (Ghidra's decompilation, or the disassembly where Ghidra
  has none), `dis NAME [LINES]` (ndisasm to the function's end, far calls
  resolved: `call far f08_15d2`, `USER.ClipCursor`), `calls NAME` (the far
  calls it makes), `xref NAME` (its callers and every far pointer to it),
  `vtable OFF [N] [OFF2]` (a method table, thunks followed; two side by
  side, the methods that differ marked), `thunk NAME...`, `ds OFF [N]`
  (data segment words).
- `origrun.py OUTDIR ROOM [--watch S] [SCRIPT LINE...]`: a room in the
  original (`WMAINSKP.EXE`) with an `otvdm.ps1` script once it's up, the
  ball and `[FFE]` traced (`ball.txt`); every step time-limited.
  `WMAINSKP.EXE` (`tools/reference/wmain_skip.py`) also leaves the mouse
  free (the original confines it to its window, `f74_0000`). `--follow S`
  traces from before the room is built (its data segment moves). A room
  with a greeting box needs a `click` first. `otvdm.ps1 start` also starts
  a guard that keeps re-enabling windows the game's dialogs disable (the
  terminal in front when a box opens), so a run waiting on one doesn't
  lock the user out.
- `roompics.py [--cpp | --config]`: every table room's pictures (its
  method 4) and its builder's settings, from Ghidra's output and a dump of
  the original's data segment (`--dump`, default `build/scratch/dg1.bin`).

The port's test switches (environment): `SCI_BALLS=l,r` the columns' balls to start with (`0,0`: the next ball lost is the game over); `SCI_TRUECOLOR=1` no colour cycles (as the original under winevdm; `testlib.py` sets it for every test run); `SCI_DEBUG=1` logs the ball each
tick (centre, velocity and remainder: `c`, `v`, `r`) (and holes, targets); `SCI_RUNNER=1` the panel's walking figure
(for `tracecmp.py --port-pattern`); `SCI_TICKSHOTS=DIR` saves the display
after every tick (`DIR/t<tick>.bmp`: no frame missed, whatever the load);
`SCI_SKIPDIALOGS=1` answers every box with its first button at once
(logged: "dialog (message XXXX) skipped"); with `EDISON_LOG` set, a
heartbeat every 2 s (of the game's clock) says what the game is doing
("room R tick T ball x,y,z", or "waiting in dialog (message XXXX)"), so a
run that hangs shows where;
`SCI_SHOOT_WHEN=cx,cy,cz,vx,vy,vz` holds a shot till the ball's in that
state; `SCI_HOLE=n` has the first room's hole to room n take the ball at
once (room 1: 504 EXIT's question, 508 / 509 the warp codes; 501-503 the
lab, high scores, credits), for what follows without a measured shot; `SCI_GAMETICKS=n` starts `[27B4]` (event 4's count: room 2's gate, the colour cycles) at n; `SCI_RANDSEED=a[,b]` sets Borland's rand() seed as the room is built (and to b once it's built), as read from the original (`d:8454`); `SCI_ROOMTICKS=n` starts `[FFE]` (the room
ticks, which the original counts from its start: the targets' and other
animations' phases) at n, as read from the original (`memwatch.py`
`t=d:ffe`); `SCI_DIALOGPIC=k` gives the
framed boxes picture `1359` + k (the original picks one at random); `SCI_AIMSEARCH=to,power,x0,x1,y0,y1,step`
plays every aim of the grid (screen points, power -1 the room's own) from
the room as built, without drawing, and logs those whose ball a hole
leading to room `to` takes ("aim x,y power p: hole to at tick t"; a
negative `to`: the first point target of kind -`to` hit, -100 the
ball near a magnet, -101 a switch turned over, -102 the ball broken (logged as heated by a
fan, zapped by an electromagnet, or broken) or caught by an electromagnet, -103 a
smiley (type 11) met, -104 a loose ball (type 0) or block (type 16) moved,
-105 a block met, -107 the room's `+FC0` set (room 96's pit, room 2's circuit), -108 the room's own code ending it (a bin, a goal), -109 room 55's `+FA2` or `+FA4` set (a magnetic ball onto its box); an eighth
number sets the ball type, a ninth the ticks played before each shot): for
bank shots to replay in the original (with `--click`s for a greeting
first; dialogs are skipped; it ends the game when done, or at once if
the room has no ball).
