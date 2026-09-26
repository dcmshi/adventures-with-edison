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
| `03` | ? | Small records starting `01 00 04 00`; probably button/hotspot layouts (SHELL) |
| `50` | ? | Starts `count, 320, 200`; probably animation paths (SHELL) |
| `60` | ? | Unknown (MYSTERY) |
| `ff02` | IFF `FORM....PBM ` | Deluxe Paint header stub (GRAFX), likely leftover |

### Files on the CD

| File | Contents |
|---|---|
| `*.HI` | Help/info text for Rock and Bach (songs, instruments), `! n n` headers |
| `*.PAT` | Probably Rock and Bach beat patterns (bitmask values), **not** FM patches. `NONAME.PAT` = empty 512-byte pattern |
| `FMDRIVER.TXT` | FM MIDI drivers the game disables in SYSTEM.INI so it can drive the OPL chip itself |
| `S*.SRF` | ASCII integers; probably screen/hotspot rectangles |
| `*.HS` | Probably high-score tables |
| `*.VID` | 618-byte binary, probably animation sequences |

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
