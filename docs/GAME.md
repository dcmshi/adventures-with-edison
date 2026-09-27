# Game Logic Notes

Reverse-engineering notes for the game executables. Addresses are
`segment:offset` in the file's own numbering, as printed by
`tools/nedis.py` (for example `f04_0172` is the function at 4:0172).

## Programs

| File | Game | Segments | Notes |
|---|---|---|---|
| `EDISON.EXE` | Launcher / main menu | 36 code + DGROUP (seg 39) | Artech; menu scripts in `SHELL.D01` group 03 |
| `WINMAIN.EXE` | Rock and Bach | 62 code | Artech; drives `ADLIB*.DLL` |
| `WMAIN.EXE` | Wild Science Arcade | 91 code + 12 data | Different codebase: more data segments, FPU emulation (`WIN87EM`), `SADLIB` |
| `MALL.EXE` | Mystery Mall | 60 code | Artech |

All four are Borland C++ Win16 programs without debug information. EDISON,
WINMAIN and MALL share a common "Artech Library" (the upper segments):
the WinG surface code (`copy_area`, `show_fscreen`, `show_logo`, …,
named by their error strings), the multimedia timer and MCI playback
(`avivideo`, `sequencer`).

Each `*ARTDLL.DLL` (`ARTDLL`, `CARTDLL`, `MARTDLL`, `SARTDLL`) is a tiny
timer DLL: `SETUPTIMERDLL`, `TIMERCALLBACK` and `MINROUTINE` wrap
`timeSetEvent` so the music driver's `UPDATE_ADLIB` runs every 13 ms (see
`docs/SEQUENCER.md`).

## EDISON.EXE (launcher)

- **`f01_0000` WinMain.** Parses the command line (sets flags `[0054]`,
  `[3447]` and `[0056]`), calls `f01_0112` (InitInstance: registers the
  class and creates the window, plus a `WinArtechBackDropWindow` backdrop
  in `f15_1554`), then runs a `PeekMessage` loop. The loop calls `f01_02aa`
  when idle.
- **`f04_0172` main menu.** Its steps:
  - initialises the palette and surfaces and starts the timer (`f06_071a`);
  - checks for the CD by opening `<drive>\MYSTERY\blank.wav`;
  - runs the intro scripts, then loops on the menu. Each pass starts
    scripts (`f06_033e`), pumps messages (`f03_0172`) and polls the chosen
    button (`f04_0024`, where 4 means none).
  - `f02_007a` then launches the chosen game with `WinExec`:
    `winmain.exe -A`, `wmain.exe -A` or `mall.exe -A`.
- **`f04_0000`** is the menu's FM music: `CADLIB.SENDSND`.

### Script interpreter (SHELL.D01 group 03)

Scripts are compiled command lists, run cooperatively from the frame
loop. Decompiler: `tools/scripts.py`.

- **Container:** `u16 count, u16 offset[count]`. Each record is
  `u16 number, u16 statements, u16 code_size, u16 statement_offset[statements], code`.
  A loaded script is keyed by `archive_id | number << 16` (`f06_00fe`
  loads, `f06_00ae` finds).
- **Statement:** `u16 command, u16 present`, then 2-byte argument slots.
  Slot k is at `+4 + 2k` and was given if bit k of `present` is set.
- **Command table:** at `DS:0848`, 26 entries of `{name, far handler, template}`.
  - Handlers are all in segment 7 and are called as `handler(context, statement)`.
  - The templates are AmigaDOS ReadArgs syntax (`/A` required, `/N` number, `/K` keyword, `/S` switch).
  - Commands: `SHOWLOGO`, `SHOWCLOGO`, `CALL`, `SHOWFSCREEN`, `STARTANIM`,
    `STOPANIM`, `ADDBUTTONS`, `KILLSCRIPT`, `SETTEXTFONT`, `STARTSOUND`,
    `DRAWLINE`, `DRAWPOINT`, `SETDRAWPEN`, `SETTEXTPEN`, `COPYFSCREEN`,
    `WORKSCREEN`, `SLEEP`, `RESTART`, `COPYAREA`, `CLEARBUTTONS`,
    `DRAWTEXT`, `SETPALETTE`, `FADE`, `UNIQUE`, `MOUSE`, `TELL`.
- **Scheduler (`f06_0404`):**
  - Script contexts form a list at `[698C]`.
  - Context layout: `+0` next, `+2` flags (1 = sleeping until the time
    at `+3`, 2 = finished, 4 = waiting on a child), `+3` u32 wake tick,
    `+7` parent, `+9` script, `+B` statement index.
  - The tick counter is the u32 at `[42B6]:0`.
  - A handler's non-zero return value is passed back to the caller as an event.
- **`STARTANIM` (`g07_04da`) slots:**
  - 0: anim id (group 50)
  - 1, 2: x, y (-1 if not given)
  - 3: WAIT (the script waits for the anim to finish)
  - 4: ?
  - 5: repeat count
  - 6: flag
  - 7: WAV id (group 40) to play with it
  - It calls `g08_020a`, the anim player.
- **The templates in the EXE don't list every slot:** for example,
  STARTANIM uses slots 4–7 but its template names only 5 arguments.
- **Usage:** the shipped scripts (all 20 in SHELL) only sequence
  animations: `STARTANIM`, `STOPANIM`, `KILLSCRIPT`. The menu logic itself
  is C code.
