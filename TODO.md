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

See `docs/ROCKBACH.md` for the map. Ported (`edison --game rockbach`,
`--level N` goes straight to hallway result N): the intro and logo, the
widgets (segment 34), the file dialogs (segments 22-23), the whole hallway
and all eight activities. Each was checked with automated captures
against the disassembly, and the parts below against the original under
winevdm (`tools/reference/otvdm.ps1`, `screendiff.py`).

- [x] The FM driver's other entry points and the song player (segment 20); the logo intro's band.
- [x] The hallway: Edison's greeting, the player's name and looks (`user.yyy`, `ed.yyy`), the sign, the credits, the quit question.
- [x] The widgets (segment 34), sliders included; the file dialogs (segments 22, 23).
- [x] The jukebox (2), the Drum Clinic (3), the Music Library (4), Harmony Hall (6), the Instrument Room (7), Sound FX (8).
- [x] The Studio's front room (`f04_112e`), band maker (`f14_10f4`) and song maker (`f15_1f84`); videos load, save and delete.
- [x] The Studio's player (`f18_22a4`, with the credits `f04_08cc`).
- [x] The Studio's video makers (`f26_1edc`, `f16_217c`, `f17_20f2`, through `f35_005a`) and their previews.
- [x] Checked against the original (pixel for pixel, but for random choices and the original's stale window pixels, see `docs/ROCKBACH.md`): the hallway's look machine, a returning player and the credits; the Studio's front room, band maker, song maker, video makers, playback (scene for scene, to the end) and file dialogs; Sound FX's LOAD, list and effects; the jukebox's and Harmony Hall's screens.
- [ ] Still to compare with the original:
  - the Music Library's end-of-piece check waits for the driver to start the sounds (a port adjustment for a timing race); winevdm has no AdLib, so there the original thinks every piece is over at once;
  - Sound FX's scroll bar arrows repeat every 0.1 s (the original repeats every poll);
  - the video makers' "playing" light flips every 0.5 s (the original flips it every 13000 polls);
  - the Drum Clinic: the kit buttons' colour 255 shows as the backdrop's grey (D7) in the original under winevdm, white in the port (`f19_0082` sets 255 to white but never sends 0 or 255 to the display; how winevdm's WinG ends up with the grey is unknown); the rest of its screen matches;
  - the Instrument Room's waveform: the original reads the instruments' WAVs from the CD, which it can't find under winevdm (its waveform comes out flat at the bottom); the rest of its screen matches;
  - the activities' other actions (only their first screens were compared).
- [ ] Decode its real tempo (`fmplay` uses 128 for now).

## Wild Science Arcade

See `docs/SCIENCE.md`. A different codebase from the other two games (C++
scene objects and an event queue, floating point), so it goes in stages:

- [x] First look: the game's flow under winevdm, WinMain, the game loop, the files.
- [x] Map the library layers (segment 14 over 63-83) and the framework: the game and player objects, the events, the room dispatcher (110 rooms and the special ones).
- [ ] Map the room base class (segment 27: the `.SRF` rooms, the table's drawing) and the object classes (segments 2-8, 15, 28-30), the physics (11, 24-26).
- [x] `edison --game science` (and the main menu's Wild Science): the archive, the title and the story (`f38_0718`) with narration; both pixel-identical to the original.
- [ ] The FM music: `SADLIB.DLL` is a different driver from the ADLIB family (none of their code patterns), so it needs its own emulation; till then the game has no FM music.
- [x] The laboratory, room 501 (`f19_0a59`): Edison walks in, the name, "Do you wanna change the way I look?", the Character Enhancer, "Cool!"; `wscience.edi` and the players in `wscience.hs`. The name prompt matches the original pixel for pixel.
- [x] The professor's first lesson (room 505, `f15_0fee`): the classroom (`2003`), his eight lines in bubbles (laid out and wrapped as `f15_31fa` and segment 23 do), the narration, MORE. The bubbles match the original pixel for pixel.
- [x] All six lessons (rooms 505-510): their pictures, scripts (anchors, tails, widths, texts, narration) and the rooms they lead to. Only lesson 5 has been compared with the original so far.
- [x] The lessons' animations (the professor and Edison, cycling sprites) and the colour cycle (70-7F every 8 ticks; the original only cycles on a 256-colour display, so not under winevdm). With the animations, lesson 5's frames match the original within 8 pixels (a bubble's right edge).
- [ ] The lessons: the professor's click easter egg (`g15_0cb7`); what lesson 10's end starts (`f31_001a`, then `31:1728`); lessons 6-10 against the original; then the arcade's menu (room 1).
- [x] `--room 501` (the lab) and `--room 505`-`510` (a lesson) to start there when testing.
- [x] The table: room 1, its controls, the ball's physics (traced state for state), the shadow in the air, the painter's order, holes and going to their rooms, breaking, losing the ball, PUSH's drop, the glass's marks (see `docs/SCIENCE.md`); checked against the original pixel for pixel.
- [ ] The arcade, next:
  - [x] Each room's pictures and builder settings; point targets (type 10) and the score.
  - The panel's locked controls: OUT OF ORDER signs and Edison running in to put them up (`f30_1569`, `f30_0d7c`: segment 30's actor). Each room's own methods (method 8: holes that need every target hit, `f03_0014`; dialogs), the completion bonus (`+F7B`, `+F87`), kind 3 targets, suckholes (type 10 kind 6), the other object types, the table's standing boxes in the painter's order (type 1 drawables).
  - Room 1's dialogs (EXIT's question, the passwords of levels 4 and 5: segment 24's), the high scores (502, segment 40; and the game over: `f40_068b`), the credits (503, `f38_0eb9`).
  - Doors within a room (holes 0, 100-500) and spit modes 0 and 1; type 3's segment 5 part; other balls (`f08_0d3e`), the push (`+4C`); the keys (S and two digits: a room; P; s).
  - Test tools (gitignored, `build/scratch/`): `regress.sh` (rest, sliders, aim and ball types, 8 aims, the shot, against the original's shots there), the otvdm scripts `room1_*.txt`, `cmp.py` (tolerant diff of view and panel by capture time), `trace.sh NAME "ms x y hold;..." [ms]` (the same presses in both, the original traced by `memwatch.py`, compared by `tracecmp.py`), `bestframe.py PORTDIR SHOT.png` (the port's capture nearest an original's screenshot: captures every 10 ms, as 20 can skip a tick).
- [ ] The arcade's menu table, the levels, high scores (`WSCIENCE.HS`), the lab, the credits.
- [ ] Compare with the original under winevdm as each part lands (`memwatch.py` reads the original's state: use it on Mystery and Rock and Bach too, for oddities in their ports).

## Formats and tooling

- [ ] Document the animation and layout formats (groups 03, 50, 60, `.VID`, `.SRF`).
- [ ] Document the `.SRF` and `.HS` formats.
