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
  (`2005`, no room: `[26CC]` = 1, then `f31_001a` and `31:1728`) four
  (anchors (530, 186), (216, 210) tail 1 width 150, (550, 146)).
- Two animated objects in each (added before the bubbles, so under them):
  the professor (`f15_0bde`: `13BB`, 3 frames over 50 ticks, at (500,
  130); lesson 9 `f15_0f19`: `13C7`, 6, at (14, 210); 10 `f15_0e43`:
  `13C1`, 6, at (540, 146)) and Edison (`13AF`, 3 over 100, at (96, 252);
  9: `13B8` at (364, 272); 10: `13B2`, 6, at (202, 218)). Their x, y and
  frame are generators (segment 16: `f16_0000` a constant, `f16_010e` a
  cycle, frame = (tick mod period) x count / period, `f16_0281` up and
  down), stepped each tick (`f15_01fa`, 50 a second); the sprite is the
  frame's base + its value. Clicking the professor swaps his animation
  (`g15_0cb7`).
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
  y). The table's boxes aren't in the port's list (it draws the table
  under every object; enough while no standing box hides one).
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
- **The swallow** (`+21`): a frame a tick, `[212C]` = 22 ticks; frame `+21 *
  5 / 22 + 1` (1-5), the sprite from `DS:20FC` (small) or `DS:2114` (big),
  6 a wall, frames 1-4 plus the ball's type's offset (`f28_0cd5`: 29
  sprites a type). Then the room's method 8 (`f28_156a`): room 1's
  (`f41_02a4`) asks for EXIT (504) and the passwords of levels 4 and 5
  (508 "electric", 509 "wildway"; wrong: spat out, mode 2); others go
  to `f27_2530`: spat out (mode 0) if it leads to this room, a door within
  the room (0, 100-500) passes it on, else event 9 to that room.
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

## The rooms (`S<n>.SRF`)

A room reads `S<n>.SRF` (`f27_0ad8`: the name built at `DS:1FF4`, read
through the run time's streams in segment 90). Text, in three parts:

1. **The shape**: a tree of boxes (`f27_0d4a`, recursive). A box is two
   rectangles `x y w h` (its top, at the box's height, then its bottom, at
   the parent's: `f27_0d4a` hands the second read over as the bottom; so
   sides can slope) and a
   height; then `01` and a child box, as many as it has, and `02`. Each
   box is relative to its parent (its x, y and height are taken off) and is
   made by the room's method 1. The root is the floor, `0 0 809 789` (the
   world is 809 x 789). `S1.SRF` (the menu) is the floor, two walls 400 high
   and a pit 50 deep whose floor (660, 60, 149, 149) is smaller than its
   mouth (570, 0, 239, 209): the ramp at the front and left down to EXIT.
2. **The objects**, `OBJn x y type a b c d e f` (segment 61, `f61_011d`
   and `f61_09bd`; rooms 0, 100 and 101 give only x and y), at most 24:

   | Type | Made by | In the rooms | Notes |
   |---|---|---|---|
   | 0 | `f07_0000` | 9 | |
   | 1 | `f06_0043` (+ `f07_0456`) | 37 | the ball (only one: "can't init more than one player") |
   | 2-6, 15 | `f05_11c1`, `f05_1749` (3, 4), `f05_210d`, `f05_26f7`, `f05_233e` | 50 (3), 11, 21, 46, 53 | |
   | 7 | `f04_01a1`, `03eb`, `0835`, `05b8` (by a sub-kind) | 69 | |
   | 8 | `f28_00f3` / `f28_0391` | 263 | a hole: `a` is the room it leads to (in `S1`: 502 high scores, 501 lab, 503 credits, 31, 21, 508, 509, 504 EXIT) |
   | 9, 10 | `f03_002c` / `f02_0be1` | 4, 267 | |
   | 11 | `f03_0865` | 9 | `a` up to 10000 |
   | 12, 13, 14 | `f02_00c2`, `f02_05f2`, `f02_1234` | 2, 9, 6 | |
   | 16 | `f07_17f2` | 9 | |
   | other | `f28_15a1` | | |

3. **`PANEL a b c d e f g h END`** (`f61_0000`): eight numbers for the
   controls under the table (85 different ones over 108 rooms;
   `0 0 0 0 0 0 0 0` in 21).

## The Artech library (segments 63-83)

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
| `S0.SRF`-`S110.SRF` | Rooms (playfields): ASCII rectangles in a world about 809 x 789 (four numbers twice and a depth, per piece), then named objects (`OBJ0 448 180`), then `END` |
| `WSCIENCE.HS` | The high scores |
| `wscience.edi` | The players (written by the game) |

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
