# TODO

Open items, roughly in the order we plan to do them. See `docs/MYSTERY.md` for
what is already ported and the original function names.

## Mystery at the Museums

Everything in the original is ported (setup, the map, all 16 puzzles, the end
of a game, high scores, saved games, custom levels and the idle animations).
What's left is checking it against the original:

- [ ] Compare each puzzle with the original at an easy and a hard level (layout, timing, scoring). So far they have only been checked against the disassembly.
- [ ] Compare the end of a game (dance, quiz, bonus maze, endings, high scores), especially the bonus maze's speed, which the original doesn't pace (the port uses 40 ms a step).
- [ ] Compare the setup prompts, saved games and the custom level editor.

## Rock and Bach

See `docs/ROCKBACH.md` for the map. Started: the intro screens and the
hallway's hot spots run (`edison --game rockbach`).

- [ ] The FM driver's other entry points (`GETADDR`, `GETVAR`, `SSTATUS`, `DIRECTDRUMOUT`, `INSTALL_PATCH`, `PLAYINS`) and the song player (segment 20), which writes riffs into the driver's sound table.
- [ ] The logo intro's band (`f05_04d8`).
- [ ] The hallway: Edison's greeting, the player's name and looks (`user.yyy`, `ed.yyy`), the animated sign, the credits and the quit question.
- [ ] The activities: the jukebox (`f03_22e8`), Drum Clinic (`f06_1e9e`), Music Library (`f30_1780`), Harmony Hall (`f07_1908`), Instrument Room (`f08_0f2e`), Sound FX (`f29_17e2`) and the Studio (`f35_018a`: bands, songs, videos).
- [ ] Decode its real tempo (`fmplay` uses 128 for now).

## Wild Science Arcade

- [ ] Wild Science Arcade (`WMAIN.EXE`).

## Formats and tooling

- [ ] Document the animation and layout formats (groups 03, 50, 60, `.VID`, `.SRF`).
- [ ] Document the `.SRF` and `.HS` formats.
