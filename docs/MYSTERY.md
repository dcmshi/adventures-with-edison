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
- **Setup:** the whole new-player flow runs: title, courtyard scene, name prompt, character changer (colours live), "Cool!", level pick, "Let's do it." and the run-off. Returning players (`.INF` files), the saved-game prompts and high scores aren't ported yet.
- **Setup details:** `setup.cpp` has the courtyard backdrop and the scene (`f08_06f8`), including the lip-synced "Cool!" part. Only part 0 runs so far; the name entry comes next.
- **Findings:**
  - The UI colour matcher reads its target colours (`DS:00BE`) as signed chars. Components above 127 therefore count as negative, and the "white" entries match dark palette colours. The port keeps this.
  - Speech: id - 0x4010 indexes the name table at `DS:0566`. The file is `<CD>\MYSTERY\<name>.wav`.
  - Flags: `-A` turns music off (`[0054]`) and `-T` sets `[7399]`.
- **Checked against the original under winevdm** (`tools/reference/otvdm.ps1`): the title screen and the courtyard scene have the same frames, colours (pixel-identical) and timing. The original next shows "Hi! I'm Edison. What's your name?" in a speech bubble with a `>` prompt: that's setup step 1.
- **Game loop and map** (`game.cpp`, `floor.cpp`): `f09_1dd8` and the map screen are ported: board generation per level (the level tables are immediates in `f09_1dd8`; the puzzle list per level is segment 62), the office with the clock (hands from the segment-51 sine table), the timer, the object grid, Edison and Smitty's entrance and talk, the Director's letter, the help boxes (text resources `3F06`/`3F07`), quitting, the Museum floor (`f10_0708`: squares are told apart by colour `A1 + n` on screen 2, painted from the path polygons at `DS:164A` and the room masks `22C4+`), and the found / not-found speeches.
- **Placeholders:** the 16 puzzles (each counts as solved), the end screens, bonus game and high scores (segments 22�24), Smitty's idle animation (`f05_03de` / `f06_21c6`) and the return-visit setup (`f08_21b6`).
- **Finding:** `f09_0dc4` saves and restores Edison's whole area of screen 2 around his talk; without that his pointing pose stays behind.
- **Testing:** `edison --game mystery` starts the game directly; `--level N` skips setup and plays level N. Leaving it returns to the launcher without the opening.
