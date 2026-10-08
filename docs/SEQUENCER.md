# Artech FM Sequencer (ADLIB*.DLL)

Reverse-engineered from `ADLIB.DLL` code segment 1 (addresses below). CADLIB,
MADLIB and ADLIB1-4 contain the same driver; only data segment 2 (songs,
patches, tables) differs. Names are our own interpretation; the reference
harness (see plan at the end) will confirm behaviour.

**Debug symbols:** the sound DLLs shipped with CodeView (NB09) debug info,
so `tools/cvsyms.py` recovers the original names of routines, labels,
variables, songs and patches (source modules: `sound.obj`, `fx.obj`,
`motor.obj`, `patch.obj`, `mtable.obj`, `adlib.obj`). `tools/disasm.py` uses
them automatically, and `tools/oplref.py` names every sound id. Our opcode
names below were chosen before the symbols were found; the original label
names are listed beside them. Key variables: `CURCHANNEL` 1E3, `DO_SOUND`
1E4, `DRUMMASK` 1E5, `GLOBALTEMPO` 1E7, `SEED` 1ED, `SOUNDTABLEPTR` 20A,
`BUFFHEAD`/`BUFFTAIL`/`BUFF` 225/227/229, `SCHNLPTR` 4D7, `OPOFFSETS` 4EB,
`FREQTABLE` 4F4. The game EXEs had their debug info stripped.

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

**Update rate.** The games register `UPDATE_ADLIB` with their timer DLL at
72 Hz. The DLL's 13 ms `timeSetEvent` callback adds 72 to an accumulator
on every tick and calls the driver whenever the total reaches 76, then
subtracts 76. The result is 72 updates per 76 timer ticks, about
72.9 Hz, unevenly spaced. `FmRenderer` reproduces this schedule exactly.

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

| Op | Our name | Original name | Params | Effect |
|---|---|---|---|---|
| 00 | SETLOOP | SETLOOP | n | loop counter = n |
| 01 | LOOP | TESTLOOP | lo, hi | if `--counter` jump to `hi:lo`, else continue |
| 02 | START | STARTVOICE | id | start another sound id |
| 03 | EARLYOFF | SETGATETHRESHOLD | n | key-off when `n` ticks remain (default 1) |
| 04 | JUMP | BRANCH | lo, hi | jump to absolute address |
| 05 | CALL | CALLSTRING | *1-byte*, u16 addr | push return, jump (per-channel stack) |
| 06 | RET | STRINGRETURN | *1-byte* | pop return |
| 07 | OCTAVE | OCTAVEOFFSET | n | octave transpose (units of 0x10) |
| 08 | END | EOS | | stop channel, key-off. Also the handler for all undefined opcodes |
| 09 | REST | REST | n | wait n ticks, key-off |
| 0A | WRITEREG | SETOPLREG | reg, val | raw OPL register write |
| 0B | TIE | NEWNOTE | note, dur | change pitch without re-keying (legato) |
| 0C | TRANSPOSE | NOTEOFFSET | n | semitone transpose |
| 0D | REGLFO | TABLEMODOP | rate, len, reg, u16 table | cycle a register through a table |
| 0E | STOPCH | STOPPARSE | ch | stop channel `ch` |
| 10 | PATCH | STUFFPATCH | n | load instrument `n` from `[0x6C1]` (11 bytes of operator regs) |
| 11 | SLIDE | PITCHDELTA | speed, s16be delta | pitch slide |
| 12 | SLIDEOFF | CLEARPITCHDELTA | *1-byte* | |
| 13 | DETUNE | FRACPITCH | n | F-number offset |
| 15 | VIBRATO | VIBRATO | rate, depth, halfperiod, delay | |
| 1A | PRIORITY | SETPRIORITY | n | |
| 1C | BARSET | INITSYNC | n | global bar counter (sync between channels) |
| 1D | BARWAIT | WAITSYNC | mask | wait for global counter bits |
| 1E | VOL | SETACCENTATTN | n | channel attenuation A |
| 20 | HOLD | STAY | dur | wait without key-off |
| 21 | RETRIG | RETRIGGER | dur | re-key current pitch |
| 24 | GATE | NOTEPERCENT | n (0-7) | key-off after n/8 of the duration |
| 26 | GTEMPO | SETMUSICTEMPO | n | global tempo |
| 27 | REGLFOOFF | KILLVECTOR2 | *1-byte* | |
| 29 | TEMPO | SETTEMPO | n | channel tempo |
| 2B | VOLB | SETCHANNELATTN | n | channel attenuation B |
| 2C | CHVOL | SETSYSATTN | ch, v | set channel `ch` attenuation C |
| 2D | CHVOLADD | SETSYSATTNDELTA | ch, d | add to channel `ch` attenuation C |
| 2E | AMDEPTH | SETAMDEPTH | b | reg BD bit 7 |
| 2F | VIBDEPTH | SETVIBDEPTH | b | reg BD bit 6 |
| 30 | VOLADD | SETSIGNEDATTN | n | attenuation A += n |
| 33 | KILL | KILLCHANNEL | ch | stop and silence channel `ch` |
| 35 | RANDPITCH | RANDOMPITCH | mask | random F-number offset |
| 36 | VIBOFF | KILLVECTOR1 | *1-byte* | |
| 39 | BEND | SETPITCHBEND | n | pitch bend via per-semitone tables `[0x0004 + 2*semi]` |
| 3A | SYNCTEMPO | GETMUSICTEMPO | *1-byte* | tempo = global tempo |
| 3B | NOP | SNOP | *1-byte* | |
| 3C | HUMANIZE | RANDOMDURATION | mask | duration += random & mask |
| 3D | TEMPOADD | TEMPODELTA | s8 | clamped to 1..255 |
| 3F | MOTOR | MOTORON | mode, table | |
| 40 | MOTOROFF | MOTOROFF | *1-byte* | |
| 41 | RHYTHM | DRUMSETUP | p6, p7, p8, 6 bytes | load drum patches for ch 6-8, set B6/A6/B7/A7/B8/A8, enable rhythm mode |
| 42 | DRUMS | DODRUM | mask | key drums in reg BD |
| 43 | RHYTHMOFF | DRUMOFF | *1-byte* | |
| 44 | DRUMVOL | DRUMATTN | mask, v | set drum attenuation |
| 45 | DRUMVOLADD | DRUMFADE | mask, v | accumulate drum attenuation |
| 46 | DRUMVOLBASE | DRUMMASTERATTN | mask, v | set base drum attenuation |
| 48 | TEMPOLINK | SETAUTOTEMPO | b | follow global tempo every tick |
| 49 | VELSCALE | SETVELOCITY | mod, car | enable velocity bytes after notes |
| 50 | EVENT | (unnamed) | n | push game event `n` |
| 51 | EVENTCH | (unnamed) | *1-byte* | push `0xA0 | channel` |
| 52 | EVENTDRUM | (unnamed) | *1-byte* | push `0x80 | drums` |

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

1. **Reference harness** (done, `tools/oplref.py`): run the original driver under a CPU emulator,
   apply NE relocations (DS loads such as `mov ax, 0x180B` are fixup chains),
   trap `out 0x388/0x389`, and record OPL register writes per tick for every
   sound id.
2. **Native sequencer** (done, `engine/src/audio/artech_fm_driver.cpp`):
   readable C++ over a copy of the data segment (song data uses absolute
   addresses into it), byte-exact with the original.
3. **Differential tests** (done, `tools/oplfuzz.py`): the game's songs only
   use 37 of the 57 opcodes, so synthetic songs covering all of them are
   injected into unused memory (F000+) and run through the original under
   Unicorn. `seqtest --cases` replays them natively. `tools/statediff.py`
   plus `seqtest --trace` pinpoint the first tick and variable where the two
   diverge.
4. **Audio** (done, `fmplay`): feed the register stream to a software OPL2 (Nuked-OPL3 or DOSBox OPL).

## Original bugs reproduced by the port

These change the output, so the port keeps them (all verified against the
original under emulation):

- **DOPITCHDELTA stack bug:** when an octave step of a pitch slide leaves an
  F-number of 0, the routine pops one word too many. Its own return address
  becomes the new F-number (low 10 bits, e.g. `0x0C8` in ADLIB, `0x0D2` in
  CADLIB) and its RET exits the whole tick, skipping the second effect and
  all lower channels.
- **WRITEPATCH operator offset:** with velocity scaling on, the level
  routines overwrite CL (the operator offset) with their shift count, so the
  carrier level and envelope registers go to the wrong operator.
- **Gate check:** after a gate key-off, the early-key-off comparison uses the
  B0 value just written instead of the remaining tick count.
- **Velocity bytes:** a note carries a velocity byte whenever either
  velocity shift *or the last velocity* is non-zero (word tests at +32/+33).
- **silenceVoice** keys the voice on again (B0 = 0x20) after silencing it.
- **DODRUM** writes register BD twice after a single latch.
- **GFLUSH** never clears the queue (its loop index doesn't advance).
- **SENDSND** stores words at byte indexes (entries overlap).

Per-DLL differences handled by detecting addresses in each DLL's code: the
sound/patch/motor tables, the GE_FLG counter (absent in CADLIB/MADLIB/SADLIB), and
the effect-routine code addresses that channels store in their data.
SADLIB (Wild Science) is an older build of the same driver: its code in
segment 2 (Borland C++ wrappers in 1), its data in 3, and every driver
variable 0x4F8 bytes further on (the bend tables' pointers at 4F8, not 0);
the driver finds that base from the tick's `mov byte [CURCHANNEL],9` and
moves its variables by it (see docs/SCIENCE.md).

Not reproduced: reading past offset FFFF of the data segment (the original
faults on real hardware), and opcodes 53-70, which jump into unrelated code
(treated as EOS; the four CADLIB songs that use them still match).
