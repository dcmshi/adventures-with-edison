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
  - [x] The panel's locked controls: OUT OF ORDER signs and Edison putting them up.
  - [x] Segment 24's dialog boxes and room 1's method 8: EXIT's question, the warp codes of levels 4 and 5 (and their points in the next table), checked against the original pixel for pixel. `SCI_HOLE=n` gets there without a shot.
  - [x] Each room's method 8 (hint holes, questions, redirects, bonuses), the rooms' greetings and doors on arrival, the room's end (`f38_020f`: the bonus by shots, the "Bonus Points" box, bonus balls); checked against the original through level 1's bank shot; spit modes 0 (state for state, through its bounce and rest) and 1 (room 2's hint holes, both walls).
  - [ ] The rooms' own code that their holes read: room 18's tick (`+FC4`), room 34's builder (`+FA4`, `+FA6`, coming in through a door), room 55's objects (`+FB4`, `+FB6`); room 57's lit shoot button.
  - [x] Kind 3 targets (the lips); RETRY (type 7, d 3) and clicks on the room's objects; bodies (the step for any body, `f08_0d3e` with masses, the 5 sub-steps at full power, segment 11's small vectors doubled); magnets: the field (segment 26), types 4, 5 and 15 (rooms 3, 13 and 33 traced state for state); Edison reaching gravity's sign from the left; the library's solid polygons (room 2's pit edge).
  - [x] Type 7's switches on the power (`+F94`: d 0 lever, 1 bullseye, 2 blinking) with type 6 powered; the ball stopped every tick a hole has it (rooms 61, 62 and 29 traced state for state, room 25's blinker).
  - [x] Type 12, the electromagnet (catching Iron, breaking the rest; Rubber's zapped frames): room 90's head and room 25's catch and breaks traced against the original.
  - [x] Type 13, the fans (breaking Ice, Rubber, Glass and Magic balls in their quarter; Ice melting): rooms 98 and 25 traced against the original.
  - [x] Type 10 kind 6, the suckhole: a fuse laid behind the ball once its points are scored, a spark burning along it 130 ticks on, the ball broken when it's reached (two entries in the room's list and two targets counted: room 25's 20); the point targets' sizes by kind (`DS:2F4`: kind 2 bigger, kind 7 smaller). Room 22 traced against the original, the fuse and its scorched trail pixel for pixel.
  - [x] Type 9, the pulling hole: rooms 6 (swallowed, the ball lost) and 11 (pulled round it) traced against the original state for state; rooms 6, 11 and 17 drawn as the original's.
  - [x] Type 14, the hot field (a lava puddle growing in spots, burning the ball): room 5's growth and puddle, room 32's burn traced against the original.
  - [ ] Room 96's pit at z -388 and its greeting's second click aiming, in the original.
  - [ ] Room 17's Iron ball at rest against the left wall (by the magnets, before any shot): the original's x velocity goes -28, 5, -28, -27, the port's -26, -32; a shot from there drifts apart (`build/scratch/t9/p17`, aim 292, 68).
  - [ ] The rooms' own: room 2's circuit and gate (`f41_076c`, `f41_0b78`, `f41_0c21`); room 13's glass wall (box looks, `+3C`: `f43_0838`); room 33's water; the pits in the painter's order (the standing boxes are in it: rooms 6, 29, 58, 70 checked; a pit's cut, `f12_220d`, reaches the redraw area's edges, so it needs the original's redraw of the changed rectangles only, `f29_0380`: room 3's magnet in a pit, its second N block gone in the original). The original redraws only a changed object's rectangle (`f08_07a7`; each queued old and new rectangle, overlapping ones merged): sprites bigger than it (the electromagnet's burst, breaking balls) leave pieces there that the port, redrawing the view, doesn't.
  - [ ] The high scores (502, segment 40; and the game over: `f40_068b`), the credits (503, `f38_0eb9`).
  - [ ] Doors within a room (holes 0, 100-500) (and mode 2's shadow: after a wrong warp code the original shows none under the ball put back, the port one; the shadow object's state through the spit, `f07_15ba`, which also runs while the ball is hidden); the keys (S and two digits: a room; P; s).
  - Test tools: `tools/testing/` (see its README: `check.py` (all of them, before a commit), `trace.sh`, `retrace.py`, `regress.py`, `aimsearch.py`, `origrun.py`, `wm.py` (reading WMAIN.EXE), `cmp.py`, `bestframe.py`, `roompics.py`, the `SCI_*` switches). They and `tools/reference/` need `EDISON_RUN` (the game's folder) and `OTVDM` (winevdm's `otvdmw.exe`, unless on the PATH); `tools/ghidra/decompile.sh` needs `GHIDRA`. The test scripts run the port with `--virtual-clock`, so load doesn't change their results; by hand, under load (ComfyUI on the GPU) timed captures skip ticks: use `--virtual-clock` or `SCI_TICKSHOTS`.
- [ ] The arcade's menu table, the levels, high scores (`WSCIENCE.HS`), the lab, the credits.
- [ ] Compare with the original under winevdm as each part lands (`memwatch.py` reads the original's state: use it on Mystery and Rock and Bach too, for oddities in their ports).

## Formats and tooling

- [ ] Document the animation and layout formats (groups 03, 50, 60, `.VID`, `.SRF`).
- [ ] Document the `.SRF` and `.HS` formats.
