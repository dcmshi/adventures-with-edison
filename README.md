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

Requires CMake 3.20+ and a C++17 compiler (GCC, Clang or MSVC).

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Sequencer test

`seqtest` replays every sound through the native FM driver and compares the
OPL register stream with reference logs recorded from the original driver:

```sh
python -m venv .venv && .venv/Scripts/pip install -r requirements.txt
.venv/Scripts/python tools/oplref.py ADLIB.DLL ADLIB1.DLL ADLIB2.DLL ADLIB3.DLL ADLIB4.DLL CADLIB.DLL MADLIB.DLL
ctest --test-dir build --output-on-failure
```

## Status

- [x] Unpack PKWARE DCL archives (`*.D01`, `GRAFX.DAT`)
- [x] Identify fonts, palettes, text resources
- [ ] Document animation / layout formats (groups 03, 50, 60, .VID, .SRF)
- [x] Decode FM music sequencer command set (docs/SEQUENCER.md)
- [x] Reference OPL log harness (run original driver under emulation)
- [x] Native C++ sequencer: all 898 sounds match the original driver write-for-write
- [ ] Software OPL + audio output (Nuked-OPL3, SDL)
- [ ] Document `.SRF` / `.HS` formats
- [ ] Decompile game logic (Ghidra, 16-bit NE)
- [ ] Engine skeleton (SDL) + software OPL for FM music
