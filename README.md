# Adventures with Edison — Native Reimplementation

A clean-room, native reimplementation of the engine behind *Corel's Adventures with Edison* (1995, developed by Artech Studios), so the original games can run on modern systems without Windows 3.1 emulation.

**This repository contains no original game data.** You need your own copy of the CD. Place the ISO in `original/` (ignored by git).

## Games on the disc

| Executable | Game |
|---|---|
| `EDISON.EXE` | Main menu shell |
| `WINMAIN.EXE` | Rock and Bach (music maker) |
| `WMAIN.EXE` | Wild Science Arcade |
| `MALL.EXE` | Mystery Mall |

## Layout

- `original/`: your ISO and extracted CD contents (git-ignored)
- `extracted/`: assets unpacked by our tools (git-ignored)
- `tools/`: asset extraction and format-analysis scripts (`pip install -r requirements.txt`)
- `engine/`: the native reimplementation
- `docs/`: file-format and engine notes

## Building

Requires CMake 3.20+, a C++17 compiler (GCC, Clang or MSVC) and git; SDL3 and
Nuked-OPL3 are downloaded and built automatically at configure time.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Launcher

`edison` is the native version of EDISON.EXE: the opening, the main menu and Mystery at the Museums (the whole game), with FM music and sound effects. Rock and Bach is ported; Wild Science Arcade has started (`--game science`: its title, story and laboratory). Open items are tracked in [TODO.md](TODO.md).

```sh
build/engine/edison original/cd/DSK3        # -O skips the opening, -A turns FM music off
```

### Music player

`fmplay` plays the game's FM music and sound effects straight from its DLLs,
at the game's music rate (72 updates per 76 ticks of a 13 ms timer, about 72.9 Hz):

```sh
build/engine/fmplay original/cd/DSK3/ADLIB.DLL                  # list sounds by name
build/engine/fmplay original/cd/DSK3/ADLIB.DLL SUNROCK1 SUNROCK2 SUNROCK3 SUNROCK4                     SUNROCK5 SUNROCK6 SUNROCK7 SUNROCK8          # a Rock and Bach band
build/engine/fmplay original/cd/DSK3/MADLIB.DLL DROPTILE --wav droptile.wav
```

### Sequencer test

`seqtest` replays every sound through the native FM driver and compares the
OPL register stream with reference logs recorded from the original driver:

```sh
python -m venv .venv && .venv/Scripts/pip install -r requirements.txt
.venv/Scripts/python tools/oplref.py ADLIB.DLL ADLIB1.DLL ADLIB2.DLL ADLIB3.DLL ADLIB4.DLL CADLIB.DLL MADLIB.DLL
.venv/Scripts/python tools/oplfuzz.py            # synthetic songs covering every opcode
ctest --test-dir build --output-on-failure
```

## Reverse-engineering tools

- `tools/nedis.py FILE.EXE`: whole-program disassembly with Windows imports named, cross-segment calls resolved and string references shown. It writes `extracted/disasm/<exe>.asm`, plus `<exe>.funcs.txt`, a one-line-per-function summary to grep.
- `tools/ghidra/decompile.sh FILE.EXE ...`: headless Ghidra decompilation to `extracted/ghidra/<exe>.c`.
  - Imports from the CD's DLLs are named, and functions found by `nedis.py` are added.
  - Needs [Ghidra](https://github.com/NationalSecurityAgency/ghidra) 12 and a JDK 21. It expects them in `D:/tools`; otherwise set `GHIDRA` and `JAVA_HOME`.
- `tools/scripts.py`: decompiles the menu scripts (see `docs/GAME.md`).
- `tools/reference/otvdm.ps1 start|shot|dialogs|stop|unlock|click|rclick|down|up|move|type|run`: runs the original game under [winevdm](https://github.com/otya128/winevdm) as a visual reference.
  - `click`, `rclick`, `down`/`up` (hold the button), `move` and `type` post input to the game's window (the real cursor isn't moved); `run SCRIPT DIR` plays a script of those, `wait` and `shot` steps (see `tools/reference/*.txt`).
  - `tools/reference/screendiff.py PORT.bmp ORIGINAL.png OUT.png` compares a port capture (`--capture`) with a screenshot of the original.
  - `tools/reference/memwatch.py find|peek|watch|dump EXE ...` reads the running original's memory: it finds the program's live data segment (DGROUP) in the otvdmw process by the executable's own static data, then evaluates expressions that follow near pointers, e.g. `vx=[[[5ffc+ae]+f77]+2]+62` (the Wild Science ball's x velocity). `watch` prints a line whenever the values change. Read-only. It works for EDISON, MALL and WINMAIN too (each one's automatic data segment).
  - `tools/reference/tracecmp.py ORIGINAL.txt PORT.log` compares a `memwatch.py watch` trace with the port's log, state by state, ignoring timing.
  - `edison --hidden` runs with no window shown and the sound muted, for test runs in the background.

#### Booting and cleaning up the original

The original's error boxes often have no owner, so winevdm attaches them to whatever window is in front, usually your terminal, and Windows disables that window until the box closes. If the game is killed or crashes while a box is open, the window (often every Windows Terminal window, since they share a process) stays disabled.

```powershell
pwsh tools/reference/otvdm.ps1 start                # boot EDISON.EXE (or: start WMAIN.EXE)
pwsh tools/reference/otvdm.ps1 dialogs              # check for open error boxes
pwsh tools/reference/otvdm.ps1 stop                 # always end a session with this
pwsh tools/reference/otvdm.ps1 unlock               # if the game already died
```

- `start` runs the game in a window: it hides the black, screen-sized backdrop that the game's library opens behind its 640x400 window, and keeps the game window on top so other windows can't cover it in shots. Set `OTVDM_FULLSCREEN=1` to keep the backdrop.
- To get to the Wild Science Arcade's menu table quickly, `python tools/reference/wmain_skip.py` writes `WMAINSKP.EXE` into the run folder. It's a copy with the intro flag cleared, so `start "WMAINSKP.EXE -A"` skips the title, the story, the lab and the professor and is at room 1 in about 8 seconds. In the port, `edison --game science --room 1` does the same.
- If `WMAIN.EXE` shows only a black window and uses a whole CPU core (seen right after a reboot), `stop` it, run `EDISON.EXE` once, and try again.
- Always end with `stop`, never by killing `otvdmw` or closing its window. `stop` closes dialogs, asks the game to quit, kills it only as a last resort, and then re-enables disabled windows.
- **Symptom:** a window that chimes when clicked and ignores all input is disabled, not frozen. Run `unlock` from any working shell (such as a new terminal window from the Start menu, or Claude Code's shell). It re-enables those windows; it skips conpty's `PseudoConsoleWindow` and UWP frames, which are disabled by design.

## Status

- [x] Unpack PKWARE DCL archives (`*.D01`, `GRAFX.DAT`)
- [x] Identify fonts, palettes, text resources
- [ ] Document animation / layout formats (groups 03, 50, 60, .VID, .SRF)
- [x] Decode FM music sequencer command set (docs/SEQUENCER.md)
- [x] Reference OPL log harness (run original driver under emulation)
- [x] Native C++ sequencer: all 898 sounds match the original driver write-for-write
- [x] Software OPL + audio output: `fmplay` (Nuked-OPL3, SDL3)
- [x] Readable refactor of the sequencer; differential tests cover all 57 opcodes (1,685 cases match)
- [ ] Decode the game's real Rock and Bach tempo (fmplay uses 128 for now)
- [ ] Document `.SRF` / `.HS` formats
- [ ] Decompile game logic (Ghidra, 16-bit NE)
- [ ] Engine skeleton (SDL) + software OPL for FM music

## Licences

Third-party code fetched at build time: SDL3 (zlib licence) and Nuked-OPL3
(LGPL-2.1; distributing binaries requires allowing users to relink against a
modified Nuked-OPL3).
