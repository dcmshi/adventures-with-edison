# File Format Notes

## Original platform
- 16-bit Windows 3.1 (NE executables), Borland C++ 1994
- Graphics via WinG; FM music via direct OPL2 register writes (`ADLIB*.DLL`: `SetOplReg`, `OutAdlib`)
- Data compression: PKWARE Data Compression Library (1990-91)

## Files
### Archives: `*.D01`, `GRAFX.DAT` (SOLVED, `tools/extract.py`)

```
[ASCII title, optional]  e.g. "BITWIT Data File

" (SHELL), "Demo Data File

" (RB),
                         "MALL Data File

" (MYSTERY); GRAFX.DAT has none
0x1A                     terminator
u16  count
count x 14-byte entries:
    u16 id               high byte = resource group, low byte = index
    u32 offset           absolute file offset
    u32 packed_size
    u32 unpacked_size
data                     each entry is a PKWARE DCL implode stream
                         (header 00 06 = binary / 01 06 = ASCII literals, 4 KB dictionary)
```

| Archive | Game | Entries | BMP | WAV | Other |
|---|---|---|---|---|---|
| SHELL.D01 | Main menu | 485 | 430 | 14 | 41 |
| RB.D01 | Rock and Bach | 1084 | 1066 | 1 | 17 |
| MYSTERY.D01 | Mystery Mall | 893 | 846 | 1 | 46 |
| GRAFX.DAT | Wild Science Arcade | 1321 | 1120 | 45 | 156 |

All bitmaps are standard 8-bit uncompressed Windows BMPs (full screens are 640x400).

### Non-bitmap archive entries (by id group)

| Group | Contents | Notes |
|---|---|---|
| `01` | Bitmap fonts | `u8 bytes_per_row, u8 height, u8 first_char (0x20), u8 last_char (0x7F)`, then 96 width bytes, then 96 x height x bytes_per_row bitmap rows. Size checks out exactly for all fonts. |
| `02`, `30` | Palettes | 768 bytes = 256 x RGB, 8-bit values (VGA 6-bit << 2) |
| `31`, `3f`, `70`-`76` | Text | Dialogue, quiz questions and answers (plain ASCII) |
| `03` | Scripts (SHELL, 20) | Compiled command lists; see [Scripts](#scripts-shelld01-group-03) |
| `50` | Animations (SHELL, 20) | Frame lists; see [Animations](#animations-shelld01-group-50). In GRAFX.DAT group 50 is bitmaps |
| `60` | Unused data (MYSTERY: `6000`, `6001`) | Not loaded: MALL.EXE never names either id. Each starts with four words (31, 45, 132, 12 and 8, 6, 24, 6), then signed byte pairs (±1, ±4, ...). In GRAFX.DAT and RB.D01 group 60 is WAVs |
| `ff02` | IFF `FORM....PBM ` | Deluxe Paint header stub (GRAFX), likely leftover |

### Files on the CD

| File | Contents |
|---|---|
| `*.HI` | Help/info text for Rock and Bach (songs, instruments), `! n n` headers |
| `*.PAT` | Probably Rock and Bach beat patterns (bitmask values), **not** FM patches. `NONAME.PAT` = empty 512-byte pattern |
| `FMDRIVER.TXT` | FM MIDI drivers the game disables in SYSTEM.INI so it can drive the OPL chip itself |
| `S0.SRF`-`S110.SRF` | Wild Science Arcade's rooms (111): see [Rooms](#rooms-snsrf) |
| `WSCIENCE.HS`, `MYSTERY.HS` | High scores: see [High scores](#high-scores-hs) |
| `*.VID` | Rock and Bach's videos (618 bytes): see [Videos](#videos-vid) |

## Data formats

### Scripts (SHELL.D01 group 03)

`u16 count, u16 offset[count]`, then records of `u16 number, u16
statements, u16 code_size, u16 statement_offset[statements], code`. A
statement is `u16 command, u16 present` and 2-byte argument slots (slot k
at `+4 + 2k`, given if bit k of `present` is set). The 26 commands and the
interpreter are in [GAME.md](GAME.md#script-interpreter-shelld01-group-03);
`tools/scripts.py` decompiles them.

### Animations (SHELL.D01 group 50)

```
u16 frames
s16 x, s16 y            the anim's position (a STARTANIM x, y of -1 uses it)
u16 ?
frames x 7 bytes:
    s16 dx, s16 dy      drawn at (x + dx, y + dy)
    u16 bitmap          archive id
    u8  ticks           how long it shows, in game ticks (16 Hz)
```

Played by `EDISON.EXE` segment 8 (ported: `engine/src/artech/anims.*`).

### Rooms (`S<n>.SRF`)

Wild Science Arcade's tables, ASCII numbers separated by spaces, in three
parts (read by `WMAIN.EXE` `f27_0ad8`; ported: `Science::loadTable`):

1. **The shape**, a tree of boxes. A box is two rectangles `x y w h` (its
   top, at its height, then its bottom, at the parent's height; so sides
   can slope) and its height; then, for each child, the byte `01` and the
   child box; then the byte `02`. Coordinates are the world's (the
   floor is the root, `0 0 809 789`); a box lower than its parent is a
   pit.
2. **The objects**, at most 24 lines of `OBJn x y type a b c d e f`
   (holes, targets, magnets, switches, blocks ...; the types and their
   arguments are in [SCIENCE.md](SCIENCE.md#the-rooms-snsrf)).
3. **`PANEL a b c d e f g h END`**: four (flags, value) pairs for the
   controls under the table (gravity, friction, power, ball type; flag 1
   locks a control, 2 hides it).

### High scores (`*.HS`)

- **`MYSTERY.HS`** (Mystery at the Museums, 1170 bytes): nine tables
  (levels 0-7, then custom levels) of ten 13-byte entries: the name (9
  bytes, NUL-padded) and the score (s32, little-endian). A missing file is
  made empty.
- **`WSCIENCE.HS`** (Wild Science Arcade, text): `HSFILE `, then one line
  per game, ` name score level screen l0 l1 l2 l3 ` and CR LF: the name
  with its spaces as dots, the level and screen it ended on (from the room
  number by the table at `WMAIN.EXE DS:26F6`), and the player's look
  (four 0-7 choices). Up to 50 games are read, sorted by score. The
  original appends a broken line (an empty record, `     0 0 0 0`) each
  time it saves; the port leaves it out. The CD's copy is the starting
  table.
- Rock and Bach keeps no high scores; its players are in `user.yyy` and
  the look in `ed.yyy` (see [ROCKBACH.md](ROCKBACH.md)). Wild Science
  Arcade's look is in `wscience.edi` (four numbers, each followed by
  ` \n`).

### Videos (`*.VID`)

A Rock and Bach video: the Studio's 0x26A-byte record (`WINMAIN.EXE`
`DS:6754`) written as is: the band (a player for each of the four roles),
the song (16 slots of track and style), the tempo, and per slot the
background, the special effect and the camera view, with their flags and
colours, then the names of the video, the song, the band and the
producer. The field-by-field table is in
[ROCKBACH.md](ROCKBACH.md) (the Studio). The CD has samples in
`\RB\EFFECTS\` and two in `DSK3`.

## FM music driver (`ADLIB*.DLL`, `CADLIB`, `MADLIB`, `SADLIB`)

A DOS-style sound driver ported to a Win16 DLL. Disassemble with `tools/disasm.py`.

- **Hardware access:** writes OPL2 directly on port `0x388` (helper at `ADLIB.DLL 1:17BE`: `AH` = register, `AL` = value). Uses rhythm mode (register `0xBD`).
- **Timer:** hooks IRQ0 via `int 21h / AX=3508h` and reprograms the PIT (ports `0x40`/`0x43`) to clock the sequencer. Also drives the PC speaker (port `0x61`) for digitised samples.
- **Sequencer:** bytecode interpreter, main loop at `ADLIB.DLL 1:0C5E`. Reads a u16 per step from the channel pointer (`[di+3]`):
  - `AL` bit 7 clear: note event
  - `AL` bit 7 set: command `AL & 0x7F` (up to `0x70`), dispatched via a near-pointer jump table (`cs:1AEE` in ADLIB.DLL)
- **Music data** lives in data segment 2 of each DLL (ADLIB 58 KB, CADLIB 19 KB, ADLIB1 5.6 KB, MADLIB 7 KB).
- ADLIB, ADLIB1-4, CADLIB, MADLIB share the same driver code (tables at nearly identical offsets); SADLIB (Wild Science) is a smaller variant.
- Exports: `INIT_ADLIB`, `UPDATE_ADLIB`, `SENDSND`, `SSTATUS`, `SWITCHSOUNDTABLE`, `INSTALL_PATCH`, `PLAYINS`, `DIRECTDRUMOUT`, `ABORTSAMPLE`, ...

### Plan
1. Decode the command set from the jump-table handlers.
2. Verification harness: run the original driver code under a CPU emulator (e.g. Unicorn), trapping `out 0x388/0x389` to record a reference OPL register log per song.
3. Write a native sequencer; it must reproduce the reference log exactly, then feed a software OPL (Nuked-OPL3 / DOSBox OPL).
