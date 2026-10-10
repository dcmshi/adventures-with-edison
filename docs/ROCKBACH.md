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

### The tempo

Every driver's global tempo (`GLOBALTEMPO`, `1E7`) starts at 0, which holds
the parts that follow it (`SETAUTOTEMPO`, `GETMUSICTEMPO`) still; rendered at
0 and at 80h, 41 of `ADLIB`'s sounds, 76 of `ADLIB2`'s, 51 of `ADLIB3`'s and
129 of `ADLIB4`'s differ, none of `ADLIB1`'s. The game sets it with an `A6 tempo`
command (`SETMUSICTEMPO`) written over a sound and started:

| Activity | Driver | Where | Tempo |
|---|---|---|---|
| The jukebox | `ADLIB` | `f09_0320` through sound 1 | `A0` (`f09_0000`), then `B4` as its screen is set up (`f03_1e9a`); the slider is `7A` + 0-96h, the hare and turtle steps of 8 |
| The Drum Clinic | `ADLIB1` | none | its timer plays a step every period / 8 ticks, the period `8C - tempo` (`f10_04b6`), from `55` |
| Harmony Hall | `ADLIB3` | `f11_04a2` through sound `B` | `C0` (`f11_0000`); the slider `7A` + its value |
| The Music Library | `ADLIB4` | `f12_0270` through sound `F` | the piece's: below |
| The Studio, the logo's song | `ADLIB2` | sound 6's byte 1 (`f20_0070`) | `[20B6]`: the video's (`6799`, `F0` in a new one); the logo `F0` |

The Music Library scales the slider (`t`, `7A`-`FF`) from `BC`-`FF` onto the
piece's own tempo `b` (its record's byte 3) to `FF`: `(t - BC) * (FF - b) / 43
+ b`, a signed division, kept to a byte. Choosing a piece puts the slider at
`DS:2DBA[5c + n] + 7A`, so each piece starts at (sounds; `b`, `t`, the tempo):

| Composer | Pieces |
|---|---|
| 0 Bach | TOCA 7-12: 90, C0, **96** · ANNA 27-28: 90, C0, **96** · BACHAIR 29-33: 90, A2, **65** · BASHEEP 34-36: B0, AA, **9B** · BACHFUGUE 75-81: 90, C0, **96** |
| 1 Beethoven | BEETHPATHE 19-21: 80, B3, **6F** · BEE5TH 37-44: B0, C3, **B8** · BEE9TH 45-52: C0, B2, **B7** · MOONLITE 53-57: B0, A5, **95** |
| 2 Brahms | BRAHUNGAR 24-26: B0, AD, **9F** · LULLABY 82-86: 90, AB, **74** · RHAPSODY 102-108: B0, 93, **80** · BRAHMWALTZ 87-93: B0, 9F, **8E** |
| 3 Chopin | CHOPWALTZ 16-18: 80, FF, **FF** · FANTASIE 109-115: B0, 7A, **63** · CHOPNOCT 170-174: A0, B8, **9B** · POLONAISE 175-182: A0, DC, **CD** |
| 4 Debussy | COLINES 94-101: B0, C0, **B4** · DEBGIRL 124-131: A0, A7, **83** · LUNE 116-123: A0, AA, **87** |
| 5 Gershwin | BESS 67-74: 80, B2, **6E** · GERPRELUDE 132-136: B0, DD, **D6** · GERRHAP 137-143: B0, 7A, **63** |
| 6 Mozart | MOZTURKA 22-23: A0, D2, **BF** · MOZHORN 58-62: A0, B8, **9B** · MOZSON 63-66: A0, D4, **C2** · MOZEINEK 144-148: B0, C0, **B4** |
| 7 Sousa | SOOSTARS 149-154: B0, B3, **A6** · SOOSEMP 155-162: A0, C4, **AB** · SOOWASH 163-169: A0, CE, **B9** |

`fmplay` uses these by default (`--tempo` overrides): a Rock and Bach
driver's tempo from the table above, a piece's from `WINMAIN.EXE` beside the
DLL, and 80h for the other games' drivers.

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
- **The Studio** (`f35_018a`, `studio.cpp`; `ADLIB2.DLL`): makes and plays videos. A video is the record at `DS:6754` (0x26A bytes), which is also the `.vid` file as is (the CD has samples in `\RB\EFFECTS\`):

  | At | What |
  |---|---|
  | `6754` | 1 |
  | `6755` | the band: the player (0-35) in each role (drums, chords, bass, solo), -1 none |
  | `6759` | the song: 16 slots of {track 0-11, style 0-15} (words; -1 empty) |
  | `6799` | the tempo (`F0`) |
  | `679B`, `67AB`, `682B` | the backgrounds: 16 flags, 16 x 4 colours, 16 choices |
  | `684B`, `685B`, `68DB` | the special effects: the same |
  | `68FB`, `690B`, `691B`, `693B`, `694B` | the camera views: two flags, a word, (4 x 2 words), the choice |
  | `696B`, `697F`, `6993`, `69A7` | the names (20 bytes): the video, the song, the band, the producer |
  | `69BB`-`69BD` | the video, the song, the band have been named |

  `f33_0144` blanks it (names of 19 spaces), `f04_107c` makes a new one (the player the producer). Each Studio screen copies Edison's colours (`DS:8CC0`, `f40_1a26` takes source first) over its backdrop's `E1-ED`.
  - The front room (`f04_112e`, backdrop `1004`): EXIT (CANCEL / NO / O.K. to "SAVE CHANGES TO YOUR VIDEO: name BEFORE EXITING?"), LOAD (the dialogs, kind 0; it plays at once), SAVE, PLAY, NEW, EDIT, DELETE (the game's folder only); Edison on his chair (`218E`, `218A-218D` at random, 3 a second). NEW and EDIT of a new video go to the band maker and then the song maker; EDIT asks "What do you want to do?" (`f04_0580`: Song, Video, Done). After the song, EDIT is pressed again.
  - The band maker (`f14_10f4`, `1002`): a player for each role (a chosen one's picture is its shape in colour `70`); the solo's picture plays a riff (`22A7 + n`, 28 frames at 3 a second); leaving fills empty roles with each group's first and asks for the band's name once. Edison's questions (`f15_1d7e`): he walks in (`21DC-21E0`) with his bubble (`21E1`), then waits 10 s (or a click, key or 400 pixels of mouse moves) or takes a name (`f15_1cf2`: 18 letters, upper case).
  - The song maker (`f15_1f84`, `1003`): a track (1-8, 74-77) or style (9-16, 63-70) picked up follows the pointer; any click puts it down, into the slot clicked if it's one (tracks 17-32, styles 46-61). A click on a slot with nothing carried plays from there (the playing slot lit, `21E8`); GO, STOP; eight songs to start from (`DS:1BE6`); the tempo (a slider and steps of 8); the song's name; the band button goes back to the band maker. Leaving fills an unfinished song with track 0, style 0 and asks for the song's name once.
  - The player (`f18_22a4`, `video.cpp`): a scene for each of the song's 16 slots (the next when the driver's event byte counts one), in the picture at (`[2E92]`, `[2E94]`) = (7E, 1C), 352 x 248. Mode 0 is the whole video from the front room: the credits (`f04_08cc`), the audience's silhouette (`22CA-22E2`), the lights (colours `C0-DF`) going down in steps of 25/256, then Edison watching (`2211-2213`, a laugh now and then), the tape turning and the VCR's meter (17 bars, up while a sound plays). Modes 1-3 are the makers' previews (the background, the effect, the camera). A click or key stops it.
    - A scene (`f18_219a`): the background (`2165 + n` and `216A + n`, two halves; 5 is colour 1; redrawn only when it changes), its four colours as ramps in entries 1-3F and the effect's in 40-7F (`f18_0520`: 16 shades each, darker by sixteenths); each set turns a step at 10 (15) a second when its flag is set. The camera view (`f18_1c00`): 0 all four members, 1-4 one of them twice the size, 5 all four as faces; its flags start a member playing (with the crowd, `6000`) and put colours 80-BF and Edison's back.
    - A frame (`f18_0bb0`, 15 a second): the effect over the clean picture, which then keeps it: 0 notes (`2177-2186`) scattered, 1 a spirograph (lines between two points going round), 2 a sprite (`2187-2189`) that comes and goes, 3 seven shrinking boxes, 4 fireworks (eight sparks), 5 a walker (`2218-221B`); then the members, still, bouncing (their trails kept) or spinning (a quadrilateral filled with one member's picture, tiled). The spirograph's and the spin's points are the centre plus cos(a) x 352 / 8192, sin(b) x 248 / 8192, with a and b the angle (`[86E4]`, a step of `A6`) scaled and moved; sine is `f53_103b` (segment 53's quarter wave, a turn is 0x10000, 1 is 0x1000). The members' places are kept in the video (`693B`), so playing changes those bytes.
  - The video makers (`f35_005a`: the camera `f26_1edc` first, then the background `f16_217c` or the effect `f17_20f2` by their tabs; `rockbach.h`'s `VideoField` names the record's fields). Each has 16 scenes (widgets 12-27, the song's slots) with their flags under them (28-43) and six choices (44-49; the camera's two motions 83, 84). A choice picked up follows the pointer's x (0x42 x 0x30, a motion 0x42 x 0x22; kept on the screen as if 0x42 x 0x30, `f16_20e8`; a move of y alone doesn't move it, as the original's check compares y with itself) and goes into the scene clicked, with the choice's default flags and (backgrounds, effects) its four colour chips, which step through `F0-FE` when clicked (each, or all four of a choice). A right click empties the scene under the pointer. GO plays the song with the scene playing lit (`21E9`) and a light flashing (`22B3 + n`, every 0.5 s here; every 13000 polls in the original); the VCR lever (`22B4 + n`) previews (the player's mode 1-3, the picture moved 8 right and 1 up on black). Leaving asks for the video's name once.
  - **Checked against the original under winevdm** (FRED.VID loaded from the hard drive): the front room and the three makers are pixel-identical (the colour chips and Edison's chair frame too, from the original's `rand()`: below); picking up, dropping, a right click, GO, STOP and a preview match.
- **Checked against the original** (winevdm, `tools/reference/otvdm.ps1` scripts and `screendiff.py`): see `TODO.md` for what has been. What's learned there:
  - The palettes in segment 63 are B, G, R (they go to the `RGBQUAD`s as they are, `f43_0186`), so tables compared or copied byte for byte with them are B, G, R too: Edison's looks (`f24_0c0c` reverses the R, G, B tables into them) and the dialogs' 16 EGA colours (`f27_0054`, whose bytes are signed chars: EGA blue comes out bright green on Sound FX's backdrop).
  - `f37_0320` copies a screen leaving colour 0 out (`[313E]`, never changed): the file dialogs show their panel that way, so the icons' checked shadows let the screen behind show (stale pixels of the chooser included).
  - The original refreshes the window short by a column and a row: `f37_29e4`'s lines leave their last pixel out (`WinGBitBlt` of x1 - x0 by y1 - y0), and so does the copy after the credits. The window then shows the old colour there (a pressed button's bevel corners, the screen's last row and column after the credits) until something redraws it; the port draws them.
  - The Drum Clinic's pointer snapping (`06:288b`) moves the game's own pointer (`[3BA2]`) onto a row and a step; grid clicks read the press (`[3BA8]`) and no live widget is under the grid, so it has no effect and isn't ported.
  - The widget poll (`f34_0fb6`) takes a press from the latch (`[3BA7]`, set by `WM_LBUTTONDOWN` in `g47_00aa`) or from the button still being held (`[3BA6]`), and hit-tests where the button went down (`[3BA8]`). So a click that ends something else and is still held when the next screen polls presses what it was on there: the click on EDIT that stops a video (`f18_22a4` stops on the latch and leaves it) opens "What do you want to do?" at once, as the front room only clears the latch. The port does the same (`Platform::lastPress`).
  - Colour 255: `f19_0082` sets entries 0 and 255 of the screen's palette to black and white but sends only 1-254 to the display (`f43_0694`: `SetPaletteEntries` and `WinGSetDIBColorTable` of 1-254), after each activity has put its backdrop's whole palette in the DIBs' colour tables. On a 256-colour display entry 255 is Windows' static white, as the port draws it; on a true-colour one (winevdm) WinG shows the colour table's 255, the backdrop's. Only the Drum Clinic's backdrop (`100B`) has something else there (D7 D7 D7, grey), so its kit buttons' colour 255 is grey under winevdm and white in the port, as the Wild Science panel's (`TODO.md`).
  - The Music Library's end of a piece: GO sends the piece and the loop checks channel 0 (`SSTATUS`) at once; the driver only starts what was sent at its next tick. Under winevdm the loop comes round before that tick, so every piece is over at once (STOP lit); on the machines of the time a tick came first. The port waits for the driver to have started the piece.
  - Two things the original doesn't pace: Sound FX's list arrows step a row each poll (16 rows in 0.3 s under winevdm today; the port steps every 0.1 s), and the video makers' playing light flips every 13000 polls (about every 0.3 s under winevdm; the port every 0.5 s).
  - The CD: `f27_01ea` asks MSCDEX (`int 2Fh`, `AX=150Bh`) whether a drive is a CD-ROM, which winevdm answers from Windows, so the original finds the CD's `\RB` folder (the instruments' and the effects' WAVs) when the CD image is mounted (`Mount-DiskImage`). The Instrument Room then matches the port pixel for pixel, its waveforms included.
  - `tools/testing/rbcompare.py` plays timelines of clicks and holds in each activity in both (the original through `WINMSKIP.EXE`, `tools/reference/winmain_skip.py`: straight into an activity, Edison's look read). With the CD mounted, the differences left are colour 255, the bevel corners, Edison's chair frame where a box stops his flashing (the original's frame is the draw of the moment, which moves from run to run with the click against its 3-a-second timer) and, with the music running in real time, a frame or two near a scene change; and the list's arrows above.
  - **Random numbers:** every choice takes the C library's `rand()` (`f36_0ec2`: `seed = seed * 343FDh + 269EC3h`, `(seed >> 16) & 7FFFh`), its seed at `DS:3088` from 1; `srand` (`g36_0eab`) is never called, and `WINMSKIP.EXE` and the port's `--level` both skip the intro (whose roll, `f05_04d8`, would draw first). Its callers: the Studio's sign (`f04_0824`), the intro's roll, the colour chips (`f16_217c`, `f17_20f2`), the video player (`f18_*`), the Music Library's quill (`f30_1780`). The port's `rbRand()` is the same generator (it had used `std::mt19937`). Compared draw for draw (`tools/testing/rbrng.py`: the seed followed by `memwatch.py`, the port's `EDISON_RNGLOG`): in `studio-edit` every seed the original showed is in the one sequence from 1, its bursts as the port's (the sign's single draws, the 14 and 15 of the makers, the pairs after), 123 draws against 121 at the end (the sign's flashes in time); in `library` the quill's pause draws the port's first five, in order, 0.2 s apart. `studio-edit`'s front room and "What do you want to do?" went from 237 and 249 pixels to 0; no other scenario changed. (`WINMSKIP.EXE`'s data segment moves as an activity loads, and in the library again some seconds in: `rbrng.py --delay`, or peeks, which find it again each time.)
- **Timers:** the library's countdowns tick in timer slot 9, so the games' own periodic timers use slots 5-8.
- **Sounds:** `f27_020e` plays `<CD>\RB\<name>.wav` by id (`DS:278C`, id - `0x6000`), else `<name>.wav` in the game's folder (the player's own); longer than 64 KB plays nothing.

### The audit: functions the port doesn't cite

What `tools/testing/deadscan.py rockbach` leaves (the game's own code,
segments 1-35), and why. The rest is cited where the port does it: the
driver timers (`setDriver`), the sound-address helpers of segments 9-12
and 20 (`GETADDR`), the pattern files, the scroll bar's segment 23, the
Sound FX settings and file calls, and segment 28's helpers in
`artech/game.cpp` (`f28_0000` drawLogo, `f28_011a` drawOpaque, `f28_0212`
fill, `f28_02e4` bmfill_poly, `f28_030a` drawScaledCentred, `f28_04e4`
text, `f28_05f8` saveArea, `f28_06d0` restoreArea: MALL's segment 6).

- **Windows' side** (segment 1), which the platform layer replaces:
  `f01_0000` (WinMain: the message loop, `PeekMessage`, a quit on
  `WM_QUIT`), `f01_00f6` (the window class and a 640 x 400 window, centred),
  `f01_027c` (empty, on the way out), `f01_028e` (the idle call: `f33_0422`,
  the whole game, unless the window is iconic and inactive), `f01_0330`
  (the window procedure, `RegisterClass`'s `01:032E`: keys to `g45_0000`,
  the mouse to `g47_00aa`, `WM_ACTIVATEAPP` into `[6562]`, the palette
  realised, MCI and wave notifications, the system menu's items), its
  `WM_SIZE` and `WM_COMMAND` handlers `f01_0664` (empty) and `f01_0690`
  (`GetMenu`, then nothing). Unused: `f01_02d0` (an About box's dialog
  procedure: no `DialogBox` calls it) and `f01_0676` (returns 0).
- **Timer callbacks nothing installs** (each sets a flag or counts; the
  installed ones are `g03_1770`, `06:1B82`, `07:1824`/`183A`/`1868`,
  `08:0016`/`088C`, `g25_0000`, `f02_0000`-`f02_0058`): `f03_175a`
  (`[8E1E]`), `f03_1790` (`[52A6]`), `f06_1bbe` (`[8CA2]`), `f06_1bd4`
  (`[8B4A]`), `f06_1bea` (`[8E14]`), `f07_1856`, `f07_1884` (empty),
  `f15_1c04` (`[8BCC]`), `f18_00b4` (`[86E2]`, `[86F4]`), `f18_03e2`
  (`[86F4]`), `f18_226e` (`[674C]`), `f29_1796` (`[86FA]` + 1), `f29_17ac`
  (empty), `f30_0b82` (`[8E1F]`); and the counters of `[8E36]`:
  `f06_0000`, `f07_1032`, `f08_0000`, `f15_1bee`, `f17_20dc`, `f24_041c`,
  `f26_1ec6`, `f29_0db0`, `f30_0b6c`. (`f06_0000` and `f08_0000` show as
  reached only by deadscan's near-pointer rule: a `push 0` is an immediate
  equal to their offset. So are `f27_0000` and `f32_0000`.)
- **Empty functions the game calls** (no effect): `f16_2158`, `f16_216a`
  (by the song maker and the three video makers), `f29_17be`, `f29_17d0`
  (by Sound FX, after a load and elsewhere), and, never called, `f32_0000`,
  `f32_0012`, `f32_0024`, `f32_0036`, `f32_005e`, `f33_034e`.
- **Errors that don't happen**: `f33_0360` shows an "Application Error"
  box (`f37_0000`) and quits; `f04_0d52` calls it only for a name longer
  than 0x40 (the game asks for 10 or 18 letters), and `f27_0000` (open a
  file or report "Error on opening file") is never called.
- **No effect**: `f33_0116` frees the font's memory (`[8D0A]`) on the way
  out; `f13_114e` also clears `[52A9]`, which nothing reads.
- **Never called**:
  - `f04_0f1e`: "Enter the name of your friend" in Edison's bubble at
    (136, 36), then a name (`f04_0d52`), upper-cased.
  - `f11_015c`: Harmony Hall's instruments: `INSTALL_PATCH` of each part's
    patches (`DS:1720`) on its channels.
  - `f12_01ca`: the Music Library's volume: `AC channel volume` for each of
    the piece's channels, through sound `F` (`f12_02f6`).
  - `f15_1c1a`: colours 1-9 turned a step.
  - `f24_1a3c`: a question on hallway widgets 5 and 6 (yes 1, no 0).
  - `f23_0024`: a scroll bar's record replaced in the list.
  - `g28_03ea`: segment 28's recolour (MALL's `f06_16d8`, `g37_265a`).
  - `f13_1270`: the loaded file's rate (`[69F0]`).
  - `f27_050e`, `f27_0536`: `_splitpath` (`f36_0ef6`) and `_makepath`
    (`f36_104a`) wrappers; `f31_00de`, `f31_0102` (`_llseek`), `f31_018a`
    (`tell`, `f36_0b7c`).
  - `f02_04b2`, `f02_05e6`: the `DIRECTDRUMOUT` and `PLAYINS` wrappers.
