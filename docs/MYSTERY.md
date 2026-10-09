# Mystery at the Museums (MALL.EXE)

Map of the program for the port. Functions are `fSS_OOOO` in `tools/nedis.py` numbering. In Ghidra's output (`extracted/ghidra/mall.c`), segment SS is at selector `0x1000 + 8 * (SS - 1)`: for example seg 8 is `1038` and seg 31 is `10f0`.

Data is in `MYSTERY.D01`. Speech and effects are `\MYSTERY\<name>.wav` on the CD, played by name by `f06_2da8`. FM music comes from `MADLIB.DLL`. The shared Artech library occupies segments 31–60: the C runtime is 31, and graphics, disk I/O, the timer and MCI are 32–60. It's the same library as EDISON.EXE's.

## The game

- **Premise:** Treasures have gone missing all over the World Museum, and the Director opens the exhibition at 10:00 a.m.
- **The search:** Edison and Smitty search 13 museums for the 17 lost objects: silky shark, silver dollar, polar bear, and so on.
- **Puzzles:** each square of the museum map holds one of 16 puzzle games.
- **Levels:** the player picks a difficulty level, or builds a custom level by assigning puzzles to map squares.
- **Saved data:** players, a saved game, Edison's colours (`MEDISON.COL`) and high scores (`MYSTERY.HS`).

## Top level

- **`f01_0000` WinMain.**
  - `-O` sets `[7399]` and `-T` clears `[0054]`.
  - The idle function `f02_00ba` runs the game loop:
    ```
    mode = 1                                   # 1: first time
    loop:
        if setup(mode) == 1: break             # f08_232c
        r = play()                             # f09_1dd8
        if r == 1: break
        mode = r == 2 ? 0x0F : 0               # 0x0F: skip to the level pick
    stop music, WinExec("edison.exe -O")
    ```
  - Leaving the game returns to the launcher without the opening (the native `edison` does the same).

## Setup (`f08_232c`): a state machine

| Step | Function | What it is (from its strings) |
|---|---|---|
| 0 | `f08_06f8(0)` | scene with Edison talking; on a return visit `f08_21b6` |
| 1 | `f08_0380` | name entry: "Hi! I'm Edison. What's your name?" … "Is this right?" |
| 2 | `f08_01ee` | load the player's `.INF` (−1: new player) |
| 3 | `f08_14d2` | "Do you want to change my looks?" |
| 4 | `f08_06f8(1)` | scene |
| 5 | `f08_0f68` | Edison's look customizer (saved in `MEDISON.COL`, `f08_1102`/`f08_11ea`) |
| 6, 7 | `f08_06f8(2)`, `(3)` | scenes |
| 8 | `f08_1600` | "You have a saved game. You want to play that game?" |
| 9 | `f08_1a4e` | "You have a custom level." |
| 10 | `f08_1b6e` | "Your last game was at level …" |
| 11 | `f08_1d2a` | "Please pick a level" |
| 12 | `f08_157e` | ? |
| 13 | `f08_1264` | ? then done |

Menu events (`[91A4]`) jump between steps: 1/4 = show the high scores, 2 = `f08_1eca`, 3 = `f08_2092`.

## The game (`f09_1dd8`) and map (segments 9–11)

### `f09_1dd8`, the game loop

- **Starting a new game** (`[B76B]` = 0), by level `[B46E]`:
  - It sets the time limit `[9314]` (`0x708`, `0xE10` or more).
  - It picks a mask of which of the 29 map squares are used and a mask of which of the 16 objects are hidden. The square table is `DS:931C`, 4 bytes per square: the puzzle, `?`, the object there (`FF` if none) and whether it's active.
  - Each square's puzzle comes from a table in data segment 62 (selector `11E8`).
  - It hides every object at a random active square (`f46_001d` is random(n)). Objects are `DS:C500 + 3k`: the museum 0–17, the square, and found.
- **Each visit to the map:**
  - The screen: backdrop `1003` with Edison's colours, then the map (`f09_08dc`), which draws each object's museum icon `2295+m` and a check `22A7` on found ones.
  - The clock (`f09_0ab2`), the object list (`f09_15f8`) and six button panels (`DS:0D98`, `0E76`, `0CDE`, `0DC6`, `0CBA`, `0E9A`).
  - Edison says the intro (`DS:1346`, sound `4012`), or "Great, you found…" or "You didn't find an object" after a puzzle, while he talks (`f09_0dc4`).
  - A 1 Hz callback (`f09_0a7a`) counts `[9314]` down. At zero, `[C12E]` = 1 (out of time).
  - The loop polls the panels. Entering a square sets `[0E74]`, which runs that square's puzzle (`f10_0708`) and then redraws the map. When no hidden object is left, `[C12E]` = 2 (won), and the end screens (segments 22–23) follow.
- **Helpers:**
  - `f09_0dc4(side, n)` animates Smitty or Edison talking on the map (`22B1`–`22B3`, `22B6`–`22B8`).
  - `f09_0f8a` is the scrolling list of museums (`DS:1124`).
  - `f09_0592` and `f09_0680` handle "quit this game?" and saving.

- `f09_15f8`: "Okay, here are the objects."
- `f11_*`: level select ("Please select difficulty level."), the custom-level editor ("Click on a game icon, then click on the map square…") and map confirmation.
- `f06_*`: shared game UI: the timer display (`%d:%02d`), "Game Paused! Please, Press Space Bar to Continue", "< Press mouse/any key >", and WAVs by name.
- `f23_*`: the end of a game: "Time taken", "Happy faces of Edison, Smitty, and …", win and lose messages.
- `f24_*`: high scores (`MYSTERY.HS`).

## Puzzles

`f10_0708` dispatches on the square's puzzle number (0-15) with the square's difficulty (0-7); names are at `DS:1A9C`. Several segments share helpers (the picture puzzles in 12-14, `stackup.c`/`colour.c` polygon code in 25-26).

| # | Puzzle | Entry | Segment size | Evidence |
|---|---|---|---|---|
| 0 | Folded Cube | `g29_10c0` | 0x14E6 | uses `g30_*` helpers |
| 1 | Liberty Planetarium | `g28_178e` | 0x1831 | |
| 2 | 3D Ball Sculpture | `g30_136c` | 0x188A | |
| 3 | Binary Lights | `g17_1256` | 0x12CB | "You got it!" |
| 4 | Question and Answer Period | `f19_16a2(level, 1)` | 0x1828 | "JEOPARDY: answers are not correct."; also the final quiz after a win (`f19_16a2(level, 0)`) |
| 5 | Dropping Squares | `g18_26da` | 0x273B | `column.c` |
| 6 | Codes | `g20_1474` | 0x1521 | |
| 7 | Concentration | `g15_1142` | 0x1A4B | "MATCH 2", "MATCH 3" |
| 8 | Circuit Analyzer | `g16_089c` | 0x0EE8 | |
| 9 | Stackup | `g25_1f6a` | 0x1FE4 | `stackup.c` |
| 10 | Slide Puzzle (With 1 Blank) | `g14_028a` | 0x06CA | picture-puzzle code |
| 11 | Color Transformation | `g27_1032` | 0x1A7C | |
| 12 | Switch Puzzle | `g13_0d44` | 0x10CC | "SPACE/TAB to view pics" |
| 13 | Arrow Puzzle (With Many Blanks) | `g12_1c60` | 0x2041 | picture-puzzle code |
| 14 | What Comes Next | `g26_19d8` | 0x1A5C | `colour.c` |
| 15 | The Dig | `g21_185c` | 0x1E17 | |

## Port plan

1. **Shared library:**
   - generalise `engine/src/shell` into a library used by all the games (screens, anims, scripts, timer);
   - add fonts (group 01), text drawing, buttons and the message boxes the games draw themselves;
   - play WAVs by name from the CD.
2. **Setup screens,** then the map and the game loop, with a placeholder for each puzzle (counted as won).
3. **Puzzles, one at a time:** each is self-contained and can be tested on its own.

## Port status (`engine/src/mystery`)

- **Library:** the shared Artech library in `engine/src/artech` now has fonts, the timer and countdowns, and a per-game FM driver.
- **UI helpers:** `ui.cpp` ports segment 6's helpers:
  - clamped drawing that goes through screen 3 when drawing on the display;
  - fills and text;
  - saved and restored areas;
  - the speech bubble (`f06_2494`);
  - UI colour matching (`f06_01f6`);
  - speech WAVs by id (`f06_2da8`).
- **Title:** `f08_2284`, shown the first time only: bitmap `100E`, music `29`, for 10 s or until a click or key, then sound `4064`.
- **Setup** (`setup.cpp`, `f08_232c`): the whole state machine runs:
  - the title, the courtyard scene (`f08_06f8`, with the lip-synced "Cool!"), the name prompt and the character changer;
  - returning players: `<first 8 letters>.INF` holds the record `DS:B465-B595` (0x131 bytes): name, last level, Edison's colours, a custom level (`B473`, its level `B4E7`, FF for none) and a saved game (`B4E8` squares, `B55C` objects, `B58C` level, FF for none, `B58D` score, `B591`/`B593` time left and total, `B595` custom flag);
  - the prompts: "Do you want to change my looks?", "You have a saved game" (`g08_1600`), "You have a custom level" (`g08_1a4e`: play, edit or not), "Your last game was at level N" (`g08_1b6e`), and "Please pick a level" (`g08_1d2a`), whose ninth button is "Make custom board" (not the high scores);
  - after a game, "Do you want to play again?" (`g08_21b6`; no leaves);
  - `MEDISON.COL` keeps Edison's last colours (4 bytes); the port writes it but it only matters before a player is known.
  - Quitting from the map ("I'll save this game.") saves the game (`f08_1f6e`).
  - Files go in `--save DIR` (default `save`), as the original wrote them to its own folder.
- **Findings:**
  - The UI colour matcher reads its target colours (`DS:00BE`) as signed chars. Components above 127 therefore count as negative, and the "white" entries match dark palette colours. The port keeps this.
  - Speech: id - 0x4010 indexes the name table at `DS:0566`. The file is `<CD>\MYSTERY\<name>.wav`.
  - Flags: `-A` turns music off (`[0054]`) and `-T` sets `[7399]`.
- **Checked against the original under winevdm** (`tools/reference/otvdm.ps1`): the title screen and the courtyard scene have the same frames, colours (pixel-identical) and timing. The original next shows "Hi! I'm Edison. What's your name?" in a speech bubble with a `>` prompt: that's setup step 1.
- **Game loop and map** (`game.cpp`, `floor.cpp`): `f09_1dd8` and the map screen are ported: board generation per level (the level tables are immediates in `f09_1dd8`; the puzzle list per level is segment 62), the office with the clock (hands from the segment-51 sine table), the timer, the object grid, Edison and Smitty's entrance and talk, the Director's letter, the help boxes (text resources `3F06`/`3F07`), quitting, the Museum floor (`f10_0708`: squares are told apart by colour `A1 + n` on screen 2, painted from the path polygons at `DS:164A` and the room masks `22C4+`), and the found / not-found speeches.
- **Puzzles ported:** the picture puzzles (`pictures.cpp`): Slide (10), Switch (12) and Arrow (13), with their shared code in segment 12 (pictures `22F0+i` scaled into screen 2 with palette resources `205, 204, 206�`, entries 16�79; the timer and score, 4 points a second left plus a 100-point bonus and 2 per second on solving; the view, gadget and exit-lever buttons) and the common end of a game `f06_1d76` (Edison's verdict, "Great, you solved it!" / "Maybe next time!"). `edison --game mystery --puzzle K --level N` plays one puzzle over and over.
- **All 16 puzzles are ported**, one file each (or so): `pictures.cpp` (Slide, Switch, Arrow), `small.cpp` (Circuit Analyzer, Binary Lights, Codes, Concentration), `quiz.cpp` (Question and Answer Period, also the final quiz), `dig.cpp` (The Dig), `sequence.cpp` (What Comes Next), `colour.cpp` (Color Transformation), `stackup.cpp` (Stackup), `drop.cpp` (Dropping Squares), `planetarium.cpp` (Liberty Planetarium), `cube.cpp` (Folded Cube) and `ball.cpp` (3D Ball Sculpture).
- **3D helpers** (`three.cpp`, segments 47-51, used by 28-30): sine and cosine from the segment-51 quarter table (Q15, a turn is 0x10000), a rotation matrix from three angles (`g50_0000`), matrix times points (`f48_0000`, sums wrapping at 32 bits), a translate (`g49_0000`), and a perspective projection (`f47_04de`: y is the depth, the eye at half the view's larger side; polygons are cut at the eye's plane, then at the view rectangle).
- **Puzzle findings:**
  - The facts learned (`DS:B7CE`) have five rows of ten: rows 1-3 are Concentration's themes, row 4 the Planetarium's (set on a *wrong* answer, `DS:5050`). The quiz asks from them, else questions 0, 1, 2...
  - Screen N's palette lives at segment 72, `0x80 + N * 0x300` (B,G,R). The Folded Cube loads resource `218` into screen 2's colours `0x20-0x2B`, the Ball Sculpture resource `219` into `0xB8-0xD3`.
  - The timer's rate is calls per second (1 about a second, 10 the countdown rate).
  - The Planetarium swaps each star's x and y the first time it runs and gives it a depth of 10-14 either side (`g28_1218`, once per run).
  - The Ball Sculpture reads its time limit before it stores the new level, so the limit is the previous game's.
  - `f06_189a` and `f06_19fa` draw about the sprite's centre.
- **The end of a game** (`end.cpp`, segments 22-24):
  - Out of time: the sad ending (`f23_0a6a`, "We lost!"), then the high scores.
  - All objects found: Edison and Smitty dance (`f23_10aa`, "Hey!"), then the final quiz (`f19_16a2(level, 0)`). Passing it plays the bonus maze and the happy ending (`f23_0000`: "Take a look at this!", then the newspaper "Happy faces of Edison, Smitty, and <name>") and enters the high scores; failing it plays the sad ending.
  - The bonus maze (`f22_0ec8`): 24 x 15 cells from one of three mazes per level group (`DS:3F6A`...), 120, 180 or 300 s. Edison walks with the arrow keys or buttons to the exit; three wanderers roam but do no harm, turning back when they see him. From maze level 3 the maze starts hidden and each step shows what Edison can see; from level 6 it's forgotten after each step. The exit is worth 100 points and 2 for each second left; clicking the exit gives up. The original runs the maze flat out, so the port paces each animation step at 40 ms (a guess).
  - High scores (`f24_*`): `MYSTERY.HS`, nine tables (levels 0-7 and custom) of ten entries (a 9-byte name, a 32-bit score). The list shows levels as 6-13, as the level buttons do. The map's High Scores button shows it too, then redraws the map (`g09_1c92`).
- **The custom level editor** (`editor.cpp`, segment 11, `f11_19b4`): in font `101`. Pick one of eight maps (the levels' sets of squares), then drag a game icon (`203E+`) onto each square of a scaled copy of the floor (`BE/100` x `C1/100` at `52, 2C`) and pick its difficulty; the number of levels per game is at `DS:197C`. "Level Ready" checks that every square has a game ("Defaults" fills the rest from the level's own list). Editing an existing custom level starts from its board. A custom level keeps its squares when the game starts, and on one the Arrow Puzzle lets the player pick the picture (`g12_1ab0`: Space and Tab browse, Enter picks). The random picture elsewhere is really random: `g12_1976` works one out from the square and then draws `random(17)` anyway.
- **Smitty's idle moments:** a 1 Hz timer (`g05_03de`) started on the first visit to the map sets `[B71E]` after 1-300 s and then every 121 s (it draws `random(180)` for the next wait but uses 120). A mouse then peeks out: on the map (`f06_21c6`), in the picture puzzles (`g12_1bec`), Concentration (`g15_10c2`), the Circuit Analyzer (`g16_07e6`/`g16_0830`), and the Folded Cube and Ball Sculpture (`g30_127c`).
- **Finding:** SDL's resampling audio stream keeps a few bytes queued until flushed, so the port flushes each WAV; without that, waits for the end of a speech (`[73B6]`) never finish.
- **Testing aids:**
  - `--drag T X0 Y0 X1 Y1 MS` holds the mouse from one point to another (for The Dig and the arrow buttons of 28-30).
  - `EDISON_SKIP=1` starts as `MALLSKIP.EXE` does (`tools/reference/mall_skip.py`): the courtyard, then straight to "Please pick a level" as the player SKIP; `EDISON_SQUARE=P` or `P,D` plays puzzle P (at difficulty D) on every square, as its `--puzzle` and `--difficulty`.
  - `EDISON_FLOOR=1` (with `EDISON_SKIP`) goes from the office straight into the Museum on the first visit, as `mall_skip.py --floor`'s `MALLSKIP.EXE` does (no entrance, letter or speeches; the timers start as usual), so a square is a click away about 7 s after the level's; both deal the same puzzles as the long way round. `mmcompare.py`'s `pPPdD` scenarios use it: puzzle P at difficulty D (easiest and hardest), its first 20 s.
  - `EDISON_RNG=<24 hex digits>` sets the generator's six words (as memory has them: read the original's with `memwatch.py peek MALL.EXE u:7638 ...`); `EDISON_RNGLOG=<file>` writes each `random(n)`, its result and the six words after (as numbers, low word first).
  - `EDISON_LOG=<file>` copies warnings and other log lines to a file (the GUI build has no console for stderr).
  - `--puzzle 16` plays the bonus maze (at `--level`), 17 a won game's end, 18 a lost one's, 19 the end after passing the quiz, 20 the custom level editor.
  - `EDISON_CUSTOM` plays a puzzle as on a custom level; `EDISON_IDLE=N` makes the idle moment come after N seconds.
- **Random numbers** (`random.cpp`, segment 46): `f46_0000` is an additive lagged generator on six words (`DS:7638`): the sum, with carries, of the first five becomes the sixth and the others shift down. Nothing seeds it: it starts from the data segment's words, and each `f04_005c` (the port's `show`), `g17_1256` and `f19_16a2` stir it with `random(200)` more draws (`f06_2cf8`). So the original deals the same board and puzzles for the same play, and the port, drawing in the same order, deals the same: from the level pick (25 draws in) both hide level 6's objects at (13, 27), (7, 17), (4, 2). The Folded Cube's and Ball Sculpture's turns take `f46_0000` itself.
- **Lines:** the library's line (`f57_0024`, 32-bit code, the same in WINMAIN and EDISON.EXE) draws one run of pixels a row from the top end, each row's run ending where the line is half a row on (16.16, rounded up), not Bresenham's; the port does the same (the clock's hands).
- **Finding:** `[B786]` is set by the setup: 0 the first time (mode 1), 1 after a game. On 1 the map skips the Director's letter and Smitty's greeting is "Are you ready to go, Smitty" (`0x1212`) instead of "Smitty, what's up?".
- **Checked against the original** (`tools/testing/mmcompare.py`, through `MALLSKIP.EXE`): from the level pick to the office's map (the level pick, "Let's do it." and the run off, Edison and Smitty's entrance, the Director's letter page by page, the objects and "Check the map and good luck!", the clock) pixel for pixel, and on into the Museum (the map on the table, `DS:0E76`, not the door: that is Credits) to its map of buildings. Found and fixed: the generator, the line, `[B786]`, "Let's do it."'s bubble at x 4 (was 1), and the countdown 2 s late (below).
- **Finding:** the button's state (`[739E]`: bit 0 set on `WM_LBUTTONDOWN`, cleared on `WM_LBUTTONUP`, bit 1 the right button; `[739F]` keeps the presses) changes only while the game takes its messages (`f06_2c70`). Many waits don't: Edison's and Smitty's talk (`f09_0dc4`, `f09_10fe`) and the entrance's steps (`f09_15f8`) only watch the countdown, so a click's release waits in the queue. After the objects are drawn the first time, `f09_1dd8` waits a second (09:29fb) unless `[739E]` is set, read before it takes any messages; a click on the Director's letter's last page is still down there (measured: down at 32.0 s, up at 35.7 s with an 80 ms click), so the original skips the second, shows "Check the map and good luck!" at once and starts the countdown a second sooner. The port keeps that state (`GameContext::held`, from each `pump()`); its talk and entrance waits `spin()` (the timer and the display, no input), and the objects' wait reads `held` first.
- **The puzzles' openings checked** (`mmcompare.py pPPdD`: each puzzle at its easiest and hardest level, its first 20 s): all pixel for pixel but two small leftovers (below). Found and fixed:
  - **When the screen is shown.** `f04_005c` stirs the generator, and the Folded Cube (29:126A: after the cube's turn), the Ball Sculpture (30:159C: after the shape's and the answer's draws), Dropping Squares (18:2332: before the deal) and The Dig (21:1A24: before the tiles' deal, `g21_02a8`) show their screens at their own points among their draws; the port showed them first. Their copies of the empty view to screen 2's top left come after the show too.
  - **`draw_poly`** (`f32_2e46` → `f58_02bd`, the library polygon WMAIN has at `f81_0280`): a list of records, each `colour << 8 | count` and its points. The Folded Cube's, Stackup's and What Comes Next's outlines are two-point records (a line by the polygon's spans: colour 0 for the cube, FF for the others), and Color Transformation's shapes are plain records; the port draws them with the library's spans (`fillPolygonSolid`, two points allowed), not its scanline fill and line.
  - **The library's scalers** (`f41_0024` opaque, `f41_0330` with colour 0 clear; WMAIN's `f73_0324`): each pixel the source's at a 16.16 step of 256 / scale (`ArtechGame::scaleStep`). Used by `f06_189a` (the Ball Sculpture's balls), its four pictures (`g30_0f98`) and the picture puzzles' picture (`g40_0030`).
  - The Folded Cube draws its twelve colours face by face (`DS:B728`, `B726`, `B738`, ...); Concentration has nine levels (the ninth: layout 0, matches of 3, 300 s); Dropping Squares' next piece (`g18_0fd2`) draws its column again till its top is empty and takes each square of cloth before its place.
  - **MALL's line** (`f06_1af8`, around the library's `f32_29e4`; WINMAIN calls that directly): on the display it draws on screen 3 and copies only a vertical or horizontal line over, `|dy|` rows (or `|dx|` columns) from the first end, so the far end stays off the display and a slanted line never reaches it (`ArtechGame::displayLine`; The Dig's belt dividers). The Switch Puzzle's marked tile (`g13_0000`) uses it too (its bottom right corner stays), and so does the outline `f06_08c0` (`ArtechGame::frame`: "Game Paused!"'s box, Dropping Squares' cells; its style-2 box in `f06_09ee` is never asked for, all the message boxes are style 0 or 1). Its other callers (`f06_031a`, `f05_00a0`) draw off the display; the editor's (segment 11) are ported.
  - Left (then): Dropping Squares' second piece (17 pixels; see below) and Color Transformation's frame corners (since fixed: the sine table).
- **The puzzles played through** (`mmcompare.py playPPdD`, moves from `mmsolve.py`): all sixteen against the original, most at two levels, through the win or the exit, pixel for pixel (but for what the original doesn't pace, below). Found and fixed:
  - **The clock through the count.** Codes (`g20_143c`) and the Planetarium (`g28_174c`) stop their second only after the bonus count, so a second during it takes a step off; Codes then shows the time left at the solve (`[930E] - [B794]`) and passes mode 3 to the result under a minute left. The Ball Sculpture starts its second again with each sculpture (30:15FB). The picture puzzles' `g12_1416` runs the count twice if a second ticks during the lever's 0.4 s (the second time from nothing): faithful, both games.
  - Codes' swap counter `[3D80]` is never reset (kept between games). The Q&A Period loads and shuffles its questions after the board is shown (19:129a: the show's stir first). The Circuit Analyzer's plugs are `20A9` + colour (the code's sprites). The Planetarium's fact box sits at the top, centred on its own lines (28:1134). The Switch Puzzle's marked tile uses MALL's display line (its bottom right corner stays). Color Transformation's circles take the sine table (`g51_1000`), which fixed its frame corners.
  - **The Ball Sculpture's added balls** (30:0e81): each candidate is checked against one word only (the list's first, then 27 words on for each ball let in: the loop advances its saved start, 30:0ea6).
  - **The Dig** picks a tile up with a click and puts it down with the next press (`[739F]`, 21:11db), not on the release; it never recomputes the UI colours (no `f06_01f6`), and draws the clock only when a second has gone.
  - **Dropping Squares' squares of cloth fly home on the display only** (`g18_0060` queues one in a table of 12 at `DS:84CE`, `g18_0120` moves it): whole steps of a fortieth of the way (truncated) towards its picture's corner, till either way gets there or 300 steps, what's under it kept on screen 2 at (0, A0). Screen 2's place stays black, and the pictures' places overlap the store of squares (screen 2's bottom left, `g18_0000`), so the squares stored there keep the black of the places filled after them; the port had put each square on screen 2's place too, over its neighbours in the store.
  - **The Dig's held tile** is grabbed into a DIB (`g35_071a`: `AllocateBitmap`'s 0x36 bytes of headers and one palette entry, then the rows bottom up), but the scaler is given the block's start as the pixels' (21:0d4e; the Ball Sculpture's `g30_0f98` skips the headers): each row is read 0x3A bytes early, its left 58 pixels from the row below, the bottom row's from the headers. The drag draws the clock at y 0xC, not 0xA (21:0da0).
  - Unpaced in the original (left as the port has them): the cube's and Planetarium's turning arrows (0x180 a pass of the loop: about 570 a second under winevdm), the bonus counts, Dropping Squares' drop and its setup, the Dig's end walk. Dropping Squares starts its timer (`g18_21f8`) before loading and dealing its screen, and the loop's first pass cuts the drop from every 10 calls to every 5: the calls during the setup (5 under winevdm, measured) put the column's step a call after the second's; the port's setup takes no time, so its step comes with the second (scenario shots avoid the tenth of a second between).
  - The Dig's solution button shows the solution while it is held (21:1b2d: the pressed sprite and the solution's wall, then `f06_2c70` till `[739E]` clears; `play15d0s`, pixel for pixel). It had seemed not to: `mmcompare.py` wrote a hold as down, wait, up, so the shots meant for the hold all came after the release.
- **The end of a game, setup, saved games, the editor** (`mmcompare.py timeout`, `allfound`, `allfound3`, `setupnew`, `savegame`, `loadgame`, `editor`, `pause`; `mall_skip.py --time` / `--found`; `setup=True` scenarios run `MALLFREE.EXE` from the title, the port a second sooner): pixel for pixel. Found and fixed:
  - The countdown (`g09_0a7a`) tests the time before its step: "0:-1" shows for a second before the game is over. It runs on through the dance, quiz, maze and ending (the office stops it after the high scores, 09:2FCA); timers are told apart by their callbacks, so the maze's (`g22_0100`) runs beside it.
  - "We lost!" is at two thirds across (23:0d5a). `f06_01f6` (the UI colours) is called by the setup, the map, the message box and the puzzles of segments 12-17, 20 and 25-27 only: Dropping Squares, the Q&A, the Planetarium, the Folded Cube and the Ball Sculpture keep the previous screen's (the Q&A's had turned "We lost!" purple).
  - The setup's steps are a switch (08:267E). Step 2: a player's file found sets `[B786]` (08:2532), so a returning player is "after a game": no Director's letter, and "Are you ready to go, Smitty". Unused objects are saved with all three bytes FF (09:27E9): the port's player file is now byte for byte the original's.
  - The yes/no (`f06_2976`) and the pause read keys held (`[929C]`, `[B774]`: a scan code's bit) once a pass, so a typed key can come and go between passes: the scenarios click.
  - The editor (`f11_19b4`) shows its screen without `f04_005c` (no stir) and with the picture's own colours 0 and FF (11:1a55, 11:1b86); its boxes are in MALL's line. Under winevdm each blit takes the source's colour table of the moment, so the editor's and the forced palettes mix on the display; the port has one palette at a time, and its 0 and FF differ there (the `editor` scenario takes them as equal).
  - P pauses the game at any panel poll (`f06_219c`: `[B776] & 200h`, scan 19h): "Game Paused!" (`f06_229a`) till Space or a mouse button is held. Its flag `[7576]` is the timer DLL's (`SETUPTIMERDLL`, 45:0160): no ticks while paused.
  - The bonus maze's loop is unpaced: under winevdm about 600 passes a second (memwatch, the wanderers' step at `DS:89E8`), so its wanderers can't be compared; the port keeps 40 ms a step. The maze doesn't end at 0:00.
- **Finding:** `f09_0dc4` saves and restores Edison's whole area of screen 2 around his talk; without that his pointing pose stays behind.
- **The keys** (the final coverage scan: of segments 1-30's 470 functions, the 89 the port never cited are the window procedure, file and drawing primitives, panel callbacks ported inline, the help button's flag `[96]`, and code nothing calls: `g06_006c`-`0180`, `g18_087c`, `g18_08c4`, `g21_0714`, `g25_084a`; what was missing was keys). The library keeps two tables of scan-code bits: `[B774]`-`[B783]` latched (set on `WM_KEYDOWN`, cleared only by `f39_0130`, which `f06_2ccc` calls, so a key stays latched till the next message box or a handler clears it) and `[929C]`-`[92AB]` held; `[C272]` is the last scan code down and `[B756]` "a key came". Ported, all pixel for pixel against the original (`mmcompare.py keys03`, `keys05`, `keys09`, `keys14`, `keys10`, `keys01`, `keysF`):
  - H shows the help, as the button (`[B75A]`), in Binary Lights (`f17_0b9c`), Dropping Squares (`f18_0bc4`), Stackup (`f25_189c`) and What Comes Next (`f26_0c82`). Dropping Squares turns its piece with the keypad's 5 as with Up (scan 4Ch: the port reads the keypad's 2, 4, 5, 6 and 8 by their keys, NumLock or not, as the scan codes are).
  - Stackup's developer keys (`f25_189c`): 1-3 start the game again at that level (25:1c12: 120 s, no points, five new rows, the misses kept); 0 makes the time 2000 s, and as it stays latched it does so every pass, so the clock stands at 33:20 till H or the help's box clears the keys. The restart showed that `f25_11b0` draws an unused colour for each stacked bar (`random(50h) + 70h` unlike the last; at least 12 draws a row), which the port had left out: invisible at the start, where every row is dealt before any is drawn.
  - Esc held ends the Slide Puzzle as it stands (14:0552: "Maybe next time!", no lever); Esc ends the Planetarium unscored, without Edison (28:1489, 28:15e7: `f28_13d0` returns before `f06_1d76`).
  - On the floor, F counts the next square's game as won without playing it (10:080e, 10:0993).
  - The Ball Sculpture's N steps `[B39C]` by 2 (30:1615), which nothing reads.
- **Testing:** `edison --game mystery` starts the game directly; `--level N` skips setup and plays level N. Leaving it returns to the launcher without the opening.
