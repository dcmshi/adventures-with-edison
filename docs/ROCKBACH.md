# Rock and Bach Studio (WINMAIN.EXE)

Data is in `RB.D01` (fonts `0100-0103`, palettes `0200-020C`, backdrops
`1000-100E`, sprites `2000-2417`, `3200-3202`, one WAV `6000`). Sound effects
are WAVs in `<CD>\RB\EFFECTS\`, and the player's own WAVs can sit next to the
game. FM music comes from five drivers, `ADLIB.DLL` and `ADLIB1-4.DLL`, one per
activity. The disassembly is `extracted/disasm/winmain.asm` (`tools/nedis.py`).

The Artech library is the same as MALL.EXE's, five segments later: `f36_*` is
the C runtime (MALL `f31_*`), `f37_0032` selects a screen (MALL `f32_0032`),
`f37_007c` copies an area, `f37_0d48` shows a backdrop, and so on. Segments
33-35 are game code.

## Top level (`f33_0422`)

- `f33_0000`: the font `101`; the working folder; the CD drive (the first
  drive where `f27_01ea` finds the CD), so the effects are at
  `X:\rb\effects\` (`DS:6564`).
- `f33_03dc(1)`: the intro: "Corel presents" (`f25_0016`, backdrop `100A`),
  then FM driver 4 (`ADLIB2`), `f20_0000`, the Rock and Bach Studio logo (`f05_04d8`,
  `1001`).
- Then a loop over the hallway (`f24_1d4a`, backdrop `1007` with its
  hot-spot mask `1006`). It returns the activity, and each activity switches
  the FM driver first (`f02_006e(n)`):

| Result | Driver | Activity | Entry | Backdrop |
|---|---|---|---|---|
| 1 | | leave: `WinExec("edison.exe -O")` | | |
| 2 | 0 (`ADLIB`) | the jukebox: play the bands' songs | `f03_22e8` | `1000` |
| 3 | 1 (`ADLIB1`) | the Drum Clinic | `f06_1e9e` | `100B` |
| 4 | 3 (`ADLIB4`) | the Music Library (composers) | `f30_1780` | `100D` |
| 5 | | a new player (setup again) | | |
| 6 | 2 (`ADLIB3`) | Harmony Hall (keys, chords, styles) | `f07_1908` | `100C` |
| 7 | none | the Instrument Room (`instinfo.hi`) | `f08_0f2e` | `1009` |
| 8 | none | Sound FX (sample, echo, reverb) | `f29_17e2` | `1008` |
| 9 | 4 (`ADLIB2`) | the Studio (`f35_018a`): videos | `f04_112e` | `1004` |

- The Studio's parts: the band maker (`f14_10f4`, `1002`), the song maker
  (`f15_1f84`, `1003`), the video makers (`f16_217c`, `f17_20f2`,
  `f26_1edc`, `1005`) and the player (`f18_22a4`).
- The hallway also has the credits (`f24_12e6`, `100E`) and "Do you want to
  quit this game?" (`f24_0f8c`). The player's file is `user.yyy`; `ed.yyy`
  keeps Edison's look.

## The FM drivers (segment 2)

`[83D0]` is the driver in use: -1 none, and 0-4 are `ADLIB`, `ADLIB1`, `ADLIB3`, `ADLIB4` and `ADLIB2` (not in name order: that is how the switch in `f02_006e` calls them).
`f02_006e(n)` removes the current one (and its 72 Hz timer, `f50_020a`) and
installs driver n. The other wrappers call the driver in use:

| Wrapper | Entry |
|---|---|
| `f02_01e8` | `GETADDR` |
| `f02_026c` | `SENDSND` |
| `f02_02e0` | `GETVAR` |
| `f02_0354` | `SSTATUS` |
| `f02_03d8` | `GEVENT` |
| `f02_044c` | `GFLUSH` |
| `g02_04b2` | `DIRECTDRUMOUT` |
| `f02_053a` | `INSTALL_PATCH` |
| `g02_05e6` | `PLAYINS` |

The port's driver (`engine/src/audio/artech_fm_driver`) has them all.
`GETVAR` returns `DS:52E` in every build: the `GE_FLG` byte that op `50`
(EVENT) increments. `SSTATUS(ch)` is the channel's ticks byte (`+05`).
`INSTALL_PATCH(ch, patch)` writes patch `[patch table + 2 * patch]` to the
channel, `PLAYINS(ch)` keys it off and on again, and `DIRECTDRUMOUT(bits)`
keys drums in register `BD`. The exports are Pascal calls.

## Songs (segment 20)

The song player lives in the driver's sound table (ADLIB2's, for example):

- Sound `70` calls sound `6` (set-up) and then JUMPs to the address stored at
  "sound" `71`, which is really that JUMP's operand.
- Sound `72` is the slot list: for each of 16 slots, 0x18 bytes: START the
  slot's 8 sounds, HOLD, then EVENT 1 (so `GETVAR`'s byte counts slots).
- Sounds `73 + 8 * slot + i` are 256-byte buffers the game fills by copying
  a track's riffs (sounds `8 + 8 * track + i`), moved to a key (`DS:20F8`)
  and mode (`DS:2104`) given by a style (`DS:20B8`: 16 {key, mode} pairs).
  Riffs 4-6 aren't moved. Note bytes (up to `7B`, 4 bytes each) are rewritten
  as octave * 12 + semitone; commands (2 bytes) are skipped; the scan ends
  after an `84` or `88`.
- `f20_0070` points sound 71 at the list (`f20_00e8` at slot n), puts the
  tempo (`[20B6]`) into sound 6, clears the counter and starts sound 70.

## Port status (`engine/src/rockbach`)

- `edison --game rockbach` (or the launcher's Rock and Bach button) runs it; `--level N` goes straight to hallway result N.
- The drawing helpers are shared with Mystery at the Museums (`ArtechGame`, `engine/src/artech/game.h`): WINMAIN's segment 28 is MALL's segment 6 again.
- **Intro:** "Corel presents" (`f25_0016`: backdrop `100A` for 10 s or a key or click, colours `70-7F` turning every 1/8 s). The logo (`f05_04d8`, `1001`): a 16-slot song plays (tempo `F0`); colours 1-5F pulse (each component bounces between 0 and 255, 14 steps a second); at 9 frames a second a spotlight (bitmap `212F` tiled into the beam polygon, spot `2130`) sweeps between x 200 and 300 and a band member plays under it (frames 1-5 of `2070 + 5 * member` from the lists at `DS:0194`). Each slot of the song brings the next member (`DS` list in the code) and the crowd (`6000`); it ends after 15 slots or at a key or click.
- **Widgets** (segment 34, `widgets.cpp`): a list of 0x1B-byte records (x0, y0, x1, y1 inclusive; flags 2 pressed, 4 no bevel, 8/10 sliders, 20 label, 40 radio, 80 filled face, 100 bitmap face, 200 hidden, 400 toggle; face colours and bitmaps, a hot key, overlays, a draw mode). `f34_0ebc` takes the list and the bevel colours, `f34_07b8` draws one (the bevel only on the display), `f34_0fb6` polls: the widget clicked (or its hot key) draws pressed until the button comes up, then its index comes back. Sliders aren't ported yet.
- **Hallway** (`f24_1d4a`, `hallway.cpp`): backdrop `1007`; the widgets of `f24_0000` (four hot spots and "done" for the look menu, and three Yes / No pairs), all hidden until a question shows them. The first time:
  - Edison walks in (`f24_0666`: frames `21C3 + n`, 5 a second, 86 x 140 at y 240; a click or key skips to a call's last frame, and to the last frame of every later call until the input is cleared).
  - "Hi I'm Edison." / "What's your name?" (`f24_1452`: a bubble, `f24_0448`, of colour `EF` with corners `229A-229D` and the tail `229E`; sound `6020`). The name (`f04_0d52` at (208, 206): up to 10 letters, upper case, "Player" when empty).
  - `user.yyy` (`f24_09c0`): 26 records of 0x19 bytes: the name (0x13 bytes, `***` when free), 0, a flag, the look (4 bytes). A new player gets the first free record and makes Edison's look; one who's been here is asked "Do you want to change the way, I look?" (widgets 7 and 8; Enter is No).
  - The look menu (`f24_1bba`: `21E2-21E4` at x 390; each of its four rows takes that part's next choice of 8; "done" or Enter ends it; Edison nags, `6022`, after 8 s and then every 20 s). The four parts are colours `E1-E3`, `E4-E6`, `E7-EA` and `EB-ED` from the tables at `DS:223E`, `2286`, `22CE` and `232E` (6-bit B, G, R, made 8-bit R, G, B by `f24_0c0c`, which only does the first 0x18 colours of each, so the third part's last two choices stay as they are). `ed.yyy` keeps the look (4 bytes); `[9244]` is the player's record.
  - Then the mask `1006` goes on screen 2 and a click reads it: 1 "Do you want to quit this game?" (`f24_0f8c`: a clean hallway, Edison `2413` talking, sound `601E`, widgets 9 and 10), 5 the credits (`f24_12e6`: `100E` for 40 s or a key or click), 10 the sign (a mouse at (522, 336): frames `22BA + {0-7, 10, 7, 10-15}` 3 a second, `6001` on frame 6). Anything else is an activity. The sign draws on screen 2 over the mask, so its rectangle then reads the hallway's colours: as in the original.
- **The jukebox** (`f03_22e8`, `jukebox.cpp`; `ADLIB.DLL`): backdrop `1000`, 64 widgets (`f03_006c`). Four columns of four members to pick (drums, chords, bass, solo: members `DS` list in the code, names at `DS:048C`; pictures `200F + m` up, `2033 + m` picked), GO / STOP, eight songs (`DS:03DD` names), the stage (`212A` at (186, 42); each member's sprite `206F + 5m + frame` at its place, `DS:0078 + 8m`), four lights (colours `5 + 16n`, 11 each: the next of four colour sets from `45 + 16k`, a chase that turns them, a pulse that moves each component, and a brightness slider), a volume slider for each player and the tempo (slider, or the turtle and the hare, `A6 tempo` from `7A` on). At 8 a second (`g03_1770`) the stage moves: a member idles while the driver says their part plays (`GEVENT`: role in the high nibble, 1 starts, 2 stops), or plays through their frames (to `FE`) when clicked, to the crowd (`6000`); with no part left playing the song starts again.
  - The music (segment 9) talks to the driver through sound 1: the game writes `09 00`, commands (`AC channel volume`, `A6 tempo`) and `88` over it and starts it. Each role's part (`DS:1494`: 4 roles x 4 members, records of a channel count, the channels at +4 and 8 songs x 3 riff sounds at +7; `DS:1274` is silence) plays the song's riffs (`f09_00fc`).
- **The Drum Clinic** (`f06_1e9e`, `drums.cpp`; `ADLIB1.DLL`): backdrop `100B`, 61 widgets (`f06_0046`): eight kits (names `DS:081E`, drum icons `DS:0784`, pictures `2343 + k`), eight patterns (`DS:087C`; 8 is "User Pattern"), four modes (widgets 9-12: the kit's picture, the grid, a chain of up to 12 kit and pattern pairs, the keyboard), each drum's three sounds (the mouths, 1-2-3-2-1), the drums themselves, GO / STOP, the tempo (it goes back to 3C with each mode), the step slider, the bomb (sound `1B`, and the player's pattern cleared) and four palette tricks (colours 2-FD turning, groups of five chasing, three groups changing places, colours 60-65 from four sets). The grid is 64 steps x 5 drums (`2341` on, `2342` off, at (192 + 4s, 64 + 36d)); a click there turns a step on or off in the player's pattern (a standard pattern is copied there first). The playhead is bar `2362`, kept at (0, 0) on screen 2 with what it covers at (0, C8). The keys play the drums (`DS:07AC`: scan codes, a zone of the keyboard each); in the keyboard mode each row scrolls 4 pixels 15 times a second and a key marks its row.
  - The drum machine (segment 10): patterns at `DS:73CA` (8 kits x 8 patterns x 64 steps, a bit a drum), 0-6 from `STANDARD.PAT` and 7 from `NONAME.PAT` (whatever the player's name; saved on the way out). A 72 Hz timer plays a step every period / 8 ticks (the period is `8C - tempo`). Drum d of kit k is sound `DS:14D8[5k + d] + 0x14 v` (v the sound, 0-2); choosing a kit writes each sound's drum number into its first byte. Sound `E` silences.
- **The Music Library** (`f30_1780`, `library.cpp`; `ADLIB4.DLL`): backdrop `100D`, 30 widgets (`f30_0000`). Eight composers (portraits `23C7 + c`, a glimpse of `23E6 + c` first; names and dates in the data segment), each with their life in `LIBINFO.HI` (pages listed at `DS:2B08`; INFO shows the next, waiting for a key or click when a page is full), their pieces in `SONG.HI` (three lines each, drawn in colours 1-6 so that setting one to colour 7 picks it out) and words about each piece in `SONGINFO.HI`; the composer's years on the timeline (`DS:2AE8`, a bar of colour `25`). The `.hi` files are blocks of lines ending with a line that starts with `!`; `!E` ends the file (`f21_0000` shows block n in a box). GO plays the piece with its own instrument (`DS:2E68`); four instruments (patches `DS:17D8`, `INSTALL_PATCH` on each channel); the tempo (`DS:2DBA` per piece, scaled onto the piece's own tempo); a quill in the inkwell. The player (segment 12): `DS:1858` points at each piece's record (a count of sounds, the first, its tempo), commands go through sound `F`; the piece is over when channel 0 is quiet (`SSTATUS`).
- **Harmony Hall** (`f07_1908`, `harmony.cpp`; `ADLIB3.DLL`): backdrop `100C`, 45 widgets (`f07_0000`): the 12 keys, 8 kinds of chord, GO / STOP, 8 styles (names from `DS:11E7`; a style starts again in C major), a fader for each part (drums, rhythm, lead, bass), the riff button's animation (`23A9 + n`), four lights (colour ranges `B0-C5`, `F0-F9`, `90-96`, `A0-A8` turning) and RESET (their colours back), the tempo. The stage (`f07_1278`): the guitar's fingers (`DS:0D12`, places `DS:1072`), the key's flats, the chord's notes on the staff (`DS:0A12`, places `DS:10EA`), and the piano, whose 23 keys are colours 1-23 (the chord's notes, `DS:0892`, lit). Choosing a style also sends the Music Library's tempo command (`f12_0270`), as the original does.
  - The player (segment 11): four parts (`DS:1700`: records like the jukebox's); sounds `C-10` get the rhythm part's three riffs, the lead's and the bass's for the style, moved to the key (`DS:1750`) and chord (`DS:175C`) unless it's C major; sound 9 (or `11`) plays them; commands go through sound `B`.
- **The Instrument Room** (`f08_0f2e`, `instruments.cpp`; no FM): backdrop `1009`, 21 widgets (`f08_002c`): 16 instruments (pictures `2263 + i`), each with two pages of `INSTINFO.HI` (block 2i + page; the book button, and choosing an instrument, turns the page), its sound (`600D + i`, the WAV) and its waveform (one line a column, colour `D0`, from the WAV's samples) with a marker (`227E`) that follows the sound (1/15 s steps of `DS:` table / size), its range on the keyboard (colours `10-44`: `45`, the range `46`) and its part of the gauge (colours 1-7: `8`, its own `9`). A click on the picture or the note button plays the sound; one on the keyboard also shows the conductor (`22E3 + n`, 21 frames at the bottom right).
- **Sound FX** (`f29_17e2`, `soundfx.cpp`; no FM): backdrop `1008`, 38 widgets (`f29_0000`; 6-26 are a radio group it never shows). LOAD (the chooser, then a list: see the dialogs), SAVE (a name, then the header at the playing rate and the selection), DELETE (the game's folder only, after "PERMANENTLY DELETE THE / WAVE FILE: name"); GO / STOP; LOOP, BACKWARD, ECHO, REVERB, LOFILTER (each lights its sliders); sliders for the speed (`(200 - v) * 20 - 2000` on the rate), the echo's delay and gain, the reverb's delay and gain and the filter's frequency; a tip line (`DS:2AA2`). The waveform (`f29_0ffe`: a column every len/596, colour `E0`) has handles (`222B`/`222C`); only the end one can be dragged (the original never hit-tests the start's).
  - The effects (segment 13, `f13_0a90`): on the input (made signed), in order: reverse, the low filter (two poles, coefficients `DS:18EC` by frequency x 16 / rate), one echo (read from the input), reverb (two taps of its own output); then scaled to its loudest and made unsigned. Up to 55000 bytes. Playing writes the WAV header over the selection's first bytes, as the original does.
- **The file dialogs** (segments 22, 23; `dialogs.cpp`): LOAD's chooser (CD ROM: the CD's `\RB\EFFECTS\`; HARD DRIVE: the game's folder; remembered for the next LOAD of the same kind), the list (12 slots under icons `22FC`/`22FD`, the names upper-cased, a letter jumps, a scroll bar with arrows and a thumb), the name box (8 letters or digits, a blinking caret, O.K. / CANCEL) and message boxes (CANCEL / YES / O.K.). Dialog colours: the 16 EGA colours' nearest entries in screen 2's palette (`f27_0054`).
- **Activities not ported yet:** the Studio; each switches its FM driver and comes back.
- **Timers:** the library's countdowns tick in timer slot 9, so the games' own periodic timers use slots 5-8.
- **Sounds:** `f27_020e` plays `<CD>\RB\<name>.wav` by id (`DS:278C`, id - `0x6000`), else `<name>.wav` in the game's folder (the player's own); longer than 64 KB plays nothing.
