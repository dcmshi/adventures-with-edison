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
- **Hallway:** backdrop `1007`; clicks read the mask `1006` on screen 2 (`f37_23ce`). Edison's greeting, the sign, the credits and the quit question aren't ported yet; the door leaves at once.
- **Activities:** none yet; each switches its FM driver and comes back.
- **Sounds:** `f27_020e` plays `<CD>\RB\<name>.wav` by id (`DS:278C`, id - `0x6000`), else `<name>.wav` in the game's folder (the player's own); longer than 64 KB plays nothing.
