# Task 001: Decode the remaining game data formats

**Branch:** `kimi/data-formats` (branch from `master`, open a PR or just push the branch)
**Owner:** Kimi. Reviewer: Claude.
**Working in parallel with:** Claude, who is reverse-engineering the game executables (`docs/GAME.md`, `tools/disasm.py`, `tools/ne.py`). Please don't edit those files. If you need a change there, note it under "Requests" in your write-up.

## Background

This repo is a clean-room native reimplementation of *Adventures with Edison* (Corel, 1995), a Win16 game. Read these first:

- `README.md`: project overview and build.
- `docs/FORMATS.md`: everything known so far about the data files.
- `docs/SEQUENCER.md`: an example of the level of detail we want (the FM music driver, fully decoded and verified).

The archives (`*.D01`, `GRAFX.DAT`) are already decoded. `tools/extract.py` unpacks them to `extracted/<archive>/<id>.bmp|wav|bin`, where `<id>` is 4 hex digits: the high byte is the resource group and the low byte is the index. Bitmaps, fonts (group `01`), palettes (`02`, `30`) and text (`31`, `3f`, `70`-`76`) are understood. **What's left is the non-bitmap data that drives screens, hotspots and animation.**

### Setup

```sh
python -m venv .venv && .venv/Scripts/pip install -r requirements.txt   # (.venv/bin on Linux/macOS)
python tools/extract.py        # needs original/ (the user has it; ask them to copy it to your checkout)
```

## Hard rule: no game data in git

`original/`, `extracted/` and `*.iso` are git-ignored. Never commit game files, dumps of them, or large excerpts (a few hex bytes quoted in docs as an example is fine). Tools must read from `original/` or `extracted/` at run time.

## Scope: formats to decode

In rough priority order. For each one, work out the layout and what every field means, as far as the data allows.

1. **`S*.SRF`** (`original/cd/DSK3/S0.SRF` … `S110.SRF`, 111 files). ASCII integers. Probably one file per screen, holding rectangles (x y w h) and parameters. `S0.SRF` starts:
   `0 0 809 789  0 0 0 0  0  0 199 809 388  0 199 809 388  300  ...`
   It looks like groups of `rect rect value`, i.e. source rect, destination rect and a number (a delay? a sound?). Find the grouping rule, what the numbers mean, and which game and screen each file belongs to.
2. ~~Archive group `03`~~: **taken by Claude.** These are compiled menu scripts (the interpreter is in EDISON.EXE and is part of the game logic).
3. **Archive group `50`** (SHELL, 20 entries). Starts `count, 320, 200`. Almost certainly animations: EDISON.EXE loads them in function `g08_00d4` (error string `Out of memory reading anim.`). Scripts start them with `STARTANIM ANIMNAME/K/A,XPOS/N,YPOS/N,WAIT/S,SOUND/K`.
4. **Archive group `40`** (SHELL 14, MYSTERY 1) and **`60`** (RB 1, MYSTERY 2, GRAFX 45). Not yet looked at.
5. **`*.VID`** (`FRED.VID`, `SUGAN.VID`, 618 bytes each), **`*.PAT`** (`STANDARD.PAT` 3584 bytes, `NONAME.PAT` 512 bytes, all zero), **`*.HI`** (Rock and Bach info text with `! n n` headers) and **`*.HS`** (high scores). These are Rock and Bach and high-score files. The `.VID` files are 16-bit little-endian words; they may be saved band or "video" arrangements.
6. **Bitmap conventions.** Which palette index is transparent in sprites? Are there sprite sheets (bitmaps holding several frames), and how are frames laid out? Which palette goes with which bitmap?

## Method

- **Data first.** Compare many files: sizes, value ranges, correlations with bitmap sizes (full screens are 640x400; some SRF values look like coordinates in a larger space, such as 809).
- **`tools/nedis.py`** disassembles a whole game EXE with imports named (`USER.CreateWindow`), cross-segment calls resolved and string references annotated. Run it as `python tools/nedis.py original/cd/DSK3/EDISON.EXE`. It writes `extracted/disasm/edison.asm` plus `edison.funcs.txt`, a one-line-per-function summary, and grepping the summary for an error string finds the parser quickly. Resource loads go through `f18_1da0` (size of an archive entry by id) and `f18_1b44` (load an entry) in EDISON.
- **The executables help.** The game code that reads these files is in `EDISON.EXE` (menu), `WINMAIN.EXE` (Rock and Bach), `WMAIN.EXE` (Wild Science Arcade) and `MALL.EXE` (Mystery Mall). `python tools/disasm.py <file>` produces disassembly under `extracted/disasm/`. Searching the executables for the file-name strings (for example `.SRF`, `%d.SRF` or `.VID`) and following the references is often the fastest route to a parser. The EXEs have no debug symbols.
- **Prove it.** A decoded format should round-trip or render. For example, draw SRF rectangles over the matching screen bitmap, or step through an animation, and confirm by eye.

## Deliverables

1. **`docs/FORMATS.md`**: a section per format with a byte or field table, the meaning of each field, and your confidence ("confirmed by parser at EDISON.EXE 3:1234" vs "inferred from data"). Replace the `?` rows in the existing tables.
2. **`tools/formats/<name>.py`**: a small dump or inspect script per format (standard library plus Pillow only). Each should print a readable decoding of one file, or of all of them, and where useful render a PNG overlay to `extracted/formats/` (git-ignored).
3. **Optional, once a format is confirmed:** a C++ loader in `engine/src/formats/<name>.{h,cpp}`, added to `edison_core` in `engine/CMakeLists.txt`.
   - Match the style of `ne_file.cpp`: C++17, `namespace edison`, return `bool` with a `std::string* error`, no exceptions.
   - Add a small test executable like `engine/tests/seqtest.cpp` that loads every file of that type from `original/` and checks the invariants.
   - It must build clean with `cmake -G Ninja -B build && cmake --build build` (warnings are on).
4. **`docs/tasks/001-data-formats-report.md`**: what you decoded, what's still unknown, open questions, and any requests for Claude (for example "please find the function that loads group 50 at runtime").

Small, frequent commits are preferred. Each commit message should say what was learned.

## Out of scope

- The FM music driver and anything under `engine/src/audio/` (already done).
- Game logic and state machines (Claude is on this).
- Sound effects `.WAV` files (already standard format).
