"""Differential test cases: synthetic songs run through the ORIGINAL driver.

The game's own songs only use part of the command set, so this generates
random-but-structured event streams covering every opcode, injects them into
unused memory of the DLL's data segment, and records what the original
driver (under Unicorn, see oplref.py) writes to the OPL chip. seqtest --cases
replays the same cases through the native driver and compares.

Case file format (text):
    dll ADLIB.DLL
    tempo 128
    maxticks 1500
    poke F000 00 00 90 06 ...     (hex address, hex bytes)
    send 2 5                      (sound ids queued before the first tick)
    expect
    <tick reg value lines, as in oplref>

Usage: python tools/oplfuzz.py [--count N] [--seed S] [DLL ...]
Output: extracted/oplfuzz/<dll>/case_NNNN.txt (git-ignored)
"""
import argparse
import random
from pathlib import Path

from unicorn import UC_HOOK_MEM_READ

from oplref import CD, DriverHarness

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "extracted" / "oplfuzz"

PAYLOAD_BASE = 0xF000  # unused tail of the 64 KB data segment

# Extra bytes after the command word, for commands with a parameter.
EXTRA = {0x0A: 1, 0x0B: 1, 0x0D: 4, 0x11: 2, 0x15: 3, 0x2C: 1, 0x2D: 1, 0x35: 1,
         0x3F: 1, 0x41: 8, 0x44: 1, 0x45: 1, 0x46: 1, 0x49: 1}
# Commands that are a single byte (no parameter).
ONE_BYTE = {0x06, 0x12, 0x27, 0x36, 0x3A, 0x3B, 0x40, 0x43, 0x51, 0x52}
DEFINED = [0x00, 0x02, 0x03, 0x07, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x10, 0x11, 0x12, 0x13,
           0x15, 0x1A, 0x1C, 0x1D, 0x1E, 0x20, 0x21, 0x24, 0x26, 0x27, 0x29, 0x2B, 0x2C, 0x2D,
           0x2E, 0x2F, 0x30, 0x33, 0x35, 0x36, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3F, 0x40, 0x41,
           0x42, 0x43, 0x44, 0x45, 0x46, 0x48, 0x49, 0x50, 0x51, 0x52,
           0x0F, 0x14, 0x31]  # (plus a few unassigned slots, which act as EOS)


class Song:
    """Builds one channel's event stream at a fixed address."""

    def __init__(self, rng, base, channel, sound_ids, patches):
        self.rng, self.base = rng, base
        self.sound_ids, self.patches = sound_ids, patches
        self.data = bytearray([channel, rng.choice([0, 0, 1, 5])])  # header: channel, priority
        # Velocity mode: notes carry an extra velocity byte while either
        # SETVELOCITY shift or the last velocity is non-zero (the driver
        # tests the words at channel +32 and +33).
        self.vel_shifts = (0, 0)
        self.last_velocity = 0

    @property
    def here(self):
        return self.base + len(self.data)

    def note(self, min_dur=0):
        r = self.rng
        octave = r.choice([0x20, 0x30, 0x40, 0x50, 0x60]) if r.random() < 0.9 else r.randrange(0, 0x80, 0x10)
        semi = r.randrange(12) if r.random() < 0.9 else r.randrange(16)
        dur = r.randint(max(min_dur, 1), 12) if r.random() < 0.85 else min_dur
        self.data += bytes([octave | semi, dur])
        if any(self.vel_shifts) or self.last_velocity:
            self.last_velocity = r.choice([0, 0x20, 0x40, 0x7F, 0xFF, r.randrange(256)])
            self.data.append(self.last_velocity)

    def command(self, op, param=None):
        r = self.rng
        if op in ONE_BYTE:
            self.data.append(0x80 | op)
            return
        if param is None:
            param = self.param_for(op)
        self.data += bytes([0x80 | op, param])
        if op in EXTRA:
            extra = self.extra_for(op, param)
            self.data += bytes(extra)
            if op == 0x49:
                self.vel_shifts = (param, extra[0])

    def param_for(self, op):
        r = self.rng
        if op in (0x0E, 0x2C, 0x2D, 0x33):
            return r.randrange(10)                       # a channel
        if op == 0x02:
            return r.choice(self.sound_ids + [0xFF])     # a real sound
        if op in (0x10, 0x41):
            return r.choice(self.patches)                # a real patch
        if op == 0x09 or op == 0x20 or op == 0x21:
            return r.randint(0, 10)                      # a duration
        if op == 0x3F:
            return r.choice([0, 1, 2])
        if op == 0x24:
            return r.randrange(8)
        if op == 0x49:
            return r.choice([0, 0, 1, 2, 3, r.randrange(256)])  # modulator velocity shift
        if op == 0x1D:
            return r.choice([1, 2, 4, 8])
        return r.randrange(256)

    def extra_for(self, op, param):
        r = self.rng
        if op == 0x0A:
            return [r.randrange(256)]                    # any register value
        if op == 0x0B:
            return [r.randint(0, 8)]                     # legato note duration (param = note)
        if op == 0x0D:                                   # rate given; len, reg, table
            table = self.base + r.randrange(len(self.data) + 1)
            return [r.randrange(8), r.choice([0x40, 0x43, 0x60, 0x80, 0xE0]), table & 0xFF, table >> 8]
        if op == 0x41:
            return [r.choice(self.patches), r.choice(self.patches)] + [r.randrange(256) for _ in range(6)]
        if op == 0x3F:
            return [r.randrange(6)]
        if op == 0x49:
            return [r.choice([0, 1, 2, 3])]                # carrier shift (param = modulator)
        return [r.randrange(256) for _ in range(EXTRA[op])]

    def build(self, length):
        r = self.rng
        self.command(0x10)                               # start with an instrument
        if r.random() < 0.4:
            self.command(0x49, r.choice([0, 1, 2]))      # velocity scaling on
        subroutine_calls = []
        for _ in range(length):
            x = r.random()
            if x < 0.45:
                self.note()
            elif x < 0.50:                               # a short counted loop over one note
                self.command(0x00, r.randint(1, 3))      # SETLOOP (outside the loop body)
                target = self.here
                self.note(min_dur=1)
                self.data += bytes([0x81, target & 0xFF, target >> 8])  # TESTLOOP
            elif x < 0.54:                               # call a subroutine appended at the end
                self.data.append(0x85)
                subroutine_calls.append(len(self.data))
                self.data += b"\0\0"
            elif x < 0.57:                               # forward branch over one note
                at = len(self.data)
                self.data += b"\x84\0\0"
                self.note(min_dur=1)
                self.data[at + 1] = self.here & 0xFF
                self.data[at + 2] = self.here >> 8
            else:
                self.command(r.choice(DEFINED))
        self.note(min_dur=1)
        self.data += b"\x88\x00"                         # EOS
        if subroutine_calls:
            sub = self.here
            for _ in range(r.randint(1, 3)):
                self.note(min_dur=1)
            self.data.append(0x86)                       # STRINGRETURN
            for at in subroutine_calls:
                self.data[at] = sub & 0xFF
                self.data[at + 1] = sub >> 8
        return bytes(self.data)


def patch_indices(h):
    """Patch numbers whose table entry points at real data."""
    table = h.layout["patch_table"]
    out = []
    for i in range(64):
        p = h.word(table + 2 * i)
        if p > table and p < 0xF000:
            out.append(i)
    return out or [0]


def make_case(dll, rng, sound_ids, patches, max_ticks):
    h = DriverHarness(CD / dll)
    h.call_export("INIT_ADLIB")
    tempo = rng.choice([0x40, 0x80, 0xC0, 0xFF])
    h.poke(h.layout["global_tempo"], tempo)
    table = h.word(h.layout["current_table"])

    pokes, sends = [], []
    base = PAYLOAD_BASE
    for slot, channel in zip(rng.sample(sound_ids, 3), rng.sample(range(9), rng.randint(1, 3))):
        song = Song(rng, base, channel, sound_ids, patches).build(rng.randint(15, 45))
        pokes.append((base, song))
        pokes.append((table + 2 * slot, bytes([base & 0xFF, base >> 8])))
        sends.append(slot)
        base += (len(song) + 0x3F) & ~0x3F
    for addr, data in pokes:
        h.uc.mem_write(h.data_base + addr, data)

    # A stream that runs off the end of the segment reads across 0xFFFF,
    # which faults on the real (protected-mode) machine; the emulator's
    # behaviour there is meaningless, so such cases are discarded.
    seg_end = h.data_base + 0x10000
    crossed = []
    h.uc.hook_add(UC_HOOK_MEM_READ,
                  lambda uc, access, addr, size, value, _: crossed.append(addr) if addr + size > seg_end else None,
                  None, seg_end - 2, seg_end + 2)

    h.log, h.tick = [], 0
    for sid in sends:
        h.call_export("SENDSND", sid)
    while h.tick < max_ticks:
        h.tick += 1
        h.call_export("UPDATE_ADLIB")
        if crossed:
            raise RuntimeError("stream ran past the end of the data segment")
        if h.idle():
            break
    lines = [f"dll {dll}", f"tempo {tempo}", f"maxticks {max_ticks}"]
    lines += [f"poke {a:04X} {data.hex(' ')}" for a, data in pokes]
    lines += ["send " + " ".join(map(str, sends)), "expect"]
    lines += [f"{t} {r:02x} {v:02x}" for t, r, v in h.log]
    return "\n".join(lines) + "\n"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dlls", nargs="*", default=["ADLIB.DLL", "CADLIB.DLL"])
    ap.add_argument("--count", type=int, default=150)
    ap.add_argument("--seed", type=int, default=1995)
    ap.add_argument("--max-ticks", type=int, default=1500)
    args = ap.parse_args()
    for dll in args.dlls:
        rng = random.Random(f"{args.seed}:{dll}")
        probe = DriverHarness(CD / dll)
        probe.call_export("INIT_ADLIB")
        sound_ids, _ = probe.sound_ids()
        patches = patch_indices(probe)
        outdir = OUT / Path(dll).stem.lower()
        outdir.mkdir(parents=True, exist_ok=True)
        made = skipped = 0
        for n in range(args.count):
            try:
                text = make_case(dll, rng, sound_ids, patches, args.max_ticks)
            except RuntimeError as e:  # the original crashed or misbehaved: not a usable case
                skipped += 1
                print(f"  case {n}: skipped ({e})")
                continue
            (outdir / f"case_{n:04d}.txt").write_text(text)
            made += 1
        print(f"{dll}: {made} cases, {skipped} skipped")


if __name__ == "__main__":
    main()
