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

See `docs/ROCKBACH.md` for the map. Ported so far (`edison --game rockbach`):
the intro, the logo with its band and song, the widgets (segment 34), the
whole hallway, the jukebox and the Drum Clinic.

- [x] The FM driver's other entry points and the song player (segment 20).
- [x] The logo intro's band (`f05_04d8`).
- [x] The hallway: Edison's greeting, the player's name and looks (`user.yyy`, `ed.yyy`), the animated sign, the credits and the quit question.
- [ ] Compare the hallway with the original: a returning player's look only shows once a part is changed, the Yes/No buttons take the hair's colour, and the look is lost after the credits. That is how the code reads, but it should be checked.
- [x] The widgets' sliders (flags 08/10, `f34_04dc`).
- [x] The jukebox (`f03_22e8`).
- [x] The Drum Clinic (`f06_1e9e`). (The original's pointer snapping over the grid isn't ported.)
- [ ] The other activities: Music Library (`f30_1780`), Harmony Hall (`f07_1908`), Instrument Room (`f08_0f2e`), Sound FX (`f29_17e2`) and the Studio (`f35_018a`: bands, songs, videos).
- [ ] Decode its real tempo (`fmplay` uses 128 for now).

## Wild Science Arcade

- [ ] Wild Science Arcade (`WMAIN.EXE`).

## Formats and tooling

- [ ] Document the animation and layout formats (groups 03, 50, 60, `.VID`, `.SRF`).
- [ ] Document the `.SRF` and `.HS` formats.
