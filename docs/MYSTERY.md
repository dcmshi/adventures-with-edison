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

- `f09_15f8`: "Okay, here are the objects."
- `f11_*`: level select ("Please select difficulty level."), the custom-level editor ("Click on a game icon, then click on the map square…") and map confirmation.
- `f06_*`: shared game UI: the timer display (`%d:%02d`), "Game Paused! Please, Press Space Bar to Continue", "< Press mouse/any key >", and WAVs by name.
- `f23_*`: the end of a game: "Time taken", "Happy faces of Edison, Smitty, and …", win and lose messages.
- `f24_*`: high scores (`MYSTERY.HS`).

## Puzzles (to be confirmed one by one)

| Puzzle | Segment(s) | Evidence |
|---|---|---|
| Slide Puzzle / Arrow Puzzle / picture puzzles | 12–14 | "SPACE/TAB to view pics", "LOGO SIZE NOT DIVISIBLE BY # OF SQUARES" |
| Concentration | 15 | "TRATION: message number is too big", "MATCH 2/3" |
| Dropping Squares | 18 | `column.c`, "fill in the missing squares of the fabric puzzles" |
| Question and Answer Period | 19 | "JEOPARDY: answers are not correct." |
| Codes | 20, 30 | "decode the secret message using famous coding systems", unscramble |
| Binary Lights | ? | "Computers use a binary number system…" |
| Stackup | 25 | `stackup.c` |
| Color Transformation | 26 | `colour.c` |
| The Dig | 22? | "Time:", "Score:"; the `#`/`O`/`I` 25-column mazes in the data segment |
| Folded Cube, Liberty Planetarium, 3D Ball Sculpture, Circuit Analyzer, Switch Puzzle, What Comes Next | ? | names at DS:198C… |

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
- **Setup:** `setup.cpp` has the courtyard backdrop and the scene (`f08_06f8`), including the lip-synced "Cool!" part. Only part 0 runs so far; the name entry comes next.
- **Findings:**
  - The UI colour matcher reads its target colours (`DS:00BE`) as signed chars. Components above 127 therefore count as negative, and the "white" entries match dark palette colours. The port keeps this.
  - Speech: id - 0x4010 indexes the name table at `DS:0566`. The file is `<CD>\MYSTERY\<name>.wav`.
  - Flags: `-A` turns music off (`[0054]`) and `-T` sets `[7399]`.
- **Checked against the original under winevdm** (`tools/reference/otvdm.ps1`): the title screen and the courtyard scene have the same frames, colours (pixel-identical) and timing. The original next shows "Hi! I'm Edison. What's your name?" in a speech bubble with a `>` prompt: that's setup step 1.
- **Testing:** `edison --game mystery` starts the game directly. Leaving it returns to the launcher without the opening.
