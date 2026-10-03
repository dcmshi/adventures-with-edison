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

## The rooms (`S<n>.SRF`)

A room reads `S<n>.SRF` (`f27_0ad8`: the name built at `DS:1FF4`, read
through the run time's streams in segment 90). Text, in three parts:

1. **The shape**: a tree of boxes (`f27_0d4a`, recursive). A box is two
   rectangles `x y w h` (its bottom and its top, so sides can slope) and a
   height; then `01` and a child box, as many as it has, and `02`. Each
   box is relative to its parent (its x, y and height are taken off) and is
   made by the room's method 1. The root is the floor, `0 0 809 789` (the
   world is 809 x 789). `S1.SRF` (the menu) is the floor, two walls 400 high
   and a pit 50 deep whose bottom (660, 60, 149, 149) is smaller than its
   top (570, 0, 239, 209).
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
