# TODO

Open items, roughly in the order we plan to do them. See `docs/MYSTERY.md` for
what is already ported and the original function names.

## Mystery at the Museums

### End of the game (segments 22-24)
- [ ] Replace the `endOfGame()` placeholder (`engine/src/mystery/game.cpp`) with the real end screens (`f23_140a`, `f23_10aa`).
- [ ] Run the final quiz, `questionPeriod(level, false)`, after `f23_10aa` (as `f09_2eb9` does).
- [ ] The Dig bonus game (`f22_0ec8`).
- [ ] High scores: the list, entering a name, and the `.HS` file (`f24_*`).

### Setup and returning players
- [ ] Load a returning player's `.INF` file (`engine/src/mystery/setup.cpp`).
- [ ] The saved-game, custom-level and last-level prompts.
- [ ] Show the high-score list from setup (it currently asks again).
- [ ] The return-visit setup (`f08_21b6`).
- [ ] The custom-level picture chooser (`g12_1ab0`).

### Puzzle polish
- [ ] Smitty's idle hints in the puzzles (`g12_1bec`, `b71e` / `g30_127c`).
- [ ] Smitty's idle animation (`f05_03de` / `f06_21c6`).
- [ ] Compare each puzzle with the original at an easy and a hard level (layout, timing, scoring). So far they have only been checked against the disassembly.
- [ ] Remove the "isn't ported yet" fallback in `puzzle()` (`engine/src/mystery/floor.cpp`) once nothing can reach it.

## Formats and tooling
- [ ] Document the animation and layout formats (groups 03, 50, 60, `.VID`, `.SRF`).
- [ ] Document the `.SRF` and `.HS` formats.
- [ ] Decode Rock and Bach's real tempo (`fmplay` uses 128 for now).

## Later games
- [ ] Rock and Bach (`WINMAIN.EXE`).
- [ ] Wild Science Arcade (`WMAIN.EXE`).
