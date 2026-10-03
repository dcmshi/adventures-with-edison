# Wild Science Arcade (WMAIN.EXE)

Reverse-engineering notes for the third game. Addresses are
`segment:offset` as `tools/nedis.py` prints them (`f38_0718` is the
function at 38:0718); Ghidra's `wmain.c` names segment n
`FUN_(0x1000 + 8 * (n - 1))_offset`.

## The game

Started by the launcher as `wmain.exe -A`; on leaving it runs
`edison.exe -O` (back to the main menu). Seen under winevdm:

1. The title (Corel presents The Wild Science Arcade).
2. The story: pictures with green captions and narration, "In the back
   corridors of the world's leading physics institute, Professor Blueman
   has been laboring over his latest project." (the institute), "The
   professor has created an arcade-like machine..." and "You and your
   friend, Edison, have been selected..." (the lab door).
3. The laboratory: Edison asks the player's name ("Hi! I'm Edison. What's
   your name?"), then the Character Enhancer (as in Rock and Bach).
4. The professor at his blackboard ("POWER 4 = 40 N FORCE", a ball and a
   puck): "Welcome to the lab!" and more lines, a MORE button.
5. The arcade machine: a table seen in perspective whose holes are the
   menu (play room, LEVEL 1, LEVEL 4, LEVEL 5, High Score, Lab, Credits,
   EXIT). The ball is shot at them; under the table, GRAVITY, FRICTION,
   BALL TYPE (Rubber) and POWER; a shot counter and a score.

## Program

- Borland C++, large model: 91 code segments and 12 data segments
  (92-103; DGROUP is 103), floating point through `WIN87EM`. Not the Artech
  library of the other games, though the art is in the same archive
  format.
- `f01_0000` is the C startup; `f62_0020` WinMain (registers the class,
  `f62_01f4`, then a `PeekMessage` loop that calls `f62_062d` when idle);
  `APPWNDPROC` (62:03a7), `APPABOUT` (62:036b), `BLANKWNDPROC` (66:0eeb).
- `f32_0c1d` is the game loop, run from the idle call: it takes events off
  a queue (`f32_082c`, `f32_008d` frees one) and hands each to the current
  scene object through its vtable (at `+8C`): types 1 and 2 to method 0
  (with 0 or 1), 4 to method 1 (a tick), 5, 7, 8 and 9 to methods 4, 2,
  3 and 6 (with the event), 6 to an object's method 12, 3 leaves (posts
  `WM_CLOSE`). There is a table of game objects (`MAX_GAME_OBJECTS
  exceeded`).
- Segment 14 is a thin graphics layer over segments 63, 64, 67, 70, 72
  and 76; segment 36 is the sounds: `f36_00ad` plays `<CD>\science\<name>.wav`,
  else (or with no CD) `data\<name>.wav` next to the game. Without them
  the original waits for ever after its title.
- `f38_0718` is the story.

## Files

| File | Contents |
|---|---|
| `GRAFX.DAT` | The art and some sounds and data (the archive format of the other games, see `FORMATS.md`); extracted to `extracted/grafx/` |
| `\SCIENCE\*.WAV` | 153 sounds: narration, the professor's and Edison's lines |
| `S0.SRF`-`S110.SRF` | Rooms (playfields): ASCII rectangles in a world about 809 x 789 (four numbers twice and a depth, per piece), then named objects (`OBJ0 448 180`), then `END` |
| `WSCIENCE.HS` | The high scores |
| `wscience.edi` | The players (written by the game) |

## Running the original

`tools/reference/otvdm.ps1 start "WMAIN.EXE -A"` with the CD's `SCIENCE`
WAVs copied to `data\` in the run folder (winevdm has no CD drive).
