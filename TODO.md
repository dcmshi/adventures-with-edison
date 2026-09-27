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

See `docs/ROCKBACH.md` for the map. Ported so far (`edison --game rockbach`,
`--level N` goes straight to hallway result N): the intro and logo, the
widgets (segment 34), the file dialogs (segments 22-23), the whole hallway
and seven of the eight activities. Each was checked with automated captures
against the disassembly, not yet against the original.

- [x] The FM driver's other entry points and the song player (segment 20); the logo intro's band.
- [x] The hallway: Edison's greeting, the player's name and looks (`user.yyy`, `ed.yyy`), the sign, the credits, the quit question.
- [x] The widgets (segment 34), sliders included; the file dialogs (segments 22, 23).
- [x] The jukebox (2), the Drum Clinic (3), the Music Library (4), Harmony Hall (6), the Instrument Room (7), Sound FX (8).
- [ ] **Next: the Studio** (9, `f35_018a`, `ADLIB2`): `f04_112e`, the band maker `f14_10f4`, the song maker `f15_1f84`, the video makers `f16_217c`, `f17_20f2`, `f26_1edc` and the player `f18_22a4`. It uses the file dialogs (kind 0 for videos).
- [ ] Compare with the original:
  - the hallway: a returning player's look only shows once a part is changed, the Yes/No buttons take the hair's colour, the look is lost after the credits (how the code reads);
  - the Music Library's end-of-piece check waits for the driver to start the sounds (a port adjustment for a timing race);
  - Sound FX: the handles vanish after LOAD until an effect is changed (how the code reads); the scroll bar's arrows repeat every 0.1 s (the original repeats every poll);
  - the Drum Clinic's pointer snapping over the grid isn't ported.
- [ ] Decode its real tempo (`fmplay` uses 128 for now).

## Wild Science Arcade

- [ ] Wild Science Arcade (`WMAIN.EXE`).

## Formats and tooling

- [ ] Document the animation and layout formats (groups 03, 50, 60, `.VID`, `.SRF`).
- [ ] Document the `.SRF` and `.HS` formats.
