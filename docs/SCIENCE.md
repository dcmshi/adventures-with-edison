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

## The story (`f38_0718`, ported: `science.cpp`)

`f36_00ad(n, 1)` plays narration WAV n (names 9 bytes apart at
`seg97:0519`), and the story waits while `[92BC]` (a WAV playing) is set.
Bit 1 of the held-keys bitmap (`DS:9560`, Escape) skips the rest.

1. WAV 8; picture `2007` on screen 2 (`f14_092c`, Edison's look in its
   palette: the 6-bit tables copied as they are, so it comes out dark);
   colour 10 set to `3F3F3F` (then the picture's own palette, green, is
   shown); three caption lines (`f14_191d`, colour 10, from (112, 284), a
   line every font height + 4; font resource `103`). Then WAVs 0 and 1, and a wait of 60 x `f32_07aa(10)`.
2. Picture `2008` and three lines; WAVs 2, 3 and 4; a wait of 60.
3. Picture `2008` and two lines, copied to the display; WAVs 5, 6 and 7;
   a wait of 600 x `f32_07aa(1)`.
4. Screen 1, fill the clip box with colour 2, fade (`f14_0000(3)`), palette
   (`f14_003a(1)`).

The title before it (`f32_0319`): FM sound `0D` (`SADLIB`), picture `2000`
on screen 1, up to 130 countdown ticks or a key or click, then `f20_0094`.

## The laboratory (room 501, `f19_0a59`, ported: `lab.cpp`)

- `wscience.edi`: the last look, four numbers each followed by `" 
"`;
  `wscience.hs`: `HSFILE ` then lines ` name score a b hair face shirt
  trousers 
` (up to 50; the game's folder, else the CD's).
- Picture `2001`; the look into screen 1's palette (`f19_06bc`; the first
  time it also turns the 6-bit tables into 8-bit colours, `f19_0614`).
- Edison's walk (`f19_0335`, the frames at `DS:1C5A`: picture, x, y + E6,
  w, h, delay / 2 ticks): 0 in, 2 a turn, 3 to the machine (drawn: `1428` at
  (300, 86), `1429` at (146, 70)), 1 away ("Cool!" at frame 16, bubble
  `142C`, sound `600C`). Meanwhile the burner and the flask
  (`f19_0f6f`: `13A0`/`13A1`, `13A2`+k).
- The name (`f19_0249`): bubble `142B`, "Hi! I'm Edison.  What's your name?"
  (colour 0E), sound `600E`, the mouth (`f19_0541`, `5010`-`5013`), then 8
  letters with a caret (`f19_0003`); none is "Player"; it's put in lower
  case with a capital first letter. A player already in `wscience.hs` (the
  first 8 letters, any case) gets their look.
- "Do you wanna change the way / I look?" (bubble `142D`, sound `6014`) is
  asked of everyone, and Edison goes to the machine anyway.
- The Character Enhancer (`f19_0976`, the buttons of segment 18): the panel
  `seg95:0032` (300, 134); buttons `seg95:0000` (the four parts, DONE); a
  click takes the part's next choice (0-7, `f19_07fa`) with its colours at
  once; DONE shows `142A`. Sounds by id are WAVs in `GRAFX.DAT` (`f36_007e`).

## The lessons (rooms 505-510, `f38_0fb5`; the first ported: `lesson.cpp`)

- Each is a game object (segment 15; lesson 5 `f15_0fee`, class `DS:1A41`;
  6 `f15_1524`; 7 `f15_19d4`; 8 `f15_1dbb`; 9 `f15_21a2`; 10 `f15_2591`) on
  the base `f15_076a(this, picture, sound, view)`: FM sound (default 25),
  the look, the display cleared, the picture with the look on screen 2.
  Pictures: 2003 (lessons 5, 7, 8), 2004 (6), 2006 (9), 2005 (10). MORE is
  the lesson's rectangle (528, 370, 88, 24). At the end, event 9 to the
  room at `+138` (lesson 5: room 1; 6: 50).
- The script (lesson 5: `g15_1260`) is a step counter (`+13A`) and a jump
  table (`15:14EE`): each step says a line (`g15_0a1d`: the last bubble
  goes, a new one) and queues the narration the next bubble plays
  (`[171A]`, `f36_00ad(n, 0)`, the name at `seg97:(n * 9 + 9700) & FFFF`;
  the first, WSA1521, is queued by the builder). Lesson 5: anchors (480,
  174) and (540, 124), width 200 (230 once), texts `75F1`, `75F4`, `75FB`,
  `75FC`, `75F5`, `75FD`, `75FE`, `75F6` (one-line text resources).
- The other scripts (from their runners' jump tables): lesson 6 (`2004`,
  then room 50) six lines, 7 (`2003`, room 2) three, 8 (`2003`, room 57)
  three (the last Edison's: anchor (166, 282), tail 0), 9 (`2006`, room
  67) three (Edison (100, 250) tail 0; the professor (352, 300)), 10
  (`2005`, no room) four (anchors (530, 186), (216, 210) tail 1 width
  150, (550, 146)); its last step (`15:29C5`) wins the game: the bubble
  gone, `[26CC]` = 1, then `f31_06c0` with the player (`[26D6]`,
  `f31_001a`): the game recorded (`f40_068b`, below), the high scores
  (`f40_0000` with 0), event 2 (a new game). Entering a lesson keeps the
  room (`+AE`; its end, `f38_020f`, if `+F71`), so the score recorded is
  the last room's.
- Two animated objects in each (added before the bubbles, so under them):
  the professor (`f15_0bde`: `13BB`, 3 frames over 50 ticks, at (500,
  130); lesson 9 `f15_0f19`: `13C7`, 6, at (14, 210); 10 `f15_0e43`:
  `13C1`, 6, at (540, 146)) and Edison (`13AF`, 3 over 100, at (96, 252);
  9: `13B8` at (364, 272); 10: `13B2`, 6, at (202, 218)). Their x, y and
  frame are generators (segment 16: `f16_0000` a constant, `f16_010e` a
  cycle, frame = (tick mod period) x count / period, `f16_0281` up and
  down), stepped each tick (`f15_01fa`, 50 a second); the sprite is the
  frame's base + its value. Each generator counts its own steps from 0
  (`f16_01a8`: frame = (count mod period) x frames / period).
- The easter egg (lessons 5-8's professor, `f15_0bde`, its `+08`
  `g15_0cb7`; lessons 9 and 10's take no clicks, `f10_00c2`): a press on
  him (his rectangle) takes the mouse (`[27AC]`, `[27AE]`) and makes his
  animation `13BE`, 3 frames over 25 ticks (the badge lights); at the
  button's release `13BB`, 3 over 50, again (`f32_00cf` lets the mouse
  go); each a new generator, from its first frame.
- Checked against the original (lessons 6-10 entered through room 61's
  holes to 506-510): every bubble of every lesson, the easter egg in
  lessons 6-8 and, after lesson 10, the game won (below), pixel for
  pixel. (The bubble's edges are stretched by `f14_148a` → `f72_02cd` →
  `f73_0324`: each pixel the source's at a 16.16 step of 256 / scale, as
  room 5's marks; a plain proportional scale missed a pixel here and there
  at their curved ends.)
- The palette: the picture with the look (`f14_092c`) always puts its
  colours 1-254 on the display and its palette into the other screens, so
  a room's table picture (`2002` on screen 3) brings the room's colours
  before its greeting, even after a lesson (lesson 9's `2006` left the
  port's room 67 greeting in the lesson's colours). Only 1-254 go into
  the other screens: their 0 and 255 stay the library's black and white.
  Under winevdm on a true-colour desktop WinG blits each screen with its
  own colour table, so `2002`'s own 255 (255, 247, 247) shows in the panel
  (from screen 3) while the view (from screen 2) shows white; on a
  256-colour display 255 is Windows' static white everywhere, as the port
  draws it (with one display palette).
- The colour cycle: `f32_0e7f(70, 7F, 8)` registers colours 70-7F to turn
  a step every 8 ticks (`f32_1113`, `f14_0148`), but only on a palette
  display (`[61F9]`, from `GetDeviceCaps`).
- The bubble (`g15_0467`, class `1B89`; drawn by its text's `f15_2a35`): a
  box the given width and as tall as the lines (font height each), placed
  by the tail (`f15_31fa`; `[1732]` = 2 here: left of and above the
  anchor by the tail's size); filled with colour F, edged with the
  bitmaps 4 / 3 (left / right, 12 wide, the box's height + 2) and 1 / 2
  (top / bottom, 11 high, the width + 24) stretched (`f14_148a`), the
  lines in colour 0, the tail (5, 7, 6 for types 0-2) at the anchor.
- Wrapping (segment 23, `f23_02a4`): from an estimate of the characters a
  line holds (first (width x width) / the text's width, then the last
  line's length), back to the space before (`f23_0193`, the space left
  for the next line) while too wide, else on to the next space
  (`f23_0218`, the space kept) while it fits.

## The segments

| Segments | What |
|---|---|
| 1, 89, 90, 91 | Borland's run time (C library, C++ exceptions and RTTI, `printf`, a string class) |
| 63-83 | The Artech library (another build of the one in WINMAIN): screens, palettes, disk, sound, timer, mouse, keys |
| 14 | The game's graphics layer over the library (`[27E0]` set means draw) |
| 10, 32, 36, 39, 62 | The framework: game objects, the game loop and start-up (`grafx.DAT`), sounds, errors, WinMain |
| 2-8, 11-13, 24-30, 33-35 | The table: 3D and physics (11 has the floating point, 12 the perspective, 27 reads the `.SRF` rooms: `PANEL`, `OBJ`, `END`) |
| 19, 21, 31, 38, 40 | The player and the name (`wscience.edi`), high scores (`wscience.hs`), the player object, the story and the shot results |
| 41-60 | Twenty segments of about 26 functions each: the rooms' own objects ("electric", "repel", "attract", "gravity", "40 Newtons", ...) |

## The framework

Ghidra's function names here start 3 bytes after `nedis.py`'s (it skips
Borland's `mov ax, ss; nop; inc bp` prologue): `f31_0783` is
`FUN_10f0_0786`.

- **WinMain** (`f62_0020`): four command-line switches (none: `[5FF6]` =
  1); the CD is the first of drives A-Z that `f62_0000` accepts, kept as
  `X:` at `seg97:0029` for the sounds; then the game object is built at
  `DS:5FFC` (`f31_0025`, play area (36, 6, 212, 118)), `[92BA]` points at
  it, and the message loop runs the game loop when idle.
- **The game class** (segment 32, vtable `DS:280A` at `+8C`): `f32_0319`
  builds it: 640 x 400, the clip boxes, resource `103`, the title
  (`2000`, 130 ticks or a key or click) when `[26CE]` is set, picture `2002`
  on screen 2, then event 1. It keeps the game objects (a list at `+86`,
  count `+88`); its tick (`f32_11a5`, method 1) calls each object's method
  0 (an object's vtable is at `+10`). Methods: 0 `f32_122f`, 1 `f32_11a5`,
  2 `f32_134d`, 3 `f32_123f`, 4 `f32_1292`, 5 `f32_1419`, 6 `f32_1237`.
- **The player class** (segment 31, vtable `DS:2775`) derives from it and is
  the game object: `f31_0025` (named "Player"; the story `f38_0718` when
  `[26CE]` is set). Methods 0 `f31_1b45`, 1 `f31_1c79`, 2 `f31_241a`, 3
  `f31_1d70`, 4 `f31_27de`, 5 `f32_1419`, 6 `f31_0783`.
- **Events** (the queue: `f32_07de` adds, `f32_082c` takes, `f32_0000(n)`
  allocates one of n bytes, the first byte its type): 1, 2 go to method 0
  (with 0 or 1), 3 quits, 4 is a tick (method 1), 5 method 4, 6 an
  object's method 12, 7 method 2, 8 method 3, 9 method 6.
- **Event 9 is "go to room n"** (`f31_0783`, its word at `+1` the room, `+3`
  an object): the room number is kept at `+8E` (the last at `+90`).
  - Rooms 1-100: the old room's objects go, the screen is cleared and
    faded, and the room's builder is called (a switch: room 1 `f41_0000`,
    2 `f41_0769`, ... about five a segment through segment 60), which
    returns the room (kept at `+AE`). Room 65 adds two pictures of its own.
  - 501 the lab's name (`f38_06dd`, then `f19_0a59`), then room 1; 502
    the high scores (segment 40); 503 `f38_0eb9`; 504 leaves (event 3);
    505 and up the professor's lessons (`f38_0fb5` with segment 15's classes).
- **Rooms**: an object of `FC6` bytes with two vtables: `+5E` the room's
  (the base class is segment 27, which also reads the `.SRF` files; each
  room overrides some methods in its own segment) and `+70` the game
  object's (segment 10, with thunks). Room 1, the arcade's menu: method 4
  (`f41_0123`) draws its labels (`1399`-`139F`, High Score, Lab, Credits,
  play room, Levels 1, 4 and 5), the logo (`13D0`) and EXIT (`13D2`) on
  screen 3. The builder sets the gravity from `[1FF2]` first.
- Other classes (vtables stored at `+10` by their constructors): about 60,
  most in segments 2-8 (the table's objects), 15 (the lessons' pictures and
  buttons) and 28-30.

## The arcade's table (being mapped)

- **A room is also its camera.** The classes go camera (segment 25,
  `f25_03bf`, class `1F62`) → room base (27, `f27_03da`, `2051`) → room
  (26, `1F76`) → each room's own (41-60; room 1 `2E29`), built as
  `room(parent, [1FEE] = 809, [1FF0] = 789, view)`, the view the player's
  play area (54, 6, 530 x 280). Camera fields: `+46` the angle (`0x2000`,
  45 degrees; `f25_07cb` takes its sine and cosine through segment 86 into
  `+4C` / `+4E`, in 32767ths: 23170 and 23152, as the table isn't
  symmetric), `+48` / `+4A` the world's size, `+50`-`+56` the view, `+58` /
  `+5A` the scroll (room 1: -[1FF2] = -279, 0; set by `f25_0779`), `+5C` the
  view's bottom row (285). The camera is also a box (`f12_0312`): the room
  is the root of its own boxes.
- **The projection** (`f25_0813`) is oblique: a point (x, y, height) is at
  screen x = x + y cos k + `+50` + `+58`, screen y = `+5C` + `+5A` -
  (y sin k + height), with k = `[1F56]` / `[1F58]` = 5 / 10 (each term
  `y * cos * 5 / (10 * 32767)` in longs, truncated). So x is 1:1,
  depth is drawn at 0.354, and the world is wider than the view (room 1's
  walls cover what's left of x 269, which lands at the view's left edge).
  Angles of `0x4000` and more are the "left perspective case not
  implemented" (`f12_093b`).
- **Drawing a room** (`f27_0e5b`, ported: `table.cpp`): the room's
  method 0 (`f27_0ec5`) makes the room the current camera (`f12_0d2f`),
  then on screen 3 draws the view's border in colour 0 and, through the
  tree (`f12_38ad`, a box before its children), every box standing up
  (`f12_2a09`), while for a pit it fills its mouth (`f12_10cd` case 6, the
  bottom rectangle) with colour 0, and its front and right sides too
  where they're the parent's edges (`f12_3835`); on screen 2 the pits
  (`f12_39b5`); then screen 3 over screen 2 within the view, colour 0
  showing what's under it (so the pits show through). Lines go through
  `f14_15f1` (each end clamped into the clip, `f11_0aea`). Then the
  machine (`2002`, with the look) on screen 3, screen 2's view copied into
  its window (`+60`), and the whole to the display. The room's method 4 then draws its own pictures on
  screen 3 (room 1: the holes' labels, the logo, EXIT).
- **Boxes** (the `.SRF` nodes, made by the room's method 1, `f27_12b6`,
  through `f25_057b`; class `f12_0312`, `116E`): bottom and top
  rectangles, a height, and five faces (segment 34, `f34_00f9`; types 1
  top, 2-5 the sides). The faces are the ball's height map:
  `f34_02fa` finds the face under a point and `f34_07ec` interpolates the
  height across a slope. They're drawn as textured polygons (bitmap fill,
  `f63_20e4`) and pattern fills (`f14_07a0`): segment 12's `f12_0ddf`,
  `f12_220d`, `f12_2871`, `f12_2a09`. This build's bitmap fill doesn't
  tile: it stretches the bitmap onto the polygon (cut to the clip first,
  `f83_0065`; `f81_0000` each row's span; `f82_02f0`, 32-bit code that
  `nedis.py` can't read: `ndisasm -b 32` can), the bitmap's rows over the
  polygon's rows and each row over its span, in 16.16 steps; so each face
  carries one whole gradient (the floor lighter towards the front). With
  it room 1's walls and floor match the original exactly; the steep grid
  lines still step differently (the library's line, `f63_1cd5`, isn't the
  engine's Bresenham).
- **A box's fields** (`46h` bytes; the root made by `f12_0312(w, h)`, the
  others by `f12_04a1(parent, rect, height)`): `+2` the parent (the root's
  is a stand-in at `DS:116E`), `+4` the bottom rectangle (x, y, w, h, at
  the parent's height), `+C` the top (clipped to the bottom), `+14` the
  height (below the parent's: a pit), `+2E` / `+30` the grid's step (30,
  the parent's; 0 when the top is narrower), `+32`-`+3A` the five faces
  (types 1, 4, 5, 3, 2), `+3C` the faces' looks (`DS:8CCC` for none, else
  five pointers, face types 1-5, `DS:926C` for none; `f12_0872` sets
  them), `+3E`-`+44` which sides the camera can't see (`f12_093b`, from
  three projected corners: for a box standing up `+3E` the left, by the
  slope of its edges, and `+40` the back; for a pit `+42` the right and
  `+44` the front).
- **Making a box** (`f12_3e0d`): the bottom (the file's second
  rectangle) within the parent's top (none if empty); the top (the first)
  within the bottom, but only if its corner lies inside the bottom
  (`f12_04a1`), else the top is the bottom.
- **Drawing a box** (`f12_2a09`): the eight corners projected (top at
  `+14`, bottom at the parent's height), then the faces back to front:
  4 the back (y far), 2 the left (x near), 3 the right, 5 the front, 1 the
  top. A face with no look of its own is filled with a texture: the back
  and front `1005`, the sides `1006`, the top `100D`; for a pit, the right
  side only where it isn't the parent's edge and the front only where it
  isn't the parent's front. A look is a colour (`f14_07a0`, a solid fill)
  or, with its third word 0, a picture at a point (`f14_1179`). The root
  has only its top (`f12_2871`: `100D`, the projected corners with +1 on
  the right). The textures use colours 80-8F, greys in `2002`'s palette.
- **The grid**: unless `[11F2]` is set, from each grid point of the
  bottom rectangle (multiples of the step) lines to the next in x
  (`f12_335e`) and y (`f12_35c5`), each a pair, colour 84 one pixel left
  (or up) of colour 81, at the heights under their ends (`f12_44f9`): a
  segment whose far end is on the same face is drawn if that face is
  textured and not hidden, else it's split in halves (recursively, the
  second half on the face where it starts). The face under a point is
  `f34_02fa` (the top, or a side by the corner's diagonals); the height on
  a side runs from the box's at the top's edge to the parent's at the
  bottom's (`f34_016a`, `f34_07ec`).
- **Redrawing a box** (`f12_220d`, from the redraw below) goes through
  the same faces in the same order but only outlines what changed (the
  polygons clipped by the library's `f83_0065`).
- Segment 11 is rectangles (union `f11_08b7`, overlap `f11_0a26`, clamp
  a point `f11_0aea`, intersection `f11_0c12`).
- **Redrawing** (the room's method 3, `f27_1e36`, for a changed
  rectangle within the view): the area cleared to colour 0, then the
  room's drawable objects (a list at `+1AD`, 17h bytes each, the count at
  `+EFD`, up to 30h) painted back to front: a 30h x 30h table at `+5FD`
  says which of two objects is in front (`f27_19d9`; rebuilt by
  `f27_1af3` when `+F1D` is set, brought up to date by `f27_1bd9` when
  `+F1B` is), each drawn by `f27_1d2f` (boxes through `f35_04ca` →
  `f12_220d`); then the score (`+F35`) and "shots: n" (`+F39`, at the
  view's corners), five more sprites (`+F3D`), and the area to the
  display (or through screen 3 with `[2024]`). Segment 35 keeps the clip
  for it (`f35_0015`, `f35_0046`).

- **Entering a room** (`f31_0783`, rooms 1-100; ported: `enterRoom`):
  screen 2 filled with colour 2; the builder (the shape, the objects, the
  panel, then the room's method 4: `f27_0e5b` with its last argument 0,
  so the machine and table on screen 3 only, then the room's pictures on
  screen 3: room 1's `f41_0126` the labels `1399`-`139F` at (58, 168),
  (104, 139), (144, 98), (236, 80), (320, 76), (408, 76), (504, 29), the
  logo `13D0` at (54, 6) and EXIT `13D2` at (478, 212), each through
  `f14_1179`, a transparent sprite drawn only if it fits); screen 3 to
  the display; then event 5, the player's method 4 (`f31_27de`): the
  room's redraw of the whole screen, then the panel's controls (`+A4`,
  `+A8`, `+AA`, `+AC`, their method `+40`).
- **The redraw at rest**: besides the objects, the score (`+F35`, a long,
  `ltoa`) in the yellow box `1425` at (480, 8), the text at (+17h, +3) in
  colour 10; " shots: n" (`DS:2040`, `+F39`) in the same box at (58, 8),
  text at (+3, +3); the five impact marks (`+F65`, `1163` + state) only
  after shots. The final copy from screen 3 is keyed on the destination
  (`f14_0c88` → `f65_0294`: screen 3's pixel only where screen 2 is still
  colour 0), unlike `f14_0c4f`'s (source-keyed).
- **Objects on screen**: an object's box (x, y, z, w, d, h; at its core
  `+6E`) has its near bottom corner and far top corner projected
  (`f25_0a51`); the rectangle (A.x, B.y, B.x - A.x + 2, A.y - B.y + 2)
  (`f27_16ae`) and its centre (half the width and height, shifted) is
  where its sprite is centred (`f14_0d69` at 1:1: colour 0 left out, and
  skipped unless it lies wholly inside the clip, `[1706]`: the whole
  screen, so a sprite past the view's edge is drawn, cut off by it).
  - A hole (type 8; `f61_011d` → `f28_00f3`, or `f28_0391` the second of
    a door when `e` isn't 0): `a` the room, `b` the wall (0 the left
    one), `c` big, `d` its height (-1: the face's under it, `f27_0903`).
    Its box is a cube of side 2r (r 17 big, 11 small), moved r to the
    left on the left wall. At rest (`f28_0d82`, frame 0) the sprite is
    `1247` (small, left wall; 3 up), `1245` (small), `1249` (big; 3 down)
    or `124B` (big, left wall) (`DS:20FC`, `DS:2114`).
  - The ball (type 1, `f06_0043`, radius 10, on the face under it): its
    box (x-10, y-10, z, 21, 21, 21); its sprite the rolling frames at
    `DS:1300` (`1016`-`101B`, 32 x 21), frame 0 at rest. Its target
    (`f06_0877`, kept at `+F79`): box (x, y, z, 20, 20, 10), the ring's
    back `1042` at (centre + 1, centre); after the drawables, with
    `[1508]` set, its front `1043` at the same place over the ball.
- **The library's lines and polygon edges** (`f63_1cd5` → `f80_0024`,
  and `f81_0000`, both 32-bit code, `ndisasm -b 32`): one run of pixels a
  row from the top end down, the runs' ends stepping by dx / dy in 16.16
  at the half rows (P = 2 x + (2k + 1) step, the next end ceil(int(P) /
  2)); a polygon's rows are the union of its edges' runs. `f14_15f1`
  clamps a line's ends into `[1706]`, the whole screen, not the view: the
  grid's lines that run out of the view land where the machine covers
  them.
- The ball's shadow (`f13_01ce`, with the linked object's `+60` set and
  `[14E0]` clear): `1040` at the ball's centre, the radius less one lower,
  before the ball.
- With these, room 1 at rest matches the original within the view but
  for 5 pixels (on one of the ramp's grid lines); outside it the panel's
  controls and the side columns (the player's `+A4`, `+AA`, `+AC`) aren't
  ported yet.

## Playing a room (ported: `play.cpp`, `panel.cpp`)

- **The loop** (`f32_0c1d`): each pass runs `f32_09a0`, which posts a tick
  (event 4) when the 50 Hz timer's flag is set (`[12F8:0000]`, from the
  multimedia timer: the rate is `f32_076c`'s 50), and a mouse event (7)
  when the button is down (`[6EC4]`), or was pressed since the last
  (`[6EC5]`), or the mouse moved, or the last event had a press (`[27D2]`)
  and the button is up. So a button let go without moving the mouse after
  an event that only had it held isn't seen at all: the control keeps the
  mouse (a quick scripted click leaves the ball type's button down in the
  original too). The event: x, y, the last x, y, and the press at `+9`.
  Its handlers read the button itself (`[6EC4]` bit 0: aiming, `f27_2d15`;
  the sliders): one window message is taken a pass, so a quick click's
  press is handled before its release arrives, and its event sees the
  button down (the port takes a click as held for its event). So the
  click after room 96's greeting aims where it was.
- **The player's tick** (`f31_1c79`): the game objects, then the room's
  tick (method 5, `f27_2434`: its objects' ticks, the impact marks aged
  every 20 ticks, the changed areas redrawn, `f29_0380`), the panel's
  (`f30_1430`: counts `[8E4E]`, the walking figure), `+A8`, the columns'
  (`f30_02d8`).
- **The mouse** (`f31_241a`): to the control that has it (`[27AC]`,
  `[27AE]`), else by place: the room within the view (method 6,
  `f27_2d15`: its clickable objects, then aiming), the panel (`f30_1879`:
  the first control whose area has the point, not locked), `+A8`, the
  columns (`f30_0496`).
- **Keys** (`f31_1d70`): M (the menu) and Q (quit) ask first; P pauses;
  S and two digits go to that room (1-110); s turns the sounds off or on;
  the others go to the room (method 7: `r` takes the ball back) and the
  panel.
- **Aiming** (`f27_2d15`, with the button held): from the point at depth 0
  under the pointer (x = mouse x - 54 + 279, height = 285 - mouse y) along
  the line of sight (-100, 282, -100), normalised (`f84_0000`: through
  the library's `atan2`, `f87_0804`, a 400h-word table at seg87:0000, and
  its sine, seg86:0000) and scaled 1, 5, 9, ... units long (`f11_02f7`),
  to the first point within 1 of the ground under it; none past depth
  314h. The target is moved there (`f08_056e`: sound 6026 if it moved
  far), sound 6023. The target's position is its box's centre (its box is
  20 x 20 x 10); made from the ball it starts with its corner at the
  ball's point, and once moved its box sits a unit above the ground.
- **The panel** (`f30_1268`, its controls from the PANEL line by
  `f61_0f76`, drawn by `f30_15f2` over the area under the view):
  - Sliders (`f30_2eb7`; gravity `f30_37f1` (-16 to 4), friction
    `f30_3a88` (0 to 16), power `f30_360f` (0 to 16)): only a knob is drawn
    (`117C`, `117D`, `117E`), on a slant: off = (value - min) * (h - 19) /
    (max - min) up from its rectangle's foot, x + 4 + off / 2 (gravity),
    x + 6 + off / 3 (friction), x + w / 4 - off / 2 (power); the rectangle
    is the kind's first frame's (`DS:234E`, `2362`, `233A`). A press takes
    the mouse (`f30_306a`); while held, every 2 panel ticks (7 after the
    first step) a step towards the pointer (up when it's above the line
    h - index * h / 9 + y - 1, `f30_2fdf`; sounds 602A up, 6029 down).
    Gravity's box shows -value / 4.0; the others the value.
  - Value boxes (`f30_2aa7`): the box joined with the text's extent
    (font 103 is 18 rows) in colour 0, the text a pixel right in colour 16,
    then in colour 23.
  - Buttons (`f30_283c`; the ball type `f30_244e`, the shoot button
    `f30_2730`): a press takes the mouse and the next state (`f30_28f1`),
    the first sprite shown while down. The ball type (`f30_2574`): Magic
    is skipped back to Ice; the names at `DS:22E2` (Ice, Stone, Rubber,
    Iron, Glass, Magic); sound 6007; the ball's frames by type (`f07_05bf`:
    `DS:1348`, `1330`, `1300`, `1378`, `1318`, `1360`).
  - The click areas: gravity (0, 300, 174 x 100), friction (174, 300, 104
    x 100), ball type (278, 300, 116 x 100), power (394, 300, 106 x 100),
    shoot (500, 300, 140 x 100).
- **The columns** (`f30_0000` left, `f30_0860` right): the tube (`140E`,
  `140F`), its balls every 24 rows from the bottom in random rolling
  frames (Borland's `rand`: they change with every redraw), PUSH (`1421`,
  `1422`). The player starts a room with 7 balls on the left and none on
  the right (`+BA`, `+BC`); a column starts with its ball out (`+132`),
  so PUSH (`f30_0496`) only works once the ball's been lost (the player
  then clears the left's, or the right's when the left is empty, or it's
  the high scores: room 502); pushing raises the top ball 6 a tick (sound
  602A) till it's out (`f30_02d8`): one ball fewer, and the ball dropped
  onto the table at the side (x = [1FF2] + 10, height 250 on the left;
  [1FEE] - 10, 140 on the right).
- With these, room 1's controls (aiming on the floor, the ramp and the
  walls, the ball types, the three sliders) match the original pixel for
  pixel; PUSH and the shot wait for the ball's physics.

## The ball's physics (being ported: `physics.cpp`)

Ghidra's `FUN_1038` is segment 8 (not 22): the physics core. Functions
reached only through vtables are missing from `nedis.py`'s listing; the
start-up initialisers too (`mov ax, ss; nop; push ds; mov ds, ax`, found by
scanning the code bytes): `seg8:3A98` sets `[1010]` / `[1014]` = 10 / 100
(the step), `seg7:1941` `[DFA]` / `[DFE]` = 9 / 9. 32-bit code (segments
80-82) reads with `ndisasm -b 32`.

- **The ball** (`f06_0043`): a core (segment 8, `f08_0066`; vtable `0D7A`,
  its tick `f06_0312` → the motion part's `f07_077e`) and a motion part
  (segment 7; the sphere at its `+2`: centre x, y, z and the radius 10;
  `f07_04d5` sets the centre and the core's box, `+6E`, = centre - r, sides
  2r + 1). Core fields: `+40` the last move, `+46` the kick (the shot,
  `f08_13a2`), `+4C` another push (`f08_15d2`), `+52` its kind's record,
  `+54` / `+56` the last object hit, `+5E` on the ground (starts 1), `+62`
  the velocity, `+68` the move's remainders (50ths), `+7C` breaking.
- **Kinds** (`f33_0003` at start-up, `DS:286C` + 26h each): `+1` the bounce
  (Ice 4/10, Stone 4/10, Rubber 9/10, Iron 2/10, Glass 4/10, Magic 15/10),
  `+11` 1 for Iron and Magic (magnetic?), `+21` fragility (Ice 20, Stone 4,
  Glass 32), `+23` 100 or 1000; the mass (the core's `+34` / `+38`, from
  `f07_05bf`): 5, 15, 5, 20, 5, 5.
- **The room's values**: gravity `+EFF` / `+F03` (the slider: p = (-value
  * 4 * 42) / (50 * 2), gravity p * -200 / 10: -120 at 1.0), friction
  `+F07` / `+F0B` (value * 255 * 8 / 256, over 255), power `+F0F` (168 *
  min(value * 63 / 16 (2000 at 16), 195)). `[27B0]` = 50 (the timer's rate,
  `f32_0777`), `[27B2]` = 42.
- **The shot** (`f27_27a3` → `f06_09a1` → `f07_0ca0`): at the target's box
  middle; the kick = (target - the ball's foot) scaled to the power
  (`f11_02f7`), z + 42 * 9000 / (mass * 50); for one tick (two at full
  power, `[E02]` = 32760): the motion part's `+E` counts it down. Sound
  6003; a shot more.
- **The step** (`f08_1a42`, each tick): the acceleration (`f08_125c`: kick
  + push, z + gravity); the face under the ball's foot (`f27_0903`) and
  its normal (`f34_0a10`: flat (0, 0, 100), a slope (its rise, across its
  width), normalised; `+15` its length, `+19` the cosine of its tilt). In
  the air (or the acceleration away from the face): the velocity across
  less 1/100. On the face: the acceleration along it (`f34_0b8a` for pure
  gravity, `f34_0d40` the projection), the velocity along it if its part
  across is 50 or more; friction against the motion: each axis back *
  (|A - A0| summed) * +F07 / (|v| summed * +F0B), at least 1 when it isn't
  0; an axis whose remainder plus -v is under friction * 10 / 100 stops.
  Then v = (v * 100 + a * 10) / 100 within +-5000; the move (v + the
  remainders) in 50ths; a long one (over r * 75 + 1) tried in such steps
  and cut where it would first go under the ground. Then the room's
  solid objects (mass at least 1, within 35 and touching: both told, their
  `+34`; mass 2 or more: a bounce and `f08_0d3e`; 5 sub-steps at full
  power); the world's sides (x, y; the near side can crack the glass,
  `f27_0772`, when (|v along| / 50) * 50 / 42 >= 16; else the ceiling at
  279): bounced (velocity across turned back, all of it times the kind's
  bounce). The face where it lands: a wall (higher than the ball's
  centre, another face): bounced in y if the face under the move in y alone
  is that high, else in x (the others damped unless in rooms 14, 60, 64,
  69, 91, `f08_1802`), and the step again (once, `[101E]`); onto another
  face or landing: the velocity across the face (`f34_0e4c`) turned back
  times the bounce (at most 3/5 landing), the room told (method `+24`:
  nothing in the base room); else the move.
- **Bodies**: the step (`f08_1a42`) is every moving object's: the ball,
  other balls (type 0, mass 10) and loose magnets (type 4); `f08_1843`
  acts only for the player's ball (`+2A` 1). Meeting an object (in the
  room's list order, the body itself skipped; not the last solid one met,
  `+56`, which is forgotten after a step that moves): both told (their
  `+34`); a solid one (mass 2 or more) first gives `f08_1843` the velocity
  backwards (once till `+54` is cleared), then `f08_0d3e`: the line of the
  centres (20 times their difference); each velocity's part along it
  (`f11_0651`, `f11_02f7`) and the rest; along it, axis by axis,
  `f08_0b39` (8087 code, 32-bit floats: P = mA uA + mB uB, E = mA uA^2 +
  mB uB^2, D = (mA P^2 - (mA + mB)(P^2 - mB E)) / mA, its absolute value;
  vA = (P -+ sqrt D) / (mA + mB), the sign by uA's, vB = (P - mA vA) /
  mB, each cut to a long); the rest back, within +-7FFFh; neither on the
  ground; their `+2C` told the velocity (nothing for what doesn't move:
  type 5, 15, 6 and 7's `+2C` are empty). The masses: the ball 5, 15, 5,
  20, 5, 5 by type; magnets 3 level + 3; bullseyes 20; holes and live
  targets 1 (soft); shadows, the target ring, levers and RETRY 0.
- **Small vectors**: segment 11's unit, dot, scale and cross (`f11_01f2`,
  `f11_0651`, `f11_02f7`, `f11_043e`) double a vector whose parts are all
  within +-2 (none 8000h) before `f84_0000`, which takes the length across
  at half a unit: (1, 0, 0) would otherwise come out straight up.
- **The bounce's effect** (`f08_1843`, the player's ball): its hit (|v
  along the normal| / 50 * 50 / 42) times its kind's fragility over 100h
  breaks it (`+7C`); else, at most every 2 timer ticks, sound 6004, 6001
  or 6002 by its speed / 100 / 2.
- **The motion tick** (`f07_077e`): the step; the rolling frames (12, a
  frame each time the squared move across adds up to 6, `+1A`; drawn
  backwards going left or towards the front; sprite = its kind's table
  [frame / 2]); the kick's countdown. Breaking (`+7C`): 6019 / 601A / 6028,
  frames at `DS:12E8`, 13 ticks, then the ball lost.
- **Holes**: the hole's core (vtable `2226`) is told when the player's ball
  touches it (`f28_13fe`) and catches it (`f28_14a2`), animates 22 ticks
  (`[212C]`, `f28_0cd5`), then its method `+20` (`f28_156a`: the room it
  leads to) and the ball parked at (0, 0) height 400.
- **The maths**: `f11_0000` a length (across by the cosine of its heading,
  `f86_106d` 4096ths, then with z), `f11_01f2` normalise, `f11_0651` b along
  a's direction, `f11_02f7` scaled to a length, `f11_043e` a cross product
  (a normalised), all through `f84_0000` (the angles' tables).
- Tested: one shot (aim (180, 250), power 5) ends where the original's
  does, the same pixel and frame, and every one of the 178 states
  (centre, velocity) the original's ball went through on the way, read
  from its memory (`tools/reference/memwatch.py watch WMAINSKP.EXE
  "cx=[[[5ffc+ae]+f77]]+2" ... "vx=[[[5ffc+ae]+f77]+2]+62" ...`), is one
  the port's went through (`SCI_DEBUG=1` logs them; `tracecmp.py`). So do
  full-power shots at the back wall and to the left, shots with every ball
  type, friction 16, shots into the sloped pit (down its slope to its
  floor), and gravity at +4: the ball rises to the ceiling (z 289, above the
  view) and bounces there for good (in the original too, never settling),
  then a shot from there. A real-time click lands at a random point of such
  a bounce, so the port can hold its shot till the ball is in the state the
  original's trace shows just before its shot (`SCI_SHOOT_WHEN=cx,cy,cz,vx,
  vy,vz`), then the paths after it compare. Useful
  addresses: the player object DS:5FFC, its room +AE; the room's gravity
  +EFF / +F03 (longs), friction +F07, power +F0F, shots +F39, its ball
  +F77 (the ball's +0: the motion part, sphere at +2; +2: the core,
  velocity +62). Not yet: other balls (`f08_0d3e`, with the room's
  solid objects), the push (`+4C`).

## The room's tick, the objects on screen (ported: `play.cpp`, `table.cpp`)

- **The tick** (`f27_2434`, the room's method 5): each object's method 0,
  the room's list (`+18E`, count `+190`) **from its end**: in room 1 the
  pit's hole, the target, the shadow, the ball, then the other holes. So
  the target and the shadow see where the ball was. Then `[FFE]` (a long,
  the room ticks from the program's start, back to 0 past 1000) counts
  one; every 20 the glass's marks grow.
- **The target** (`f06_0aa8`): the ball still when its speed (`f11_0000`)
  is at most 100 or it hasn't moved since the last tick; otherwise the
  target is hidden (its drawable's `+60`, which means hidden everywhere).
- **The shadow** is an object of its own (`f07_12d6`, a box 2r+1 square, 2
  high; tick `f07_15ba`, drawn by `f13_04ab`): when the ball moved, shown
  if its bottom is 2 or more off the ground under it, at (x, y, ground + 1)
  (its box's centre projected); hidden, the ball draws `1040` under itself
  (`f13_01ce`: the radius less one below its centre; not while `[14E0]`).
- **The painter's order** (`f27_1af3` → `f27_19d9` → `f35_0744`, drawn by
  `f27_1d2f`): the drawables (`+1AD`, 0x17 bytes: `+8` the object, `+A`
  its kind, 1 for the table's own boxes) compared pairwise into a 48 x 48
  "in front of" matrix (`+5FD`); then in the list's order each with
  nothing in front of it is drawn after everything behind it. Two objects:
  none if their rectangles don't meet or they're apart along the
  projection's slant; else apart along y (nearer in front), x (further
  right in front) or z (higher in front); a hole (kind 3, the loader's
  `+2A`) by the centres across its wall (`+24`: 0 the left one); else
  along the thinnest side of their common box (z first on a tie, then
  y). The table's boxes (kind 1, added as the shape's read, `f27_12b6`
  → `f27_1864`: 24 at most, not the root) first in the list: their
  extent (`+18`, `f12_3aba`: the bottom rectangle, from the lower of the
  two heights, the difference high) and its rectangle (`+26`); against an
  object (overlapping on every axis), a box standing up is behind it if
  the object's centre (a unit lower) is over its top or a side the
  camera sees, or else past its extent (`f12_3bd4`, `f25_027a`), a pit in
  front unless the object is above its rectangle's slanted top edge; two
  boxes, a parent behind (`f12_40fd`). A box is drawn (`f35_04ca`, once an
  object has been, `[2982]`, and only if its rectangle meets the area) by
  cutting to colour 0 on screen 2 (`f12_220d`, clipped to the area) the
  polygons (`f12_10cd`) of the faces the camera can't see, so the table
  shows there over what was drawn behind it: standing up, its back (4,
  `+40`), its left (2, `+3E`) and its bottom (6, `+3E`), together its
  whole outline (room 6's hole at 194, 236, behind the wall, isn't seen);
  a pit, its front (5, `+44`) from the area's left and bottom edges and
  its right (3, `+42`) and all right of it to the area's edge. (A face
  with a look, `f12_00de`, draws the look instead, `f14_12e9` or
  `f14_07a0`.) A pit's cut reaches the area's edges, so what was drawn
  beside it before it is wiped there: the areas are kept small by the
  redraw of changed rectangles (`f29_0380`, below). Checked: room 3 at
  rest, its second N block (in a pit) and a hole on its left wall cut
  away as in the original.
- Sprites are clipped to `[1706]`, the whole screen: only the view's part
  is copied to the display, so one past the view's edge shows cut off.
- Checked against the original: a shot's frames, the ball at the
  ceiling, in the pit, swallowed by a hole, breaking, all pixel for pixel.

## Holes, rooms, losing the ball (ported: `holes.cpp`, `science.cpp`)

- **A hole** (`f28_00f3`; type 8; methods: tick `f28_0671`, draw
  `f28_0d82`, `+1C` hit `f28_13fe`, `+20` `f28_156a`, `+28` swallow
  `f28_14a2`, `+2C` spit `f28_1442`): its sphere (`+2`) its box's centre a
  unit lower, radius r / 2. The ball's step (`f08_1a42`) looks through the
  room's objects whose `+34 / +38` isn't 0 (holes 1, the ball 5): within
  35 on each axis and the two radii of the ball's centre after its move.
  A soft one (under 2) takes it: an idle hole stops and hides the ball
  (`f07_0ead`, `f07_03be`), the room busy (`+F6F`); the ball's move,
  remainder and kick are cleared (the step goes on with its own copy).
- **The swallow** (`+21`; and the spit, `+23`): each tick the ball stopped
  (its `+2C`'s `+10`, `f07_0ead` → `f08_0721`: velocity, remainder and
  kick, `f08_13a2`) and hidden (`f07_0381`), so a field only gives it a
  tick's pull it never uses (room 62); a frame a tick, `[212C]` = 22 ticks; frame `+21 *
  5 / 22 + 1` (1-5), the sprite from `DS:20FC` (small) or `DS:2114` (big),
  6 a wall, frames 1-4 plus the ball's type's offset (`f28_0cd5`: 29
  sprites a type). Then the room's method 8 (`f28_156a`): room 1's
  (`f41_02a4`, see the dialog boxes below) asks for EXIT (504) and the passwords of levels 4 and 5
  (508 "electric", 509 "wildway"; wrong: spat out, mode 2); others go
  to `f27_2530`: spat out (mode 0) if it leads to this room, a door within
  the room (0, 100-500) passes it on (its other half's `+2C`, mode 0),
  else event 9 to that room.
- **Doors** (`f61_011d`): a hole whose `e` isn't 0 is the second half of a
  door (`f28_0391`, as `f28_00f3` but its `+C` 0 and its `+0E` the hole
  the file made before it, the room's `+F96`; none: "improper door
  construction"). The ball into it comes out of that hole (spit mode 0);
  into the first half, as its own `+C` says (most lead back into the room:
  spat out of itself). One way: rooms 3, 6, 7, 13, 35, 57, 58, 67, 70 and
  others. `f` not 0 clears the hole's `+2D` (event 9 then isn't told the
  hole). Checked: room 13's (in at OBJ5, out of OBJ4, the hole to 15), 189
  states traced.
- **Spitting out** (`+23`, `f28_1445`): the frames backwards (only one in
  modes 1 and 2); then mode 2 puts the ball back (`f27_287d`), 0 shoots it
  50 out of the wall (`f27_27a3`, no shot counted), 1 a door's.
- **Event 9** (`f31_0783`): rooms 1-100 tables (the new room's own class,
  segments 41-60: its pictures and objects); 501 the lab, then room 1 when
  it came from there (else lesson 5); 502 the high scores, 503 the credits
  (both then back); 504 quits; 505-510 the lessons. The player's ball
  counts (`+BA`, `+BC`) stay (7 and 0 for a new game, `f31_1b48`). Object
  type 3 (`f61_09bd` → `f06_0348`) is the player's ball too, with a
  segment 5 part.
- **Breaking** (`+7C`, set by `f08_1843` on a hard hit): in the ball's
  tick (`f07_077e`), on even `[FFE]`: sound 6019 (glass) or 601A, then 13
  frames (`+1A`) from its type's table (`+22`: `DS:13E4` Ice, `13BA`
  Stone, `1390` Rubber, `1438` Iron, `1462` Glass, `140E` Magic; 6 bytes,
  the sprite first), drawn alone; then the player's ball is lost.
- **Lost** (`f31_0504`): put back (`f27_287d`), then away (on the ground at
  0, 0: z 410 in room 1) and hidden; shots 0; the left column's PUSH
  ready if it has balls, else the right's; else the high scores (502).
- **PUSH** (`f30_0496`, then `f30_02d8`): the column's top ball rises 6 a
  tick; out, the ball is made again, shown at the column's top (x 289 or
  799, y 0, bottom 250 or 140) and slid (`f27_293b`) to the room's place
  (`+F73`, `+F75`, on the ground) 2 a step on each axis, the room drawn
  each step, nothing else running (about half a millisecond a step under
  winevdm: the whole slide some 60 ms).
- **The glass** (`f27_0772`, from the near wall's bounce at 16 or more):
  sound 6019; a mark (one of five: `+F3D` rectangles, `+F65` stages) at the
  ball's rectangle's corner, the size of `1164`; every 20 room ticks a
  stage more, to 3; drawn after the score boxes as `1163 +` the stage (2
  at most).
- Traced against the original (`memwatch.py`): a ball into a hole, a glass
  ball breaking, the PUSH slide, step by step; their frames pixel for
  pixel.

## Each room's class, point targets (ported: `table.cpp`, `targets.cpp`)

- **A room's class** (its builder in the dispatcher's switch, segments
  41-60): method 4 draws its pictures on screen 3 (`show_Clogo` at fixed
  places: two to seven a room, none in 81-90 and a few others; room 2 even
  paints a hole, `1245`). The builder also sets `[30C]` (the targets'
  points by the shots), `+F7B` (the completion bonus; 1500 by default, 0
  in room 1) and two tables of 6 bytes (from a shot count on,
  `f27_088b`: `+F8D` the targets', `f27_0859`: `+F87` the completion's;
  128ths, 128 by default). `tools/testing/roompics.py` takes them all.
- **Point targets** (type 10; `f03_002c`: `c` the kind, at most 10; `b`
  the points, at most 1000, in hundreds; `a` unused): a cube on the ground
  at its corner, twice its kind's size (`DS:2F4`, a word a kind: 13, kind 2
  17, kind 7 10; `f08_0384`), sphere at its centre a unit lower, radius the
  size; soft (the ball goes on). Counted in `[30A]`, hits in `[308]` (`f03_0014`:
  all hit, which some rooms' holes ask before letting the ball through).
  Its tick (`f03_0207`) every 11 ticks, by `[FFE]` and its creation number
  (`+1E`: the ball makes three): idle, the next of its kind's frames
  (`DS:15CC`: 24 bytes a kind, the count then the sprites); hit (`f03_0593`:
  sound 601A, its box 8 bigger each way, no longer met), its kind's hit
  frames (`DS:2CC`: the first and how many), then its points (with `[30C]`
  times `+F8D[shots]` / 128) to the score (`f06_0208`: `[BD8]`, the game's,
  shown in the room's box; sound 602B), shown as `1236` + hundreds - 1;
  five counts on, gone.
- **Kind 3** (the lips, `1374`-`1379`, `1158`-`115A`): hit (`f03_0593`)
  every time the ball meets it (no points, no `[308]`): its box 8 bigger
  each way; the player's ball, if Ice (its type record's first byte 0:
  the records `DS:286C` Ice ... `292A` Magic are 0-5), caught (`+10` 0:
  put on the ground at 0, 0 by `f08_056e`, which sounds 6026 for a move
  of more than 15, then stopped and hidden, `f07_0ead`, `f07_03be`), else
  broken (`+7C`; sound 6019 for Glass, 601A else); its hit frames. In its
  tick, holding it keeps it there; after its hit frames (5 counts on),
  holding, it spits (`DS:15C4`: `1166`-`1169`, `+10` 1); 3 counts on the
  ball comes out on the ground 34 in front of its sphere's centre, shown
  (`f07_03fb`), and is shot (`f27_27a3`, no shot counted) 20 further
  forward at the centre's height; then (and after a ball it broke) idle
  again, its box back. Checked in room 2 (a Stone ball broken, an Ice
  ball caught and thrown three times over, traced state for state).
- **Locked controls** (PANEL's flags, `f61_0f76`): locked either way; 2
  shows an OUT OF ORDER sign at once (`f30_2097`), 1 queues it for Edison
  to put up (`f30_1569`; from the right, the list backwards). Signs: a
  slider's `134C` at its frame's middle a pixel right (`f30_20ec`, its
  `+1A`: the frame at the slider's place), the ball type's `134D` 6 right
  and 4 down of its picture's middle (`f30_25fd`).
- **Edison putting them up** (the panel is segment 30's walking figure:
  `f30_0910`; its tick `f30_0d7c` from the panel's `f30_1430`, every
  `+142` ticks of `[22CA]`): he starts off the panel, a random side (x -60
  or 719), at the panel's middle + 4 (the panel is (0, 286, 640, 114): y
  347). To each control (`f30_148f`): 48 past its right edge coming from
  the right, else 48 before its left (a slider's edge is where it takes
  the mouse). Moving (`+15E` 1), 17 a step, frames by mode (`f30_0c6e`:
  1 running 14-23 / 38-47 every 2 ticks, 3 carrying 0-9 / 24-33 every 2, 2
  putting up 10-13 / 34-37 every 5; the second set facing right); on
  arriving (the step not taken) a mode change; at the control (`+15E` 2)
  he still slides 17 a step, and after the 4 frames the sign goes up
  (`f30_152c`) and he heads for the next or off the panel (-60 or 719),
  where he stops and the controls take the mouse again (`[22C8]`). A place
  off the panel (below 0, or 639 and on) only stops his frames on
  arriving (`f30_0c6e` with 0): gravity's sign, coming from the left (48
  left of it, -48), then goes up at once.
  Footsteps: sound 6009 on frames 1, 6, 15, 20, 25, 30, 39, 44. Drawn
  (`f30_1120` → `f14_12e9` → `f72_02cd`) as `131C` + his frame, on the last
  row of his 96 x 104 rectangle round his middle. Checked: his state, read
  from the original (`memwatch.py`), step for step, and 30 screenshots
  pixel for pixel.
- Checked against the original: rooms 31 and 21 (pictures, targets at
  rest pixel for pixel but for their phases), a shot in room 21 traced
  state for state from room 1 (through the hole) and its score (1600).

## The dialog boxes (segment 24, ported: `dialog.cpp`)

- **A box** (1CCh bytes on the caller's stack; built by `f24_0003`,
  `00df`, `01be`, `02ba`, `03a3` (typed), `0482` (buttons), `057d` (a list
  of strings)): `+0` the parent's rectangle, `+2` the buttons, `+4` one
  field (OK or typed), `+6` / `+8` its middle (then its corner), `+A` / `+C`
  the picture's size, `+E` / `+10` an extra picture and how it's aligned
  (`f24_06bf`: low nibble 1 centred, 2 left, else right; high 10h, 20h,
  else bottom; 80h it is the box), `+12` the message (text 7000h + n),
  `+14` the first button's text, `+16` the box, `+1E` the stand, `+26` the
  style (0 framed, 1 plain), `+28` the answer, `+2C` the first button's
  row, `+30` the face, `+38` screen 2 under the face, `+3A` / `+3C` the
  mouth, `+3E` closed, `+40` what was typed (64), `+81` five strings
  (`057d`), `+1C6` the highlighted one, `+1CA` typing. `[1F1C]` is Edison's
  face (0 none; sprites `13A3` + 4 x max(face, 1) + frame, frame 2 at
  rest). Room 1 puts each in the middle of the room's window (`+60`, the
  view: (319, 146)).
- **Drawing** (`f24_0e8c`, on screen 2, then the frame to the display): the
  picture, for style 0 one of `1359`-`135D` at random (`DS:1E7C`, ten
  entries, Borland's `rand` x 10 / 8000h), style 1 `1393` (`DS:1EA4`);
  `f24_0a34`: the stand `1394` centred under the frame's foot (unless
  `GetPixel` on screen 2 there returns 2, which an RGB never is), and for
  style 0 the frame (`f24_07e1`: 22 out, 328 x 236; `138F` top and `1392`
  left at its corner, `1390` bottom and `1391` right against its far
  edges). The buttons' rows from the picture's foot, 28 apart and 10 up
  (not for style 0): `f24_08a9` (x + 23, row, 223 x 23). The message
  (`f24_095e`: x + 13 (style 0) or 23, y + 8 or 28, 246 wide, down to the
  buttons) through segment 23 (left, colour F, a line only if its corner is
  in the rectangle; the first estimate is (w x w) / w here: the layout's
  base class, `DS:17ED`, measures characters until `f15_3409` swaps in the
  pixel one, `DS:17FD`); each button `1396` with its text centred
  (alignment 1, the first line only, 4 down); one field: `1396` and the
  text 4 right and 5 down (`f24_0cc7`: "OK..." at `DS:1F3D`, or what was
  typed). The face: frame 2's size, x = (w - fw) x 5 / 8 + 10 (50, 54 or 26
  more by face and style), its foot 4 above the picture's (style 1) or 2 /
  8 below.
- **Waiting** (`f24_193e`): every pass handles one Windows message
  (`f36_0000`) and waits 3 ticks without handling any (`f24_1d43`, a busy
  wait on the 50 Hz counter), the face's next frame every sixth pass
  (`DS:1F1E`: 1 0 1 2 3 2 1, from a 1 on to 1 or 3 at random). No
  buttons: a key or a click. Buttons: a key (`y`, or `Y` with several
  buttons, is the second; any other the first), or the button held
  (`[6EC4]`, set by WM_LBUTTONDOWN and cleared by WM_LBUTTONUP) with the
  mouse in **the button tried this pass** (they're tried in turn, one a
  pass): a click held for one pass only counts on its button's turn, so
  with NO / YES half the clicks are missed (seen in the original: clicks
  on YES 30 ms apart taken, missed, taken, missed). One field: any click.
  The press (`f24_0d89`): sound 6025, the button down (`1398`) with "ZAP!"
  (`DS:1F38`), 30 ticks.
- **Typing** (`f24_1b15`): letters, digits and spaces up to 64,
  backspace or left takes one away, Del clears (not in the port: its
  platform has no Del key), Enter ends; each pass the
  field, a caret (8 wide, the font's height less 4, after the text) in
  colour F or 56 by the pass (8 each), the face again over the field; no
  wait, so the caret flickers faster than screenshots 20 ms apart can
  follow (the port takes a pass a millisecond).
- **Closing** (`f24_1f26`): event 5 to the player (its method 4) for the
  frame and again for the stand: the room's redraw there and every
  control's.
- **Room 1** (`f41_02a4`): EXIT (504): face 0, style 0, "Do you want to quit
  this game?" (`720B`), NO / YES (`7202`, `7203`), sound 6016; NO spits the
  ball back (mode 2). Levels 4 and 5 (508, 509): face 2, style 1, typed,
  "Please type in the warp code:" (`7002`), narration 6102; compared with
  "electric" / "wildway" by `strnicmp` (64); then a box with no face,
  "That's right!" (`7205`, narration 6148) and the room's `+F7F` set to
  60000 / 75000, or "Sorry, wrong answer." (`7204`, 6147) and the ball spat
  back. `+F7F` reaches the score (`f06_0208`) at the room's end
  (`f38_020f`), which event 9 runs when the next table comes: after the
  lesson, room 57 opens at 60000 (read from the original).
- Checked against the original: EXIT's box, NO by a key, the ball back,
  the warp code's box, typing, the caret, "That's right!" and "Sorry,
  wrong answer." with ZAP!, all pixel for pixel; the clicks' turns; the
  score in room 57. Still different: after the wrong code the original
  shows no shadow under the ball put back and the port draws one (29
  pixels; the shadow object's state through the spit, `f07_15ba`).
- Room 1's holes: 502 High Score, 501 Lab, 503 Credits, 31 the play room,
  21 Level 1, 508 and 509 Levels 4 and 5, 504 EXIT. Every one checked
  against the original from a shot in room 1 (power 5: the lab (280, 156),
  31 (260, 148), 21 (332, 148)): rooms 31 and 21 as they come (21's
  targets rising, the walker's side aside), the lab through the name, the
  Character Enhancer and DONE back to room 1 (0 pixels at rest but for
  the professor's, the flask's and Edison's frames). Left at rest, the
  columns' balls differ from the original's (some 590 pixels): each is a
  random frame (`f30_0599`, Borland's `rand`), and the original's differ
  from run to run too. Between the lab and room 1 the original shows the
  black display a while (the lab's clear, then room 1 built under
  winevdm; `f31_0783`'s `f14_0000(1)` only keeps the palette, no fade).

## Each room's word on its holes, the room's end (ported: `rooms.cpp`)

- **Method 8** (the room's `+5E` table, `+20`; `f28_156a` calls it when a
  hole has swallowed the ball): most rooms only pass on to `f27_2530`;
  47 have their own (`tools/testing` has no generator: transcribed by hand
  from the disassembly). Their pieces: a box with OK (`f24_00df`, or
  `f24_01be` with a picture in it, aligned by `f24_06bf`), two buttons
  (`f24_0482`: "Want to try again?" Give up / Try again, NO / YES, "Would
  you like to reset the screen?"), a code to type (`f24_03a3`, compared
  by `strnicmp`: "repel", "attract", "gravity", "equals", "force",
  "earth", "electricity"; room 21's warp machine "40 Newtons" or
  "40Newtons", "magnetic", "electric", "wildway": lessons 506-509 and
  30000-75000 points), Edison's face (`[1F1C]`) and narration; then the
  ball spat out (mode 0, 1 or 2) or on (`f27_2530`), the hole redirected
  (its `+C`: a door C46h to a bonus room, room 60 always to 64, 64 to
  69), the completion bonus `+F7B` set (2000 for most "right" exits),
  `+F87`'s shares (`f27_0859`), a bonus ball (`+F83`), `+F71` (the room's
  end before a lesson), the room's own fields (`+FA2`-`+FC4`: counters,
  a question asked once) and the game's (`[8E50]`-`[8E54]`). Some check
  that every target is hit (`f03_0014`: `[308] >= [30A]`) or the shots.
- **Hint holes** close after their message (`f28_1514`: `+2F`): drawn as
  `11C6` (small, left wall, 3 up), `11C7`, `11C8` (big, left wall),
  `11C9`, and they no longer swallow the ball (`f28_14a5` returns).
- **Spitting out** (`f28_0671`'s end, by `+1F`): 2 the ball back where the
  room put it (`f27_287d`); 1 set down on the ground in front of the hole
  (`f08_056e`: on the back wall at the sphere's x - its radius + r, y -
  its radius - 4r; on the left wall at x + 4r, y + 1); 0 at the hole's
  mouth (back wall: x - radius, y - radius - 10; left wall: x, y + 1; its
  bottom the sphere's) and shot at the hole's place 50 out of its wall
  (`f27_27a3`, no shot counted). Then the hole stays "leaving" (`+25`)
  till the ball is clear, its physical part's `+34` 0 meanwhile (1 again
  after): out of the ball's step's reach, so it neither takes the ball
  nor clears its remainder.
- **The room's end** (`f38_020f`, event 9 runs it for the old table when
  the next is another table but room 1, or a lesson after a room that set
  `+F71`; room 59 runs it itself): once (`+F3B`); the warp points (`+F7F`)
  to the score; a bonus ball (`+F83`) for each new 10000 points
  (`[29BA]`); then, with a ball and a bonus or a ball to give, `+F39` at
  least 1, the bonus by the shots (`f38_0003`: `+F87[min(shots, 5)]` x
  `+F7B` (kept within 0-100000) / 128): "Bonus Points" (`7206`), the list
  "1 Shot.... Bonus: n", "k Shots... Bonus: n" (2-4), " Your Bonus:   n"
  (highlighted: button `1396`, colour 18), the balls to give over the
  first line (`f24_175a`), on picture `135E` (`f24_057d`); a key or a
  click; the bonus counted in 100 at a time (`f06_0208`, each with its
  sound and the score box redrawn), the highlighted line again in colours
  16-18 in turn (`f24_1829`), 7 ticks each unless Escape is held
  (`[9570]` bit 1: the held keys by scan code); then each ball: sound
  601F, the line blank, the right tube a ball more (at most 4). After a
  lesson (`[171C]`, set by every lesson's end, `f15_08b7`) all of it
  without the box or 601F; till then the score changes silently
  (`f06_0208` skips its sound and box).
- **Arriving** (the builders' own ends, after the room's pictures): a
  greeting on a black screen (`f24_1ee3`; `[275C]`, set while event 9
  builds a room, keeps the box's closing from redrawing): always in rooms
  6, 7 (`7033`), 9, 16, 41, 55, 59, 67 (two), 92, 96; by the room the
  player came from (its `+90`) in 3 (from 55), 13 (from 7), 47 (from 50:
  two, and `[8E50]`, `[8E52]` cleared), 54 (from 47); unless coming from
  the same room in 10, 18, 32, 36, 70. Doors met on arrival: room 34 from
  22 or 40 (the ball out of the hole to 117, mode 0; the doors to 22 and
  40 shut; else its greeting), room 35 from 43 (the ball out of that door,
  mode 2, 2 shots; else the hole to 1000 shut and two boxes, "Friction is
  a force that acts..."). Holes shut by the game's flags: 47 (`[8E50]` the
  door to 33, `[8E52]` to 23), 54 (`[8E54]` 48, `[8E50]` 100 and 101), 96
  (66, always).
- Checked against the original:
  - room 21's hint hole 102 (the box with picture `135F`, the ball back,
    the hole drawn shut);
  - level 1 finished in one bank shot (aim (272, 277), power 10: all five
    targets, then hole 35): the "Bonus Points" box and the count, frame
    for frame (zero pixels differ in the view), and room 35's greeting
    (0 pixels with the same random picture);
  - the warp to room 57 through lesson 8 (60000 and the bonus ball in the
    right tube, the room's end after a lesson); room 1's boxes;
  - spit mode 0 (room 35's door back into itself): the launch, the
    bounce off the hole's own wall and the rest, every state the same
    (`memwatch.py`, remainders `+68`-`+6C` too);
  - spit mode 1, both walls: room 2's hint holes 100 (left wall, aim
    (156, 212)) and 101 (back wall, (400, 176)), power 6, reached in the
    original with the S key (`S02`): the box, the ball set down
    (317, 226, 10) and (529, 292, 10), and the holes drawn shut.
- Rooms 18 (`+FC4`), 34 (`+FA4`, `+FA6`), 35 (`+FC0`) and 55 (`+FB4`,
  `+FB6`) read fields their own code (not ported) sets. Rooms 2 and 35
  at rest match the original (the boxes' order, above) but for the
  columns' random balls and a target's sparkle.

## The room's other objects (being ported: `things.cpp`)

- **An object's core** (`f08_0066`, `80h` bytes, its method table at
  `+10`): `+0` its rectangle on the screen, `+28` the room, `+2A` whose
  (1 the player's ball, 3 a hole's), `+34` / `+38` its mass (two longs;
  0 out of the ball's step's reach, 1 soft, 2 and up solid: the ball 5,
  a switch's bullseye 20), `+52` the ball's type record, `+60` hidden,
  `+6E` its box (x, y, z, w, d, h). Its methods: `+00` its step (the
  ball's `f08_1a42`), `+04` draw, `+08` the mouse, `+10` its area to be
  redrawn (`f08_07a7`), `+18` / `+1C` hide / show, `+34` met by the ball,
  `+4C` its sphere. An object made of parts has a wrapper with its own
  method table (`tools/testing/wm.py vtable OFF` lists one, following
  the thunks, which add to `this`).
- **The mouse in the room** (`f27_2d15`): an object that has it
  (`+186`) first; else the first in the room's list (`+18E`) whose
  rectangle has the point, its `+08`; one that takes it ends there; else
  aiming. The generic `+08` (`f08_07c6`) drags the object; the ball,
  holes, targets and the shadow take none; type 0 drags only in room 0.
- **Type 7, switches** (segment 4: `f04_01a1` kind 0, `f04_03eb` 1,
  `f04_0835` 2, `f04_05b8` 3, by `d`): a 34 cube at (x, y, `b`; -1 the
  face's under it, `f04_00ba`); on or off (`+4`, `f04_008e`: its area
  redrawn); drawn (`f13_0a5b` / `f13_0b03`) as the word at its `+14`
  table + 4 `a` + 2 for on, at its rectangle's centre (`DS:15AA`: the
  lever `11D0` / `11D1`; `15BA` the bullseye `1424` / `1423`; `15B2`
  RETRY `115B` / `115C`, an empty picture). Kinds 0-2 work the power
  (their `+10`: the room's `+F94` as they're made, the last type 6, 12 or
  13; none yet: the switch isn't made, so room 25's OBJ3 never is), and
  with `c` start on. Method 3 (`f04_0321`) sets it, then tells the power
  (the power part's `+0C` with on or off): sound 6026 when switched off,
  and when switched on unless the power's `+10` says `\r` (type 6's,
  `f05_29f2`; 12's and 13's say 2). Type 6's power part is at its `+22`
  (`f04_0000`, table `7B0`): its `+0C` (`f05_29c1`) sounds 6027 when
  switched on, then sets its `+4` (the object's `+26`: its field and its
  picture) and redraws; 12's and 13's (at their `+0C`) are the plain
  `f04_008e`. Kind 0 a lever, clicked (`f04_03ab`: a press switches it
  over and is taken); kind 1 a bullseye (mass 20), switched over when a
  body meets it (`f04_04c9`, unless hidden); kind 2 the same (its sprites
  kind 1's), and its step (`f04_08e1`): every 30 room ticks (`[FFE]`) the
  next of `e`'s 16 bits (`+18`, from -1): set, shown (`+1C`, `f08_04b3`),
  else hidden (`+18`, `f08_0469`: `+60` set and its rectangle emptied, so
  not drawn; nothing in the step tests `+60`, so it stays solid).
  Checked: rooms 61 (a lever clicked on and off, its magnet pulling the
  Iron ball, 435 states) and 62 (both levers on from the start, the ball
  pulled into a hole) traced state for state; room 29 (a bullseye shot
  on, 42 states); room 25's blinker (bits 170: one 30-tick slot shown in
  ten, `[FFE]` read from the original).
- **RETRY** (type 7, `d` 3, `f04_05b8`; mass 0): keeps the game's score
  (`[BD8]`, `f06_0000`) and the player's balls (`+BA`, `+BC`) as the room
  is built. Clicked (`f04_070e`), while neither column waits for its PUSH
  (their `+132`): on (its picture blank), the balls and score back (the
  room's box too, `f06_028a`), no completion bonus (`+F7B` 0) and event 9
  to this room (so the room's end runs, with nothing to give). Its step
  (`f04_06e3`) turns it off on its sixth tick. Checked in room 2.
- **Type 12, the electromagnet** (`f02_00c2`; `a` its period): its motion
  part at `+0` (sphere `+2`), its power part at `+0C` (`f04_0000`, table
  `260`: `+0C` the plain `f04_008e`, `+10` says 2, so its switch sounds
  6026), its core at `+20` (`f02_0000`: a box 44 wide, 2 deep, 52 high at
  (x, y, the ground); mass 1, soft). Its sphere (`f10_1582`) radius 13 at
  the box's centre in x and y, resting on the ground there (centre 13 up).
  `+16` the head's drop, `+18` the box's height less 12 (40), `+1A` `a`,
  `+1C` the way (1 down), `+1E` the frame since a catch (-1), the core's
  `+7C` caught. Its step (`f02_03fb`), only while powered: every `a` room
  ticks (`[FFE]`) the way turned; every 6, caught, the next frame (to 3),
  else the drop 2 more or less, kept within 0 and 39 (redrawn unless it
  stopped there). Met (`f02_02a0`) by the player's ball (`+2A` 1) with
  none caught: if the head's bottom (the ground + 40 - drop, unsigned) is
  at most the ball's top + 1, an Iron ball is caught (`+7C`, frame -1,
  redrawn) and any other breaks (its core's `+7C` 1); a Rubber one also
  gets its motion part's `+2A`, which draws its break from `DS:14B6`
  (every type's `+26`) instead of its own `+22`. A caught ball isn't held:
  it rolls on (room 25: into the hole under it). Drawn (`f13_0dc1`) from its
  rectangle: caught, `115F` + the frame (from 0) at the centre; else the
  head `13D4` (`f14_1179`) at the rectangle's corner lowered by the drop,
  then the frame `13D3` centred over it. Checked: room 90 (the head going
  down and up, `[FFE]` read from the original, pixel for pixel), room 25
  (an Iron ball caught, Glass and Rubber ones broken, traced state for
  state). The original redraws only an object's rectangle (`f08_07a7`), so
  the burst's pieces outside it can stay on the display (ported, below:
  the redraw of changed rectangles). Checked in room 25 (an Iron ball
  caught; Glass and Rubber ones zapped): at rest the same as the original
  but for the head's drop at the moment of the shot.
- **Type 13, the fan** (`f02_05f2`; `a` its way, 0-3): its motion part at
  `+0` (sphere `+2`: a 26 cube's centre a unit lower, radius 13), its power
  part at `+0C` (table `1C4`: `+0C` the plain `f04_008e`, `+10` says 2), its
  core at `+27` (`f02_0530`: a 26 cube at (x, y, the ground); mass 25,
  solid; met: the generic `f08_122b`). `+14` `a`; `+15` / `+17` its quarter
  of headings (from `2000h`, `A000h`, `E000h`, `6000h` by way, to that +
  `4000h`); `+19` its wind's sprites (`DS:16E4` + 8a: `13FF`-`1402`,
  `1409`-`140C`, `13FA`-`13FD`, `1404`-`1407`); `+1B` the blades' frame
  (-1 still); `+1D` its picture (`DS:16DC` + 2a: `13FE`, `1408`, `13F9`,
  `1403`); `+1F` its wind's corner from its rectangle's ((18, -17), (-47,
  11), (22, 0), (-60, 0)) and its size (56 x 36, 56 x 36, 72 x 30, 72 x
  30): each new frame redraws the fan (its core's `+10`) and then that
  area (`f02_0870` → `f29_0494`), so the wind, outside the fan's
  rectangle, leaves nothing behind when it stops (checked in room 25). Its step (`f02_08e9`), while
  powered or its blades still turn, with the player's ball: d from its
  sphere's centre to the ball's. Every 10 room ticks (`[FFE]`), powered
  and d within 8r, the next frame (0-3, round) and sound 601B; else,
  turning, the next till still. Each tick, d within 4r, |dz| at most r and
  d's heading (`f87_0804`) in its quarter (way 2's across 0): `f07_030e`
  with 200, which breaks a ball whose type's `+23` (`f33_0263`: Ice 100,
  Stone 1000, Rubber 100, Iron 1000, Glass 100, Magic 100, from `f33_02a8`)
  is at most 200 and that isn't breaking (`+7C` 1), and sets its motion
  part's `+28`: its break sounds 6028, and an Ice ball's draws from its
  `+24` (`DS:148C`, melting). Drawn (`f13_0f2b`): its picture at its
  rectangle's centre, then, turning, its wind (`f14_1179`) at its corner.
  Checked: room 98 (a Rubber ball broken, 40 states; the fan's area pixel
  for pixel at rest), room 25 (an Ice ball melted, the fan spinning down
  frame for frame).
- **Magnets** (segment 5; the rooms built by `f61_09bd`, segment 26's
  class): each has a magnetic part (`f05_0003`, registered with the room,
  `f26_00c0`; its strength `+6` / `+A`, a ratio of longs). The field at a
  point (`f26_02e2`): every part's `+20` but the asker's, summed. A round
  one's (`f05_0a1b`; the ball's part and types 4, 5): nothing unless its
  core's type record is magnetic (`+11`: only Iron's is 1) and the point
  within 16 of its radii; else k - k |d| / 256 (k its strength, d from
  its sphere's centre, `>> 8` of the product) along d. A body's
  acceleration (its core's `+30`: `f05_0c0b` for the type-3 ball and
  loose magnets): `f08_125c` plus, if its own record is magnetic, the
  field at its centre, backwards when its strength is below 0 (so unlike
  poles pull). The type-3 ball's strength is 200 * [27B2] / ([27B0] * 2)
  = 84 (`f61_09bd`); a magnet's (`f05_1902`) its level's of `DS:728`
  (70, 210, 420, 700, 980, made at start-up while `[27B0]` was still 30)
  times [27B2] times its pole over [27B0] (50).
- **Types 4 and 5** (`f05_1749`, `f05_210d`; a its size and strength, a
  byte; b its pole): a cube of side 2a at (x, y) on the ground
  (`f08_0384`), its sphere at the cube's centre a unit lower, radius a
  (`f07_0ed0`); level 0-4 by a above 7, 9, 12, 16 (`f05_1e60`); mass 3
  level + 3; type Iron (`28DE`). Type 4 steps as a body (`f05_1f76` →
  `f08_1a42`), type 5 doesn't. Drawn (`f13_0720`) as `DS:151E` + 10 for S
  + 2 level (`1045`-`1049` N, `104B`-`104F` S) at its rectangle's centre.
  Checked: room 17 at rest (its Iron ball held on the left wall, its
  loose magnet creeping right for some seconds) and a shot along the wall
  from there, remainders and all, when fired at the same point of the
  magnet's creep (a few ticks off and it drifts apart).
- **Types 15 and 6** (`f05_233e`, `f05_26f7`; a, b, c its wall, d its
  height, -1 the ground): the core's box at (x, y, the ground) (`f05_2243`)
  2a + 6 deep, 2a high, 2a + 6 wide, 2a + 18 on the left wall (c 0); the
  sphere at that box's centre a unit lower (the motion part, `f07_0ed0`,
  takes its centre from the core), radius a - 1; with a height, the centre
  at d + a. Level, strength, mass, type as type 4's (`f05_1749`); neither
  moves. The field (`f05_252f`; type 6 `f05_2933` only while powered,
  `+26`): as the round one but without the type's test, the strength turned
  for points past the centre in x (left wall) or y (back wall). Drawn
  (`f13_0823`, `f13_0937`) as `DS:1532` (type 6 `155A`, powered `1582`) +
  20 for c 1 + 10 for S + 2 level. Checked: room 33 (an Iron ball shot past
  the bar on the white block, traced till it falls in the water: the
  room's `+24`, the ball broken on (405, 278), see below), pixel for pixel
  at rest.
  Checked: room 3 (the Iron ball pulled across to a type-5 magnet and
  held inside it, 868 states) and room 13 (three loose magnets and the
  ball, 625 states), traced (`memwatch.py --follow`) state for state.
- **Type 10 kind 6, the suckhole** (`f02_0be1`; the factory insists the
  player's ball is made first): a fuse that a spark burns along to the ball.
  Its core is shared by its motion part at `+0` (`f07_12d6`, the shadow's
  class, made from the ball: the fuse) and a kind 6 point target at `+18`
  (`f03_002c`, `b` its points); a second target of its own at `+40` (kind
  7, no points: the spark, `f03_002c` with its own core, hidden where the
  suckhole is). So it makes two entries in the room's list and counts two
  targets in `[30A]` (and as the spark is never hit, `[308]` can't reach
  `[30A]` in its rooms); the spark stays soft while hidden, so the ball's
  step meets it (clearing the ball's remainder, as any soft body does). Its
  step (`f13_1036`): till its target's points are scored (`+2C`, the
  target's `+14`), the target's (`f03_0207`); its draw (`f13_15e0`) the
  target's too. Then, when no other fuse burns (`[76]`; `f02_0d35` clears
  it if this one started): the fuse shown, its box 6 x 6 x 2 at the ball's
  centre less its radius on the ground there, its sphere that box's centre
  (`f11_1523`, radius 1), the first point of the track (`DS:881A`, 300 x
  and y; `[8CCA]` its last). Each tick after: the fuse follows the ball
  (`f07_15ba` with `+16` [14E4], so never hidden for being near the ground:
  to the ball's x, y, the ground + 1, each tick the ball's centre changed;
  hidden while the ball breaks, shown again once it's not hidden); `+3E`
  counts to 300; a tick it moved, its sphere's x, y the track's next. Past
  130 ticks the spark is shown and put at the track's start (`f08_056e`:
  sound 6026 for a move of more than 15; its sphere the ground + 10 there),
  then each tick moves on one along the track (`+3C`); where the table seen
  8 below its rectangle's centre (`f27_304c`) is within 6 of the ground
  under it and its rectangle's corner is in the view, the burnt fuse `141B`
  there on screen 3. At the track's last point, the ball still in the room:
  the spark hidden, the ball broken (`+7C`), the suckhole done (`+3A`), the
  fuse hidden, `[76]` clear. The fuse is drawn (`f13_15e0`) on screen 3,
  `1412` at its box's centre projected, where the table seen at its
  rectangle's centre is within 4 of the ground under its sphere: so it's
  left on the table wherever it was redrawn. `f27_304c` (a screen point to
  the table): from depth 0 under the point (x less the view's left and
  scroll, height the view's bottom less y) along the line of sight scaled
  1, 9, 17, ... long, to the first point below the ground; none past depth
  314h. Checked: room 22 (the suckhole hit, the fuse laid, burnt and the
  ball broken, 227 states traced; with `[FFE]`'s phase read from the
  original (`SCI_ROOMTICKS`), the fuse before the spark and the scorched
  trail after it pixel for pixel).
- **Type 9, a pulling hole** (`f28_15a1`, its class "suckHole"; rooms
  6, 11, 17): a hole (`f28_0391`: leads to no room, `+0C` 0) of its own
  sprites (`DS:212E` small, `2146` big) and colour cycle (`f32_0e7f(A0,
  A6, 3)`); `a` unused, `b` the wall, `c` big, `d` its height as a
  hole's (its core's box, `f28_0000`; -1 the ground's): `f28_0391` is
  given -1, but its sphere is at `d` all the same (read in the original:
  room 11's at (243, 219, 50), room 17's at (273, 448, 36) and (554,
  270, -34)); its pull (`+37`)
  200 (big 400) times `[27B2]` over `[27B0]`; `+35` the player's ball. Its
  tick (`f28_18fd`): while idle (`+21`, `+23`, `+25` clear) and the ball
  within 16 times its sphere's radius, the ball pushed (`f08_15d2`, its
  `+4C`) towards the sphere's centre, on each axis the pull less |d| *
  the pull / 256 (`+39` set), then the hole's tick; farther, the push
  taken back once and no tick. Swallowed (`+20`, `f28_1b25`): sound
  `602C`, a busy wait of 132 ticks, the ball lost (`f31_0504`). The push
  is only ever set or cleared there: a hole taking the ball, the spit and
  the ball put back (`f07_0ead`, `f27_287d`) leave it, so the swallowed
  ball is still pushed (stopped again each tick) and leaves for 0, 0 with
  a tick's push in its velocity. Rooms 6 (a swallow and the ball lost)
  and 11 (pulled round the hole, never in) traced state for state.
- **Type 11, the smiley** (`f03_0865`): a kind 0 point target (`a` its
  points) with its own core, a cube of 34 (size 11h), its sphere kind 0's
  (radius 13). Drawn: `1357` at its rectangle's left, 9 above its bottom
  (`f14_1179`), then the target's draw. Four states, each a sequence
  (`f03_01bf`): 0 `16BC`, 1 `16C0`, 2 `16C4` + 2 near, 3 `16CE`. Its tick
  (`f03_091b`), from the ball's foot to its sphere's foot (d) and the
  ball's velocity (v): coming at it (min(dx vy, dy vx) * 100 / the larger
  over 40, vx dx and vz dz not negative, |dz| under 60): state 2, near = 3
  less min(length / ((2r + 1) 4), 3); else with every v / 50 0 (the ball
  at rest): state 0 if cx % 3, else state 1 if cx % 2; else from state 2,
  state 0. Met (`f03_0bc9`): a Rubber ball state 1 and broken; any other
  state 3, the points scored, `16CE` for a frame. Checked: room 46 (a
  Rubber ball broken, 228 states; a Glass one scoring, 170), frames
  matched given `[FFE]`'s phase.
- **Type 0, a loose ball** (`f07_0000`): a Rubber ball (type record
  `28B8`) of radius `a`, mass 10, no shadow, on the ground at (x, y); its
  tick the ball's (`f07_077e`: rolling, breaking), hidden once broken (soft
  then). Drawn as the player's ball is (`1300` rolling, the breaking
  frames). Checked: room 71 (a rack of six, 236 states of all seven
  bodies; frames within 6 pixels, a grid line of the table's).
- **Type 2, a magnetic ball** (`f61_09bd` → `f05_11c1`, the type-3
  ball's class without the player's parts; no room has one): `a` its
  radius, resting on the ground at (x, y); its motion part type 0's
  (`f07_0000`: no shadow, its tick `f07_077e`, its draw `f13_01ce`), its
  magnetic part at `+2` (strength 200 * `[27B2]` / (`[27B0]` * 2), as the
  type-3 ball's), its type Iron (`f07_05bf` with `28DE`: mass 20, Iron's
  frames), its acceleration the field's (`f05_0c0b`); it takes no mouse
  (`f05_08f3`). Checked: room 13 with one added (`OBJ11 420 120 2 10`, in
  copies of the game's folders), pulled among the three loose magnets,
  1124 states of all five bodies traced.
- **Type 16, a block** (`f07_17f2`): a cube of side 2`a` on the ground at
  (x, y) (its core's type record Rubber), `b` its picture at the
  rectangle's corner, `c` draggable, `d` stepped as a body (`f08_1a42`),
  `e` its mass (0: 1, soft). Checked: room 96 (the block met at tick 8,
  and the shot on: 152 states of the ball, into the pit).
- **Type 14, the hot field** (`f02_1234`; rooms 5, 7, 29, 32, 59, 98: a
  lava puddle that grows on the table): its sphere at `+2` (at (x, y) on
  the ground there, radius `a`), a core of a 6 cube at `+18` (mass 0: out
  of the step's reach; its draw `f13_0000` only a debugging outline, with
  `[12E6]`), its colour cycle `f32_0e7f(4D, 4F, 10)`. Its hot spots, a
  list at `+0C` (`+12` the count; 10 bytes each: centre, `+6` the full
  radius, `+8` the radius so far): the first (`f02_110b`) at its centre,
  full at `a` / 2 + rand() * (`a` - `a` / 2) / 8000h. `+16` growing. Its
  tick (`f02_1627`): growing, every 6 room ticks (`[FFE]`) `f02_1481`:
  each spot in turn (new ones in the same pass) not yet full one bigger
  and marked (`f13_16f9`); then, with at most 4 spots and its radius over
  5, at rand() * r / 8000h above r * 16 / 20 a new spot (`f02_0e4a`: a
  point a half to a whole radius off its centre each way (rand() for x,
  y, then the signs), at the field's height; inside the field, full at
  three quarters to all of what's left of the field's radius there, else
  0); all full, it stops. Then the player's ball on the ground (`+5E`)
  and not breaking: its foot (centre less radius) within the field and
  within a spot's radius so far (`f11_1732`, radius 1 each) is heated with
  2000 (`f07_030e`): any type breaks, sound 6028, Ice melts. The mark
  (`f13_16f9`, radius 2 up): `140D` (68 x 17) at the spot's centre
  projected, at max(r, 6) / 26, on screen 3 and the display; `f14_0d69`
  draws it only where it lies wholly on the screen at 1:1, and at 1:1 when
  the scale's whole part is 1 (radii 26-51), else `f72_02cd` →
  `f73_0324` (32-bit code: `(w * scale) >> 8` wide about the point, each
  pixel the source's at a 16.16 step of 256 / scale). Checked: room 5's
  growth (spots and rand() seed at every step, from the original's seed)
  and its puddle pixel for pixel (but where the original drew it on the
  display over the N magnet, which its changed-rectangle redraw leaves);
  room 32 (the ball burnt at the same room tick, 207 states traced).
  Rooms 5 and 32, entered with the S key, make 3 rand() calls between the
  field's and its first growth; the port's `--room` start makes 8 (the
  panel and columns), so `SCI_RANDSEED` takes a second seed.
- **Redrawing** (`f29_0380` at the room's tick's end): the changed areas
  (`f27_16ae`, an object's `+10`: its old and new rectangles queued, 32 at
  most) are redrawn every `+180` ticks: each room's builder sets 2, but the
  room's tick sets it from `[27E6]` (1, nothing changes it) whenever that's
  not 0, so every tick.
  - `f27_16ae` (from `f08_07a7`, the core's `+10`, and hiding,
    `f08_0469`): the rectangle now (`f25_0a51` from the core's box, empty
    when hidden) against the one kept at the core's `+0`: meeting
    (`f11_0a26`), their union (`f11_08b7`) queued, else the new then the
    old; the new kept. Classes whose `+48` is 6 (types 4 and 5) or 8 (type
    2) and that are shown, with the same rectangle, queue nothing. A core
    gone (`f27_160c`) queues its rectangle; so do the glass's marks
    (`f27_0772`, `f27_2434`) and the rooms' pictures on screen 3 (rooms
    12, 34, 54).
  - `f29_0313` queues (a full queue is redrawn first); `f29_0380` takes
    the last off, merges it into the nearest earlier one it meets, else
    redraws it (`f29_0494` → the room's method 3); till none are left.
  - In the method 3 (`f35_04ca`), an object is drawn only if shown and its
    rectangle meets the area, and a box only once an object has been
    (`[2982]`) and if its rectangle meets the area; the target's front
    (`[1508]`) only if its back was.
  - Ported (`noteChanges`, `queueArea`, `redrawAreas`): at each tick's end,
    each drawable whose rectangle or look (the sprites it would draw)
    changed is queued as `f27_16ae` would; one gone, its rectangle.
- **Each room's own methods** (its `+5E` table against the base room's:
  every room has its pictures `+10`, its word on its holes `+20` and `+2C`;
  `+14` its tick, in rooms 2, 9, 12, 18, 34, 54 (all ported); `+24` a body onto a face,
  in 23 rooms; and some builders make objects of their own: rooms 2, 5,
  10, 12, 13, 18, 37, 40, 42, 45, 52-55, 92).
- **`+24`, a body onto a face** (`f27_2657`, nothing, in the base room):
  the body step (`f08_1a42`) calls it with the body and the face when it
  lands or rolls onto another face (and the core's `+38`, `f08_1236`).
  The rooms' (ported: `rooms.cpp`, `roomFaceMet`), "on (x, y)" meaning the
  face's box is the deepest box under that point (`f12_4284`):
  - the player's ball breaks (`+7C`) on (430, 0) in room 11 (`f43_0136`),
    and the same code with its own point in rooms 14 (306, 160), 17 (715,
    625), 33 (405, 278: its water), 36 (301, 360), 38 (700, 300), 41 (481,
    417), 49 (326, 301), 50 (405, 278), 51 (501, 47), 66 (487, 160), 92
    (512, 245); two points in rooms 37 (650, 210; 346, 153) and 74 (369,
    140; 608, 112), four in room 5 (730, 150; 354, 0; 424, 35; 494, 0),
    one before its goal in room 91 (439, 123). Room 8's is empty.
  - Room 91's goal (`f59_014a`): the ball on (524, 208): as its hole's
    method 8, a bonus ball, 2000, the shares, event 9 to room 14.
  - The bins of rooms 60 (`f52_1046`), 64 (`f53_0602`) and 69
    (`f54_07fa`): any body clears `+F7F`; the ball on a bin, its points
    there (60: 200-800 at x 684; 64: 100-700; 69: 200, 400, 600, 200 at x
    293); points set, event 9 to 64, 69, or (69) 508 with `+F71`. Event 9
    joins the queue behind the next tick's event 4, so the room goes on a
    tick (in the port, `exitNextTick_`).
  - Room 96's pit (`f60_0216`): a body but the ball on (315, 160) counted
    (`+FC0`) and put at 0, 0 (`f08_056e`: stopped first, its `+40`,
    `f08_0721`: velocity, remainders, kick); four in, the hole to 66
    opened (`f28_153f`: `+2F` clear, redrawn).
  - Room 2's circuit (`f41_0b78`): the player's Iron ball on the top (the
    face's `+2` 1) of the box under (400, 325): `+FC0` on (the first time,
    `+FC2`, colour cycle 4A-4C every 6 ticks); else off (`f32_0f77`).
  - Room 10 (`f42_0f2a`): while `+FC0`, an Iron body on (420, 248) past
    x 392: `+FC0` off, the cycle at 4A stopped, the blocks `+FB4` and
    `+FB6` (its gate) put at 0, 0, sound 601A. (Never in play: its file
    locks the ball to Stone, `PANEL ... 1 1`, and its loose balls are
    Rubber; so not traced.) Room 55 (`f51_1777`): on (295, 388) or (636, 386),
    its magnetic ball `+FB4` sets `+FA2`, `+FB6` `+FA4` (both: its hole to
    3 lets the ball through), the player's ball breaks, any other body is
    put at 0, 0; its restart (the hole to 101) puts the two balls back at
    (403, 223) and (550, 248). Checked: an Iron ball knocking `+FB6` into
    its pit (`+FA4` set), all seven bodies (the ball, the four loose
    magnets, the two magnetic balls) traced, 705 states.
  Checked: room 33 (the ball broken in the water), 96 (a block into the
  pit, all nine bodies, 317 states; the ball itself into the pit, z -390,
  329 states), 69 (lever OBJ4 on: the ball carried into the 200 bin, 158
  states, then lesson 508), 60 (its first lever: down the pit's slope
  into a bin, 121 states), 64 (its second: 118 states, into room 69),
  91 (at full power into the goal, 51 states, and its bonus box pixel for
  pixel), the breaking boxes of rooms 36, 38, 49 and 66.
- **Box looks** (`+3C`, `f12_0872`; ported: `Box::looks`): a builder's
  list of five records (`f12_0000`, faces 1 top, 2 left, 3 right, 4 back,
  5 front; `DS:926C` the default) of `f34_0000` (`+0` a picture or colour,
  `+2` 0: drawn as is in a redraw, else cut as a default face; `+4` 0: a
  picture with its corner at `+6`, `+8`, else a solid colour). The box
  drawing (`f12_2a09`) draws a look's picture (`f14_1179`) and no grid on
  that face; the cut (`f12_220d`, faces 4, 2, 1, the bottom, 5, 3) draws it
  (`f14_12e9`, clipped to the area) instead of cutting. Every builder's
  look is a picture: `117B` (one clear pixel: the face left to the room's
  own pictures) or one at a point (rooms 18 `10DD`, 42 `10A8`, 45
  `10AE`-`10B0`, 52 `10CE`, 55 `10CF`/`10D0`, 92 `10E1`). Read from each
  builder's helper with `tools/testing/calltrace.py`; the rooms: 5, 10,
  13 (its glass wall), 18, 37, 40, 42, 45, 52, 53, 55, 92. Checked at rest
  against the original: rooms 45 (pixel for pixel), 92 (one pixel), 13.
- **The builders' own objects** (ported: `roomObjects`): room 10's hole
  to 12 at (627, 330), its gate of two blocks (`115E`, 17, mass 60, at
  (607, 309), (636, 309); `+FB4`, `+FB6`) and three loose Rubber balls,
  `+FC0` on; room 18's loose magnet (10, N) at (478, 252) (`+FB8`); room
  55's two magnetic balls (type 2's) at (403, 223), (550, 248); room 12's
  magnet (`f43_0386`: `f05_26f7`, type 6 with the file's OBJ1's
  arguments 13, -1, 1, 10, at (362, 329), its power part `+FA8`, off);
  room 54's (`f51_09ce`): a bullseye (`f04_0502`: `f04_03eb` with a 0, b
  -1, its method 3 `f04_059d` only setting it: no power, no sound) at
  (363, 400) (`+FA8`), two magnets that don't move (`f05_210d`, 13, S
  `+FB8` and N `+FBA`) at (506, 400), S put 80 up, N on the ground at
  (505, 401). `f08_056e` with a height (its fourth argument 1): the
  sphere's centre its radius above it, no sound; without, on the ground
  there (sound 6026 for a move of more than 15); `+5E` set either way.
- **Room 12's tick** (`f43_043d`): on 5 game ticks in 6 (`[27B4]` not a
  multiple of 6), `+FC0` set while switches 1-4 (`f27_0a5a`: the first
  object whose `+48` says 11, a switch, with that `+2`, its last
  argument) are all on. All on and the magnet off: on (sound 6027),
  `+FC2`, colour cycle 4A-4C, the lights `10D9` on screen 3 at (371, 108)
  and (470, 108); else, the magnet on: off, and with `+FC2` (cleared)
  the cycle stopped and `10D8` there. Checked against the original pixel
  for pixel: at rest, the four levers on, one back off.
- **The order kept** (`+5FD`, a byte a pair: "drawn after"): the room's
  method 3 (`f27_1e36`) makes it again (`f27_1af3`, every pair through
  `f27_19d9` → `f35_0744`) only when `+F1D` is set: a core made
  (`f08_0066` → `f27_1518`, which adds its drawable, `f27_18b7`, kind 0;
  the table's boxes kind 1, `f27_1864`) or gone (`f27_1409`, `f27_160c`:
  the last drawable moved into its place). Else, with `+F1B` (something
  changed), `f27_1bd9` compares again only the pairs where one is marked
  (`+1AF`), keeping the others' order. `f27_16ae` (an object moved or
  hidden: `f08_0469` empties its rectangle) sets `+F1B`, and marks it
  only when it's a table box or its core's `+7E` is set: 1 from
  `f08_0066`, 0 from types 5 (`f05_210d`), 15 and 6 (`f05_233e`) and 12
  (`f02_0000`), which aren't meant to move. So room 54's two magnets (S
  let down onto N by its tick) keep the order of when the table was made,
  none (S up there, their rectangles apart): the list's, N (made after)
  in front. Ported (`drawObjects`, `drawOrder_`): the table kept while
  the drawables are the same ones, a pair compared again when one that
  marks itself moved.
- **Room 54's tick** (`f51_0dc2`): while the bullseye is on, every third
  game tick: going down (`+FC2` clear) `+FC0` one more, past 29 `+FC4` and
  `+FC2` set and the bullseye off; with `+FC4` the N magnet at (505, 401)
  16 under the S one; going up `+FC0` one less, below 1 `+FC2` clear and
  the bullseye off, the N magnet 16 under the S one. Then `10B3` + (`+FC0`
  odd) on screen 3 at (422, 38) and the S magnet at (506, 400), 80 - 2
  `+FC0` up. Checked: the bullseye hit at power 16, the way down (48
  states of the counters and both magnets); a second hit from where the
  ball stopped, the way up (93 states, the ball's 286); at the bottom N
  drawn in front of S (the order kept, above), as in the original (the
  picture the same but for the other objects' animation frames).
- **The keys** (`f31_1d70`, the player's method 3, event 8; ported:
  `keyEvent`): with a room (`+AE`) and nothing holding the keys (`+B0`),
  `p` pauses (face 1, style 1, `609` "What are we waiting for?",
  narration `6181`, 120 ticks, the music off till OK), `q` asks to quit
  (face 0, `20B`, buttons `202` NO / YES, sound `6016`; YES: event 3),
  `m` is an easter egg (face 2, `613` "Hey wait! You can't go through
  here! Press NO!", narration `618B`; YES: `614`, `618C`; YES: face 0,
  `615`, sound `6018`, event 9 to room 24). Any time: `s` turns the
  sounds over (`[27F6]`: off, the WAV stopped and the music off, music 25
  back on), `S` and two digits goes to that room (1 outside 1-110). Other
  keys go to the holder (`+B0`), else the room (its `+1C`), else the panel
  (`+A4`, `+A8`): in play, the room's. Every room's `+1C` is the base's
  `f27_31fb`: `r`, with `+F85` (always 1, `f27_03da`), the room not busy
  (`+F6F`), the ball not breaking (`+7C`) and neither column waiting for
  its PUSH (`+132`), takes the ball back: slid (`f27_293b`, as PUSH's
  drop) from its centre (`f07_04c1`, taken as its bottom) to the room's
  place, then 300 points off (`f06_0208`); other keys nothing. Checked
  against the original in room 1 (a shot, then `r`): the same but for the
  shadow under the ball at the end, which the original leaves out (29
  pixels; as after a wrong warp code, below). Checked: the three boxes against the original's
  (but for the left column's random ball frames and the `m` box's random
  picture).
- **The high scores** (room 502, `f40_0000`; ported: `highScores`,
  `recordGame`, `loadPlayers`): the display cleared, `200A` on screen 2,
  `wscience.hs` loaded (`f19_115a`, `f21_0041`: up to 50 records of four
  strings and four numbers, the score `atol`'d; each appended, then the
  list's method `+4` (`f39_1940`: it swaps the first and the last), then
  the quicksort `f39_1183` / `f39_129d` through a sorter (`DS:1272`) with
  the list's compare `f19_1061` (0 less, 1 the same, 2 more) and swap: no
  sort if all are the same, else the pivot the first unless the first
  different one after it is less, the partition putting the more ones
  first; ties land as it goes, so the order of equal scores changes); a
  read past the end (before 50) makes one more record, empty, score 0,
  and the list's sorted once more. Ten rows from (100, 103), 25 apart:
  name (dots as spaces), score (12 wide, right-aligned, a comma every
  three digits) at +146, level at +316, screen at +424, in colour 0 two
  right and down then in F; the first row with the player's name and the
  room's score (`+F35`) in 1. Sound `6013` (with the sounds on), the
  screen shown; a key or a button ends it (every 30000 polls colours
  A0-BF turn a step, `f14_0148`, on a 256-colour display: not ported);
  the list's destructor saves it (`f21_0447`, the empty record too, as a
  broken line the next load misreads: the port leaves it out). From room
  1 (its hole to 502, `f31_0783` with 0) then event 9 to room 1.
  The game over (`f31_0504`, the last ball lost; `f31_06c0`):
  `f40_068b` loads the list, adds the game (the room's score; the level
  and screen from `DS:26F6` by the room, high and low nibbles, room 65's
  once the game's won, `[26CC]`; the name, `DS:26D8`, spaces as dots; the
  look, `f21_0000`), sorts and saves it; the room `+8E` -1, event 9 to
  502, then event 2 (`f31_1b45` with 1: a new game, `[8E50]`-`[8E56]`,
  `[26CC]`, the score cleared, the balls 7 and 0, event 9 to room 1).
  Checked against the original pixel for pixel: the screen from room 1's
  hole (its tie order and the file it saves); the game over (room 32: all
  eight balls, one on the table and seven in the column, shot into its
  lava; `Player 0 4 4` recorded, the screen the same); the game won (room
  65 at power 16 into its hole to 510, its bonus 3000, lesson 10 to its
  end: `Player 3000 5 15` recorded, room 65's level and screen, the
  screen the same). The saved files the same but for the empty record
  the original appends.
- **The credits** (room 503, `f38_0eb9`; ported: `credits`): the display
  cleared, `2009` with the player's look on screen 2 (`f14_092c`,
  `f19_06bc`), sound `6013`, shown till a key or a button, then event 9
  to room 1. Checked against the original pixel for pixel.
- **Colour cycles** (segment 32; ported: `cycleStart`, `cycleStop`,
  `cycleStep`, `roomCycles`): a list (`[921C]`, `[921E]`) of 6-byte
  records (`f32_00e3`: the first colour, the count, the period, the uses).
  `f32_0e7f(first, last, period)` adds a use to the one with that first
  colour (and gives it the new period), else adds one; `f32_0f77(first)`
  takes a use away, none left it goes. Each game tick (event 4,
  `f32_11a5` → `f32_1113`) every cycle whose period divides `[27B4]` turns
  a step (`f14_0148`: each colour takes the next one's, the last the
  first's, into WinG's colour table). All of it only on a 256-colour
  display (`[61F9]`): not under winevdm on a true-colour desktop, so it
  can't be compared there (the port's `SCI_TRUECOLOR=1` turns it off, as
  the tests do). Who starts them: every magnetic part (`f05_0000`: types
  2-6 and 15, the type 3 ball) 90-97 and 98-9F every 3 ticks; a type 6
  magnet (`f05_26f7`) 4A-4C every 4; a pulling hole (`f28_15a1`) A0-A6
  every 3; a hot field (`f02_1234`) 4D-4F every 10; their destructors stop
  them. The rooms' constructors (after their builders): 4D-4F every 6 in
  rooms 5, 14, 15, 17, 20, 28, 30, 37, 38, 46, 47, 51, 55, 57 (its EXIT
  sign), 63, 66, 72, 74, 91, 92, 96, 99; 4A-4C (6) in 10 and 16, (4) in 92;
  B0-B6 (5) in 20-23, 26, 29, 31, 34, 37, (6) 14, (3) 30, (4) 77, 99; C2-C7
  (5) in 4, 30, (3) 33, 36, 49, 50, 97; A0-A6 (3) in 99. Room 12's builder
  stops its two type 6 magnets' 4A, its destructor (`f43_0314`) adds two
  back for their destructors. In the rooms' code: room 2's circuit, 10's
  gate, 12's and 18's ticks (above); room 9's tick 4A-4C (6) while switch
  99 is on (`f42_0805`).
- **Room 18** (`f44_0846`, its builder `f44_0a97`): the hole to 60 shut
  and hidden; a gate, a small hole to 1000 on the ground at (668, 262)
  over it (`f28_00f3`: b 1, c 0, d -1; `+FBC`), shut. Its tick
  (`f44_11a7`): the loose magnet (`+FB8`) falling (`+66` below 0) with its
  box's centre below 62 and within (397-427, 232-264), before (`+FC0`,
  `+FC4` clear): `+FC0`, `+FC4` set, `+FC2` 0, colour cycle 4D-4F, sound
  6029. While `+FC0`, `+FC2` up to 50: the gate to (677, 278) at that
  height (`f08_056e`; its box, as room 2's, (666, 267, `+FC2` - 6)), the
  hole to 60 shut and hidden; at 50 opened and shown, `+FC0` clear. Its
  hole to 100 (`f44_0f60`) without `+FC4`, "try again": switch 1001 (its
  RETRY) on, its method 3 (`f04_070b`: the room again). Checked: the
  magnet knocked into its slot (the ball and the magnet, 124 states; the
  counters and the gate, 59), the screen at the end.
- **Room 34's tick** (`f47_0cbf`): at `[27B4]` mod 64 0 and 31, `1092` /
  `1093` on screen 3 at (392, 20): its sign blinking. Both pictures match
  the original's.
- **Room 9's tick** (`f42_0805`): on 15 game ticks in 16, the colour cycle
  4A-4C every 6 ticks while switch 99 is on, stopped when off (without
  switch 99 it would skip the base tick; the room has it). Nothing shows
  under winevdm; the port has no room colour cycles.
- **Room 12's constructor** (`f43_022d`): `f32_101b(4A)` / `f39_0214` only
  log "color cycle still set" (a debugging check); `+F7B` 0.
- **Room 35's constructor** (`f47_0d46`): from 43, the ball spat out of its
  hole to 1000 (mode 2: put back where the room put it), the hole to 43
  shut, 2 shots, `+FC0` set; else `+FC0` clear, the hole to 1000 shut and
  its two boxes (3, `5E8`). `f71_00ce` clears a run-time table
  (`DS:9558`-`9570`). Not traced (the original from 43).
- **Room 2** (`f41_076c`, segment 41): the gate, a small hole to 1000 at
  (606, 329) on the back wall (`f28_00f3`: b 1, c 0, d -1; `+FBA`), shut;
  the room file's hole to 1001 (`+FBC`) shut and hidden (`f08_0469`). Its
  tick (`f41_0c21`, then the base `f27_2434`): with `+FC0`, every third
  game tick (`[27B4]`, event 4's count, the colour cycles' too): at 24
  the hole to 1001, still shut, opened (sound 6029, shown, `f28_153f`);
  below 40 the gate (`+FBE`) one higher (`f08_056e` with a height: its
  sphere at (616, 345, its radius + `+FBE`), its box after it,
  `f07_11c5`; read in the original's memory: a 23 cube at (605, 334,
  `+FBE` - 6)). Checked: an Iron ball onto the circuit (111 states; on,
  off and on again at the original's ticks), the hole opened at its room
  tick with `[27B4]` two ahead of `[FFE]`, the gate raised pixel for
  pixel.

## The rooms (`S<n>.SRF`)

A room reads `S<n>.SRF` (`f27_0ad8`: the name built at `DS:1FF4`, read
through the run time's streams in segment 90). Text, in three parts:

1. **The shape**: a tree of boxes (`f27_0d4a`, recursive). A box is two
   rectangles `x y w h` (its top, at the box's height, then its bottom, at
   the parent's: `f27_0d4a` hands the second read over as the bottom; so
   sides can slope) and a
   height; then the byte 01 and a child box, as many as it has, and the
   byte 02. Each
   box is relative to its parent (its x, y and height are taken off) and is
   made by the room's method 1. Each one added, its parent's children are
   sorted (`f12_3e0d`: segment 39's quicksort, as the high scores', with
   `f12_029c` → `f25_027a` on the boxes' extents, `+18`, set by
   `f12_093b`: the bottom from the lower of the two heights, the
   difference high), the "more" ones first: an empty extent (a box at its
   parent's height) before the rest; else, apart along y, the further back;
   along x, the further left; along z, the lower; else the larger y. So
   the tree's order (what's drawn first, `f12_38ad`, and the face under a
   point) isn't the file's: in room 35 the block at x 419-538 comes before
   the one at 539, which hides its right side. Checked against the
   original at rest: rooms 35, 39 and 63 then match but for the columns'
   random balls (40's walls too; its lips and targets are a frame apart).
   The root is the floor, `0 0 809 789` (the
   world is 809 x 789). `S1.SRF` (the menu) is the floor, two walls 400 high
   and a pit 50 deep whose floor (660, 60, 149, 149) is smaller than its
   mouth (570, 0, 239, 209): the ramp at the front and left down to EXIT.
2. **The objects**, `OBJn x y type a b c d e f` (segment 61, `f61_011d`
   and `f61_09bd`; rooms 0, 100 and 101 give only x and y), at most 24:

   | Type | Made by | Objects | Notes |
   |---|---|---|---|
   | 0 | `f07_0000` | 9 | a loose Rubber ball, `a` its radius (draggable in room 0 only) |
   | 1 | `f06_0043` (+ `f07_0456`) | 37 | the ball (only one: "can't init more than one player") |
   | 2 | `f05_11c1` | 0 | a magnetic ball: type 0's, Iron, with the type-3 ball's magnetic part (see above) |
   | 3 | `f06_0348` (`f61_09bd`) | 50 | the ball with a magnetic part (`f05_11c1` at its `+6`) |
   | 4, 5 | `f05_1749`, `f05_210d` | 11, 21 | magnets: `a` the strength (`f05_1e60`: 0-4 by 7, 9, 12, 16) |
   | 6 | `f05_26f7` | 46 | type 15 on a switch's power (`+F94`) |
   | 7 | `f04_01a1`, `03eb`, `0835`, `05b8` (by `d`) | 69 | switches; `d` 3 RETRY (see below) |
   | 8 | `f28_00f3` / `f28_0391` | 263 | a hole: `a` is the room it leads to (in `S1`: 502 high scores, 501 lab, 503 credits, 31, 21, 508, 509, 504 EXIT); `e` not 0: the second of a door |
   | 9 | `f28_15a1` | 4 | a pulling hole: `b` its wall, `c` big (see above) |
   | 10 | `f03_002c`, kind 6 `f02_0be1` | 267 | point targets (`c` the kind); kind 6 a suckhole |
   | 11 | `f03_0865` | 9 | the smiley: a kind 0 target, `a` its points (up to 10000) |
   | 12, 13 | `f02_00c2`, `f02_05f2` | 2, 9 | power (`+F94`): an electromagnet that catches Iron and breaks other balls, a fan (`a` its way) that breaks Ice, Rubber, Glass and Magic ones (see above) |
   | 14 | `f02_1234` | 6 | the hot field: a lava puddle growing from `a` (see above) |
   | 15 | `f05_233e` | 53 | a magnet on a wall |
   | 16 | `f07_17f2` | 9 | a block: `a` half its side, `b` its picture, `c` draggable, `d` stepped, `e` its mass |

   Types 2-6 and 15 are made by `f61_09bd` (which hands the rest to
   `f61_011d`), the magnets' rooms (segment 26's class, `f26_0000`: a
   list of the field's sources at `+F98`-`+F9E`; `f26_01e6` the field at
   a point, each source's `+20` summed).

3. **`PANEL a b c d e f g h END`** (`f61_0000`): eight numbers for the
   controls under the table (85 different ones over 108 rooms;
   `0 0 0 0 0 0 0 0` in 21).

## The Artech library (segments 63-83)

- **Polygons**: `draw_poly` (`f63_1fbf` → `f81_0280`) and the stretched
  bitmap fill share the clip (`f83_0065`) and the spans (`f81_0000`, the
  library line's steps along each edge): a solid polygon covers exactly
  what a textured one with the same corners does (room 2's pit's mouth,
  cut to colour 0 on screen 3, is filled again by its sides on screen 2;
  a generic scan conversion leaves a line of colour 0 along it).


The same library as WINMAIN's (see `ROCKBACH.md`), laid out differently:
the screens are 5-word records at `DS:92CE` (`[92CC]` the display's
number, `+6` a type, 1 the display and 2 a RAM screen, `+8` the WinG
bitmap's record), the palettes 3 bytes an entry at `seg100:0050 + 300n`
(B, G, R as WINMAIN's), the display's colour table at `seg92:0400`. In
Ghidra's names segment 63 is `11f0`, 64 `11f8`, 67 `1210`, 69 `1220`,
70 `1228`, 72 `1238`, 74 `1248`, 75 `1250`, 76 `1258`, 77 `1260`, 80
`1278`, 81 `1280`, 83 `1290`; DGROUP (103) is `1330`.

| Function | What | WINMAIN's |
|---|---|---|
| `f63_0033` / `f63_0066` | select / current screen | `f37_0032` / `f37_0064` |
| `f63_0081` | copy an area from screen to screen | `f37_007c` |
| `f63_0291` | the same leaving colour 0 out | `f37_0320` |
| `f63_0938` | fill a rectangle | `f28_0212` |
| `f63_09d7` | a full-screen picture onto a screen, with its palette | `f37_0d48` |
| `f63_0c0a` | copy a screen's palette to another | `f43_02da` |
| `f63_0e99` | copy an area to another place | |
| `f63_143b` / `f63_10cd` | save / restore an area (a handle) | `f28_05f8` / `f28_06d0` |
| `f63_1761` | the clip rectangle | |
| `f63_178e` | a pixel | |
| `f63_1cd5` / `f63_1e2c` | a line / a run | `f37_29e4` / `g37_2c4c` |
| `f63_1f69`, `1fbf`, `200f`, `20e4` | polygons: one colour, outline, pattern fill, bitmap fill (segment 81, clipped by 83) | `f37_2fae` |
| `f64_0000` | a bitmap by id (loaded and cached) | `f38_0000` |
| `f64_00ad` / `f64_025d` | draw a bitmap opaque / transparent | `f28_011a` / `f28_0000` |
| `f64_0415` / `f64_045a` | a bitmap's width / height | `f38_055a` / `f38_05a8` |
| `f67_*` | RAM screens: allocate, copy (opaque, transparent, destination-keyed), set | `f40_*` |
| `f69_*` | the data file: open (`f69_0000`), the cache, PK decompression, resources by tag | `f42_*` |
| `f70_0557` / `f70_06bf` | set / get the display's colours (first, count) | `f43_0694` / `f43_0846` |
| `f70_0791` / `f70_07c5` | a screen's palette out / in | |
| `f72_0000`, `02cd`, `0579` | a clip box; a bitmap scaled (by 256ths) and centred | `f28_030a` |
| `f74_0000` / `f74_00c0` | confine the cursor / the mouse messages | `f47_0000` / `g47_00aa` |
| `f75_*` | WAV output (`waveOut`, up to 120000 bytes) | |
| `f76_0000`, `0021`, `02e1`, `02fe` | text: height, draw, set font, width | `f49_*` |
| `f77_*` | the timer: countdowns (`DS:95F2`, 5), periodic callbacks (10), `UPDATE_ADLIB` | `f50_*` |
| `f66_*` | the display: WinG bitmaps, the system palette, MIDI drivers off in `system.ini` | |
| `f71_*` | keys held (a bit for each scan code) | |

Segment 14 wraps these: `f14_0af1` select, `f14_0c15` copy, `f14_0cc1` fill
(`{x, y, w, h}`), `f14_092c` a full screen with Edison's look put into its
palette (E1-ED, as Rock and Bach), `f14_0cf8` a bitmap's size,
`f14_126e` / `f14_08cc` / `f14_0877` clip boxes, `f14_1742` / `f14_191d`
text size / text, `f14_170d` the font, `f14_1953` / `f14_19b4` save /
restore an area, `f14_0000`-`f14_0384` palettes (fades, turning a range).

## Files

| File | Contents |
|---|---|
| `GRAFX.DAT` | The art and some sounds and data (the archive format of the other games, see `FORMATS.md`); extracted to `extracted/grafx/` |
| `\SCIENCE\*.WAV` | 153 sounds: narration, the professor's and Edison's lines |
| `S0.SRF`-`S110.SRF` | The rooms (above; `FORMATS.md`) |
| `WSCIENCE.HS` | The high scores (`FORMATS.md`) |
| `wscience.edi` | The player's look: four numbers (written by the game) |

## Running the original

`tools/reference/otvdm.ps1 start "WMAIN.EXE -A"` with the CD's `SCIENCE`
WAVs copied to `data\` in the run folder (winevdm has no CD drive).

To compare the arcade without the title, story, lab and lesson:
`python tools/reference/wmain_skip.py` writes `WMAINSKP.EXE` into the run
folder, a copy with the intro flag `[26CE]` cleared (it's 1 in DGROUP and
nothing writes it): `f31_0025` skips the story, `f32_0319` the title, and
the player's first event 9 goes to room 1 instead of 501. Then
`otvdm.ps1 start "WMAINSKP.EXE -A"`: room 1 at rest about 8 seconds later.
The port's equivalent is `--game science --room 1`.
