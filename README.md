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

### Music player

`fmplay` plays the game's FM music and sound effects straight from its DLLs,
at the game's timer rate (13 ms per tick):

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
