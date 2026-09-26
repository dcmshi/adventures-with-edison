# Artech FM Sequencer (ADLIB*.DLL)

Reverse-engineered from `ADLIB.DLL` code segment 1 (addresses below). CADLIB,
MADLIB and ADLIB1-4 contain the same driver; only data segment 2 (songs,
patches, tables) differs. Names are our own interpretation; the reference
harness (see plan at the end) will confirm behaviour.

All addresses of data (`[0x…]`) are offsets into the DLL's data segment 2,
which song data also uses for absolute jump/call targets.

## Driver entry points

| Export | Internal | Purpose |
|---|---|---|
| `INIT_ADLIB` | `1B94` | select sound table 0, reset OPL (`sub_0B9D`), clear queue |
| `REMOVE_ADLIB` | `1BC7` | reset OPL |
| `UPDATE_ADLIB` | `1BD2` | per-tick entry, called by the game from a timer: motor effect, drain queue (`sub_0A37`), run sequencer tick (`sub_0BF1`) |
| `SENDSND(id)` | `0A13` | push sound id onto 16-entry ring queue (`[0x229]`, head `[0x225]`, tail `[0x227]`) |
| `SWITCHSOUNDTABLE(n)` | `0AED` | `[0x20A] = [0x6BF + 2n]` |
| `GEVENT` / `GFLUSH` | `0B0B` / `0B3B` | read / clear the 32-entry game-event ring written by ops `50`-`52` |
| `GETADDR(n)` | `0B5F` | 0 = DS, 1/2 = sound table, 3 = patch table (`0x6C1`), 4 = motor tables (`0x52F`) |
| `SETMOTOR(v)` | `0AA4` | continuous "motor" pitch effect on channel 0 |
| `INSTALL_PATCH`, `PLAYINS`, `DIRECTDRUMOUT` | `171C`, `1787`, `1749` | direct instrument play (Rock and Bach) |

## Starting a sound

`id` indexes the word table at `[0x20A]`. The target begins with a header word:
low byte = **channel** (0-9), high byte = **priority**. If `priority >=
channel.priority` the channel is reset (`sub_18EF`), `ptr = header + 2`,
`tempo = 0xFF`, `ticks = 1`. Channel `0x12` skips the operator silence.

## Timing

`sub_0BF1` visits channels 9 → 0. For each active channel (`ticks != 0`):
`acc += tempo`; only on 8-bit carry does a sequencer tick happen:
`--ticks`; when `ticks == gate` or `ticks == early_off` (and channel != 9)
the note is keyed off; at `ticks == 0` events are read until one sets a
non-zero duration. After that, the two per-channel effect routines run.

## Event stream

Events are read as a word: `AL`, `AH`.

**Note** (`AL` bit 7 clear): `AL` = pitch (`hi nibble` octave, `lo nibble`
semitone 0-11), `AH` = duration in ticks (0 = continue reading). Pitch →
F-number table `[0x4F4]` (12 words) + octave/semitone transpose + detune,
optional bend. Writes `A0+ch`, `B0+ch` with key-on. If velocity scaling is
enabled, one more byte = velocity follows and the operator levels are rewritten.

**Command** (`AL` bit 7 set): opcode `AL & 0x7F`, parameter `AH`. Commands
marked *1-byte* have no parameter (the reader steps back one byte). Extra
bytes follow the command word where listed.

| Op | Name | Params | Effect |
|---|---|---|---|
| 00 | SETLOOP | n | loop counter = n |
| 01 | LOOP | lo, hi | if `--counter` jump to `hi:lo`, else continue |
| 02 | START | id | start another sound id |
| 03 | EARLYOFF | n | key-off when `n` ticks remain (default 1) |
| 04 | JUMP | lo, hi | jump to absolute address |
| 05 | CALL | *1-byte*, u16 addr | push return, jump (per-channel stack) |
| 06 | RET | *1-byte* | pop return |
| 07 | OCTAVE | n | octave transpose (units of 0x10) |
| 08 | END | | stop channel, key-off. Also the handler for all undefined opcodes |
| 09 | REST | n | wait n ticks, key-off |
| 0A | WRITEREG | reg, val | raw OPL register write |
| 0B | TIE | note, dur | change pitch without re-keying (legato) |
| 0C | TRANSPOSE | n | semitone transpose |
| 0D | REGLFO | rate, len, reg, u16 table | cycle a register through a table |
| 0E | STOPCH | ch | stop channel `ch` |
| 10 | PATCH | n | load instrument `n` from `[0x6C1]` (11 bytes of operator regs) |
| 11 | SLIDE | speed, s16be delta | pitch slide |
| 12 | SLIDEOFF | *1-byte* | |
| 13 | DETUNE | n | F-number offset |
| 15 | VIBRATO | rate, depth, halfperiod, delay | |
| 1A | PRIORITY | n | |
| 1C | BARSET | n | global bar counter (sync between channels) |
| 1D | BARWAIT | mask | wait for global counter bits |
| 1E | VOL | n | channel attenuation A |
| 20 | HOLD | dur | wait without key-off |
| 21 | RETRIG | dur | re-key current pitch |
| 24 | GATE | n (0-7) | key-off after n/8 of the duration |
| 26 | GTEMPO | n | global tempo |
| 27 | REGLFOOFF | *1-byte* | |
| 29 | TEMPO | n | channel tempo |
| 2B | VOLB | n | channel attenuation B |
| 2C | CHVOL | ch, v | set channel `ch` attenuation C |
| 2D | CHVOLADD | ch, d | add to channel `ch` attenuation C |
| 2E | AMDEPTH | b | reg BD bit 7 |
| 2F | VIBDEPTH | b | reg BD bit 6 |
| 30 | VOLADD | n | attenuation A += n |
| 33 | KILL | ch | stop and silence channel `ch` |
| 35 | RANDPITCH | mask | random F-number offset |
| 36 | VIBOFF | *1-byte* | |
| 39 | BEND | n | pitch bend via per-semitone tables `[0x0004 + 2*semi]` |
| 3A | SYNCTEMPO | *1-byte* | tempo = global tempo |
| 3B | NOP | *1-byte* | |
| 3C | HUMANIZE | mask | duration += random & mask |
| 3D | TEMPOADD | s8 | clamped to 1..255 |
| 3F | MOTOR | mode, table | |
| 40 | MOTOROFF | *1-byte* | |
| 41 | RHYTHM | p6, p7, p8, 6 bytes | load drum patches for ch 6-8, set B6/A6/B7/A7/B8/A8, enable rhythm mode |
| 42 | DRUMS | mask | key drums in reg BD |
| 43 | RHYTHMOFF | *1-byte* | |
| 44 | DRUMVOL | mask, v | set drum attenuation |
| 45 | DRUMVOLADD | mask, v | accumulate drum attenuation |
| 46 | DRUMVOLBASE | mask, v | set base drum attenuation |
| 48 | TEMPOLINK | b | follow global tempo every tick |
| 49 | VELSCALE | mod, car | enable velocity bytes after notes |
| 50 | EVENT | n | push game event `n` |
| 51 | EVENTCH | *1-byte* | push `0xA0 | channel` |
| 52 | EVENTDRUM | *1-byte* | push `0x80 | drums` |

## Channel state (`di`, table of pointers at `[0x4D7]`)

| Off | Meaning | Off | Meaning |
|---|---|---|---|
| 00 | attenuation C | 21 | gate tick |
| 03 | data pointer (0 = idle) | 24 | detune |
| 05 | ticks remaining | 25/26 | tempo / accumulator |
| 06 | loop counter | 27/28 | shadow A0 / B0 |
| 07 | octave transpose | 29/2B | effect routine ptrs (pitch / reg-LFO) |
| 08 | priority | 2D | gate fraction |
| 09, 0A.. | call depth, return stack | 2E/2F | patch TL mod / car |
| 12 | semitone transpose | 30 | attenuation B |
| 13-16 | slide speed/acc/delta | 31 | additive (from C0) |
| 17-1F | vibrato state | 32-34 | velocity scaling, velocity |
| 20 | attenuation A | 35 | early key-off tick |
| 36 | humanize mask | 39-3F | reg-LFO state |
| 40 | tempo link | 41/42 | last note / bend index |

Operator levels: `TL = patch_TL + A + B + C - (velocity << (scale+1) >> 8)`,
clamped 0..63.

## Plan

1. **Reference harness:** run the original driver under a CPU emulator,
   apply NE relocations (DS loads such as `mov ax, 0x180B` are fixup chains),
   trap `out 0x388/0x389`, and record OPL register writes per tick for every
   sound id.
2. **Native sequencer:** implement the above; diff its register log against
   the reference until identical.
3. **Audio:** feed the register stream to a software OPL2 (Nuked-OPL3 or DOSBox OPL).
