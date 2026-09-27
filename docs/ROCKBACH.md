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
  then FM driver 4, `f20_0000`, the Rock and Bach Studio logo (`f05_04d8`,
  `1001`).
- Then a loop over the hallway (`f24_1d4a`, backdrop `1007` with its
  hot-spot mask `1006`). It returns the activity, and each activity switches
  the FM driver first (`f02_006e(n)`):

| Result | Driver | Activity | Entry | Backdrop |
|---|---|---|---|---|
| 1 | | leave: `WinExec("edison.exe -O")` | | |
| 2 | 0 (`ADLIB`) | the jukebox: play the bands' songs | `f03_22e8` | `1000` |
| 3 | 1 (`ADLIB1`) | the Drum Clinic | `f06_1e9e` | `100B` |
| 4 | 3 (`ADLIB3`) | the Music Library (composers) | `f30_1780` | `100D` |
| 5 | | a new player (setup again) | | |
| 6 | 2 (`ADLIB2`) | Harmony Hall (keys, chords, styles) | `f07_1908` | `100C` |
| 7 | none | the Instrument Room (`instinfo.hi`) | `f08_0f2e` | `1009` |
| 8 | none | Sound FX (sample, echo, reverb) | `f29_17e2` | `1008` |
| 9 | 4 (`ADLIB4`) | the Studio (`f35_018a`): videos | `f04_112e` | `1004` |

- The Studio's parts: the band maker (`f14_10f4`, `1002`), the song maker
  (`f15_1f84`, `1003`), the video makers (`f16_217c`, `f17_20f2`,
  `f26_1edc`, `1005`) and the player (`f18_22a4`).
- The hallway also has the credits (`f24_12e6`, `100E`) and "Do you want to
  quit this game?" (`f24_0f8c`). The player's file is `user.yyy`; `ed.yyy`
  keeps Edison's look.

## The FM drivers (segment 2)

`[83D0]` is the driver in use (-1 none, 0-4 `ADLIB`, `ADLIB1`-`ADLIB4`).
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

The port's driver (`engine/src/audio/artech_fm_driver`) has `INIT`, `REMOVE`,
`UPDATE`, `SENDSND`, `GEVENT` and `GFLUSH` so far.

## Port status

Not started: the map above is the plan.
