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
- [x] The FM music: `SADLIB.DLL` is the ADLIB family's driver (an older build, its variables 0x4F8 further on), played by the native driver; all 68 sounds match the original's register writes. The game's calls (`f32_135d`: the arcade's music one of three songs at random) where the original makes them.
- [x] The story's `s` key (`f36_004e`: the sounds over, the WAV and the music off), checked after each page, sound and wait as the original's (scan code 1Fh in `[9563]`).
- [x] The laboratory, room 501 (`f19_0a59`): Edison walks in, the name, "Do you wanna change the way I look?", the Character Enhancer, "Cool!"; `wscience.edi` and the players in `wscience.hs`. The name prompt matches the original pixel for pixel.
- [x] The professor's first lesson (room 505, `f15_0fee`): the classroom (`2003`), his eight lines in bubbles (laid out and wrapped as `f15_31fa` and segment 23 do), the narration, MORE. The bubbles match the original pixel for pixel.
- [x] All six lessons (rooms 505-510): their pictures, scripts (anchors, tails, widths, texts, narration) and the rooms they lead to. All compared with the original (lessons 6-10 through room 61's holes): the bubbles match pixel for pixel.
- [x] The lessons' animations (the professor and Edison, cycling sprites) and the colour cycle (70-7F every 8 ticks; the original only cycles on a 256-colour display, so not under winevdm). With the animations, lesson 5's frames match the original within 8 pixels (a bubble's right edge).
- [x] The professor's easter egg (`g15_0cb7`: pressed, lessons 5-8's professor turns to `13BE` till the release), checked against the original; the table's picture brings the room's colours to the display (`f14_092c`), so a greeting after a lesson is in the room's colours.
- [x] The bubbles' stretched edges as the original's scaler (`f73_0324`: 16.16 steps): every bubble of lessons 6-10 pixel for pixel.
- [x] The arcade's menu (room 1) against the original: every hole from a shot (the play room 31, Level 1's 21, the lab through to room 1 again; the others before); at rest only the columns' random balls differ, as the original's own runs do.
- [x] `--room 501` (the lab) and `--room 505`-`510` (a lesson) to start there when testing.
- [x] The table: room 1, its controls, the ball's physics (traced state for state), the shadow in the air, the painter's order, holes and going to their rooms, breaking, losing the ball, PUSH's drop, the glass's marks (see `docs/SCIENCE.md`); checked against the original pixel for pixel.
- [x] The arcade, next:
  - [x] Each room's pictures and builder settings; point targets (type 10) and the score.
  - [x] The panel's locked controls: OUT OF ORDER signs and Edison putting them up.
  - [x] Segment 24's dialog boxes and room 1's method 8: EXIT's question, the warp codes of levels 4 and 5 (and their points in the next table), checked against the original pixel for pixel. `SCI_HOLE=n` gets there without a shot.
  - [x] Each room's method 8 (hint holes, questions, redirects, bonuses), the rooms' greetings and doors on arrival, the room's end (`f38_020f`: the bonus by shots, the "Bonus Points" box, bonus balls); checked against the original through level 1's bank shot; spit modes 0 (state for state, through its bounce and rest) and 1 (room 2's hint holes, both walls).
  - [x] The rooms' own code that their holes read: room 34's `+FA4`, `+FA6` (coming in through a door), room 18's `+FC4`, room 55's objects, room 57's lit sign (its colour cycle).
  - [x] Kind 3 targets (the lips); RETRY (type 7, d 3) and clicks on the room's objects; bodies (the step for any body, `f08_0d3e` with masses, the 5 sub-steps at full power, segment 11's small vectors doubled); magnets: the field (segment 26), types 4, 5 and 15 (rooms 3, 13 and 33 traced state for state); Edison reaching gravity's sign from the left; the library's solid polygons (room 2's pit edge).
  - [x] Type 7's switches on the power (`+F94`: d 0 lever, 1 bullseye, 2 blinking) with type 6 powered; the ball stopped every tick a hole has it (rooms 61, 62 and 29 traced state for state, room 25's blinker).
  - [x] Type 12, the electromagnet (catching Iron, breaking the rest; Rubber's zapped frames): room 90's head and room 25's catch and breaks traced against the original.
  - [x] Type 13, the fans (breaking Ice, Rubber, Glass and Magic balls in their quarter; Ice melting): rooms 98 and 25 traced against the original.
  - [x] Type 10 kind 6, the suckhole: a fuse laid behind the ball once its points are scored, a spark burning along it 130 ticks on, the ball broken when it's reached (two entries in the room's list and two targets counted: room 25's 20); the point targets' sizes by kind (`DS:2F4`: kind 2 bigger, kind 7 smaller). Room 22 traced against the original, the fuse and its scorched trail pixel for pixel.
  - [x] Type 9, the pulling hole: rooms 6 (swallowed, the ball lost) and 11 (pulled round it) traced against the original state for state; rooms 6, 11 and 17 drawn as the original's.
  - [x] Type 14, the hot field (a lava puddle growing in spots, burning the ball): room 5's growth and puddle, room 32's burn traced against the original.
  - [x] Room 96's greeting's second click aims (the press's event sees the button down, `[6EC4]`); the ball into its pit (z -390) traced, 329 states.
  - [x] Room 17's Iron ball at rest against the left wall matches the original (86 states); a shot from there (aim 292, 68) too, fired at the same moment of its loose magnet's creep (278 states with the remainders).
  - [x] The rooms' face hooks (`+24`): the ball broken on a box (14 rooms, room 33's water), room 91's goal, the bins of 60, 64, 69, room 96's pit; room 2's circuit and gate (its tick, `[27B4]`).
  - [x] Box looks (`+3C`: a face's own picture, or none) in rooms 5, 10, 13, 18, 37, 40, 42, 45, 52, 53, 55, 92; the builders' own objects of rooms 10 (its gate of blocks, loose balls, hole to 12), 18 (a magnet), 55 (two magnetic balls); room 10's face hook. Rooms 45, 92, 13 checked at rest.
  - [x] Room 55's face hook (`f51_1777`, traced: 705 states of seven bodies); room 12's magnet (`f43_0386`) and tick (`f43_043d`, pixel for pixel); room 54's bullseye and magnets (`f51_09ce`) and tick (`f51_0dc2`, its way down traced).
  - [x] Room 54: its way up (a second hit) traced; at the bottom N in front of S: the order table kept between draws (`+5FD`: `f27_1bd9` compares again only pairs with one marked moved, `+7E`; the fixed magnets don't mark themselves).
  - [x] The ticks of rooms 9 (`f42_0805`: only a colour cycle), 18 (`f44_11a7`: its magnet's slot, gate and hole to 60, traced), 34 (`f47_0cbf`: its sign, matched); room 18's RETRY from its hole to 100; the builders' other calls (room 12's are a debugging check; room 35's arrival from 43 out of its hole to 1000, not traced).
  - [x] The colour cycles (`f32_0e7f`, `f32_0f77`, `f32_1113`: the magnets', holes', hot fields' and rooms'), on by default as on a 256-colour display; `SCI_TRUECOLOR=1` (the tests) turns them off, as under winevdm, where they can't be compared.
  - [x] Traced against the original: rooms 60 and 64's bins (levers), room 91's goal (and its bonus box), the breaking boxes of rooms 36, 38, 49, 66; rooms 18 and 55 at rest are in their traces' start. Room 10's gate can't open in play (the ball locked to Stone, its loose balls Rubber).
  - [x] The pits in the painter's order (a pit's cut reaching the redraw area's edges) and the redraw of only the changed rectangles (`f27_16ae`: an object's old and new rectangles queued; `f29_0313`, `f29_0380`: overlapping ones merged; `f35_04ca`: only what meets the area drawn). Room 3's pit checked against the original (its second N block and a wall hole cut away).
  - [x] Compare against the original the pieces that sprites bigger than their rectangle leave on the display: room 25's electromagnet (a catch, two zaps) the same at rest; the fan's wind now redrawn by its own area (`f02_0870`), as the original's, so it leaves no pieces.
  - [x] The high scores (502, `f40_0000`: the original's load, swap and quicksort, so its order of ties) and the game over (`f40_068b`, then a new game), the credits (503, `f38_0eb9`); the screens pixel for pixel. The game over (room 32, all eight balls lost) and the game won (`[26CC]`: lesson 10's last step, `f31_06c0`, from room 65's hole to 510) checked against the original: the records and the screen the same.
  - [x] The shadow under a ball put back (after a wrong warp code, or a slide: PUSH, `r`): drawn without it till it moves, as the original (`f27_287d` shows the shadow object, `f07_03fb`; a slide draws with `[14E0]` set; nothing redraws the ball after). Both checked against the original.
  - [x] The panel's white: winevdm's, not the game's. Colour 255 of picture 2002 (the machine, on screen 3) is (255, 247, 247); `f14_092c` copies only colours 1-254 into the other screens, so screen 2's 255 stays the library's white. On a true-colour desktop WinG blits each screen with its own colour table, so the panel (from screen 3) shows 247 and the view (from screen 2: the logo, EXIT) white; on a 256-colour display colour 255 is Windows' static white everywhere, as the port draws it. Left as it is (the port has one display palette).
  - [x] Doors within a room (the file's `e`: `f28_0391`, out of the hole before it); room 13's traced.
  - [x] The keys (`f31_1d70`: p pause, q quit, m the easter egg to room 24, s the sounds, S and two digits a room); the boxes checked against the original. The room's (`+1C`, `f27_31fb`: `r` takes the ball back for 300 points) too; the panel only gets keys with no room.
  - [x] Mode 2's shadow: the shadow's tick (`f07_15ba`) as the original's, running while the ball's hidden, hidden while it breaks; the ball put back (`f27_287d`) leaves the shadow's last centre, so its tick hides it.
  - Test tools: `tools/testing/` (see its README: `check.py` (all of them, before a commit), `trace.sh`, `retrace.py`, `regress.py`, `aimsearch.py`, `origrun.py`, `wm.py` (reading WMAIN.EXE), `cmp.py`, `bestframe.py`, `roompics.py`, the `SCI_*` switches). They and `tools/reference/` need `EDISON_RUN` (the game's folder) and `OTVDM` (winevdm's `otvdmw.exe`, unless on the PATH); `tools/ghidra/decompile.sh` needs `GHIDRA`. The test scripts run the port with `--virtual-clock`, so load doesn't change their results; by hand, under load (ComfyUI on the GPU) timed captures skip ticks: use `--virtual-clock` or `SCI_TICKSHOTS`.
- [x] The arcade's menu table, the levels, high scores (`WSCIENCE.HS`), the lab, the credits.
- [x] A scenario tool (`tools/testing/scenario.py`, `scenarios.py`): a timeline of presses, drags, moves and typing from a table room, played in the port and the original, each of the original's shots against the port's nearest frame, the columns masked: room 1's holes, lessons 6-10, the game over and won. In `check.py` (against the original's shots of its last `--orig` run; local only, as `regress.py`).
- [ ] Compare with the original under winevdm as each part lands (`memwatch.py` reads the original's state: use it on Mystery and Rock and Bach too, for oddities in their ports).

## Formats and tooling

- [x] Document the formats in `FORMATS.md`: SHELL's scripts (group 03) and animations (group 50), Mystery's unused group 60, `.SRF`, `.HS`, `.VID`.
