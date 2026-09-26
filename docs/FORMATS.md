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

### Still unknown
- `.bin` archive entries (probably animation scripts, palettes, sound/music tables)
- `S*.SRF`: ASCII, whitespace-separated integers; looks like screen/hotspot rectangles
- `*.HS`: likely high-score tables (mostly zeros)
- `*.VID`: 618-byte binary, possibly animation sequences
- `*.PAT`: probably FM instrument patches for the OPL music

