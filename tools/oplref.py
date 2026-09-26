"""Reference OPL register logs from the original Artech sound driver.

Loads an ADLIB-family DLL into a 16-bit real-mode CPU emulator (Unicorn),
applies its NE relocations, fakes the little DOS/hardware it touches, then for
each sound id: INIT_ADLIB, SENDSND(id), and UPDATE_ADLIB once per tick,
recording every OPL register write. These logs are the ground truth our
native sequencer must reproduce.

Output: extracted/oplref/<dll>/<id>.txt, lines "tick reg value" (hex), plus
an index.txt summary. (Git-ignored: derived from game data.)

Usage: python tools/oplref.py [DLL ...] [--max-ticks N] [--ids 0,1,5] [--tempo 0x80]
"""
import argparse
import re
import struct
from pathlib import Path

from unicorn import UC_ARCH_X86, UC_HOOK_INSN, UC_HOOK_INTR, UC_MODE_16, Uc, UcError
from unicorn.x86_const import (UC_X86_INS_IN, UC_X86_INS_OUT, UC_X86_REG_AX, UC_X86_REG_BX,
                               UC_X86_REG_CS, UC_X86_REG_DS, UC_X86_REG_ES, UC_X86_REG_IP,
                               UC_X86_REG_SP, UC_X86_REG_SS)

from cvsyms import load_symbols
from ne import NEFile

ROOT = Path(__file__).resolve().parent.parent
CD = ROOT / "original" / "cd" / "DSK3"
OUT = ROOT / "extracted" / "oplref"

# Real-mode paragraph for each NE segment, plus stack and stubs.
SEG_PARA = {1: 0x1000, 2: 0x2000, 3: 0x3000}
STACK_PARA = 0x4000
STUB_PARA = 0x5000  # 0000: return sentinel (hlt); 0010+: import stubs (hlt)
MEM_SIZE = 0x60000



def detect_layout(ne):
    """Find the driver's data addresses in its own code (they shift per DLL).

    See docs/SEQUENCER.md; offsets named after ADLIB.DLL's values.
    """
    code = ne.segment_bytes(1)
    u16 = lambda b: int.from_bytes(b, "little")
    # SWITCHSOUNDTABLE: shl bx,1 / mov bx,[bx+TABLES] / mov [CUR],bx
    m = re.search(rb"\xd1\xe3\x8b\x9f(..)\x89\x1e(..)", code, re.S)
    if not m:
        raise ValueError("not an Artech ADLIB-family driver")
    tables, current = u16(m.group(1)), u16(m.group(2))
    # channel pointer table: mov di,[bx+CHAN]
    chan = u16(re.search(rb"\x8b\xbf(..)", code, re.S).group(1))
    # opcode dispatch: mov si,JUMPTABLE / mov bx,cs:[bx+si]; op 26 = GTEMPO: mov [TEMPO],ah
    jt = u16(re.search(rb"\xbe(..)\x2e\x8b\x18", code, re.S).group(1))
    op26 = u16(code[jt + 2 * 0x26:jt + 2 * 0x26 + 2])
    if code[op26:op26 + 2] != b"\x88\x26":
        raise ValueError("unexpected GTEMPO handler")
    tempo = u16(code[op26 + 2:op26 + 4])
    # STUFFPATCH: mov cl,[YAMOFF] / sub bx,bx / mov bl,ah / shl bx,1 / mov si,[bx+PATCH]
    patch = u16(re.search(rb"\x8a\x0e\xf1\x01\x2b\xdb\x8a\xdc\xd1\xe3\x8b\xb7(..)", code, re.S).group(1))
    init = next(n for n in ne.names.values() if n.startswith("INIT_ADLIB"))
    return {"sound_tables": tables, "current_table": current, "chan_table": chan,
            "global_tempo": tempo, "jump_table": jt, "patch_table": patch,
            "suffix": init[len("INIT_ADLIB"):]}


class DriverHarness:
    def __init__(self, dll):
        self.ne = NEFile(dll)
        self.layout = detect_layout(self.ne)
        self.uc = uc = Uc(UC_ARCH_X86, UC_MODE_16)
        uc.mem_map(0, MEM_SIZE)
        uc.mem_write(STUB_PARA << 4, b"\xf4" * 0x1000)
        self.imports_hit = set()
        self._stubs = {}
        for index, para in SEG_PARA.items():
            uc.mem_write(para << 4, self._relocated(index))
        self.data_base = SEG_PARA[2] << 4
        self.tick = 0
        self.log = []
        self._latch = 0
        uc.hook_add(UC_HOOK_INSN, self._on_out, None, 1, 0, UC_X86_INS_OUT)
        uc.hook_add(UC_HOOK_INSN, self._on_in, None, 1, 0, UC_X86_INS_IN)
        uc.hook_add(UC_HOOK_INTR, self._on_int)

    # --- loading -------------------------------------------------------------

    def _relocated(self, index):
        seg = bytearray(self.ne.segment_bytes(index))
        for r in self.ne.relocations(index):
            if r["kind"] == "internal":
                tseg, toff = r["target"]
                sel, off = SEG_PARA[tseg], toff
            else:
                # Imported KERNEL routine: point at a unique hlt stub so any
                # call is detected (the sound code should never make one).
                key = r["target"]
                stub = self._stubs.setdefault(key, 0x10 + len(self._stubs))
                sel, off = STUB_PARA, stub
            for site in r["sites"]:
                if r["addr_type"] == 2:
                    struct.pack_into("<H", seg, site, sel)
                elif r["addr_type"] == 3:
                    struct.pack_into("<HH", seg, site, off, sel)
                elif r["addr_type"] == 5:
                    struct.pack_into("<H", seg, site, off)
                else:
                    raise NotImplementedError(f"addr type {r['addr_type']}")
        return bytes(seg)

    # --- hardware / DOS fakes ------------------------------------------------

    def _on_out(self, uc, port, size, value, _):
        if port == 0x388:
            self._latch = value & 0xFF
        elif port == 0x389:
            self.log.append((self.tick, self._latch, value & 0xFF))
        # PIT (0x40/0x43), PIC (0x20) and speaker (0x61) writes are ignored.

    def _on_in(self, uc, port, size, _):
        return 0

    def _on_int(self, uc, intno, _):
        if intno != 0x21:
            raise RuntimeError(f"unexpected int {intno:#x}")
        ah = uc.reg_read(UC_X86_REG_AX) >> 8
        if ah == 0x35:  # get vector -> ES:BX = 0000:0000
            uc.reg_write(UC_X86_REG_ES, 0)
            uc.reg_write(UC_X86_REG_BX, 0)
        elif ah != 0x25:  # 0x25 = set vector: ignore
            raise RuntimeError(f"unexpected int 21h AH={ah:#x}")

    # --- calling into the driver ----------------------------------------------

    def call_export(self, name, *args):
        """Far-call an exported pascal routine with u16 args.

        Names are given without the per-DLL suffix (INIT_ADLIB, not INIT_ADLIB1).
        """
        seg, off = self.ne.export(name + self.layout["suffix"])
        uc = self.uc
        uc.reg_write(UC_X86_REG_SS, STACK_PARA)
        sp = 0xFFF0
        for a in args:  # pascal: push left to right
            sp -= 2
            uc.mem_write((STACK_PARA << 4) + sp, struct.pack("<H", a))
        sp -= 4
        uc.mem_write((STACK_PARA << 4) + sp, struct.pack("<HH", 0x0000, STUB_PARA))
        uc.reg_write(UC_X86_REG_SP, sp)
        uc.reg_write(UC_X86_REG_DS, SEG_PARA[3])  # DGROUP, as Windows would set
        uc.reg_write(UC_X86_REG_ES, SEG_PARA[3])
        uc.reg_write(UC_X86_REG_CS, SEG_PARA[seg])
        uc.reg_write(UC_X86_REG_IP, off)
        try:
            uc.emu_start((SEG_PARA[seg] << 4) + off, STUB_PARA << 4, count=5_000_000)
        except UcError as e:
            cs, ip = uc.reg_read(UC_X86_REG_CS), uc.reg_read(UC_X86_REG_IP)
            raise RuntimeError(f"{name}: {e} at {cs:04x}:{ip:04x}") from None
        cs, ip = uc.reg_read(UC_X86_REG_CS), uc.reg_read(UC_X86_REG_IP)
        if (cs, ip) != (STUB_PARA, 0):
            if cs == STUB_PARA:
                who = [k for k, v in self._stubs.items() if v == ip - 1]
                raise RuntimeError(f"{name}: called import {who}")
            raise RuntimeError(f"{name}: did not return (stopped at {cs:04x}:{ip:04x})")
        return uc.reg_read(UC_X86_REG_AX)

    # --- driver data ---------------------------------------------------------

    def word(self, off):
        return struct.unpack("<H", self.uc.mem_read(self.data_base + off, 2))[0]

    def byte(self, off):
        return self.uc.mem_read(self.data_base + off, 1)[0]

    def poke(self, off, value):
        self.uc.mem_write(self.data_base + off, bytes([value]))

    def sound_ids(self):
        """(startable ids, skipped ids) of the current sound table.

        The table has no stored length. It ends at the first entry that points
        below the driver's variables (sound data always lies above the
        sound-table list), or where the earliest data it points at begins.
        Entries whose header byte isn't a channel (0-9) are phrase snippets
        used by PLAYINS rather than sounds, and are skipped.
        """
        table = self.word(self.layout["current_table"])
        lowest = self.layout["sound_tables"]
        ids, skipped = [], []
        bound = 0x8000
        i = 0
        while i < bound:
            p = self.word(table + 2 * i)
            if p <= lowest:
                break
            if p > table:
                bound = min(bound, (p - table) // 2)
            (ids if self.byte(p) <= 9 else skipped).append(i)
            i += 1
        return ids, skipped

    def idle(self):
        for ch in range(10):
            di = self.word(self.layout["chan_table"] + 2 * ch)
            if self.byte(di + 5) and self.word(di + 3):
                return False
        return True


def record(dll, ids=None, max_ticks=6000, tempo=0x80):
    h = DriverHarness(dll)
    h.call_export("INIT_ADLIB")
    try:
        names = load_symbols(dll)  # the DLLs shipped with CodeView debug info
    except ValueError:
        names = {}
    outdir = OUT / Path(dll).stem.lower()
    outdir.mkdir(parents=True, exist_ok=True)
    summary = []
    skipped = []
    if ids is None:
        ids, skipped = h.sound_ids()
    for sid in ids:
        h.call_export("INIT_ADLIB")
        h.poke(h.layout["global_tempo"], tempo)
        h.log, h.tick = [], 0
        h.call_export("SENDSND", sid)
        while h.tick < max_ticks:
            h.tick += 1
            h.call_export("UPDATE_ADLIB")
            if h.idle():
                break
        ended = "ends" if h.idle() else "loops/long"
        ptr = h.word(h.word(h.layout["current_table"]) + 2 * sid)
        chan = h.byte(ptr)
        name = names.get((2, ptr), "")
        (outdir / f"{sid:03d}.txt").write_text(
            "".join(f"{t} {r:02x} {v:02x}\n" for t, r, v in h.log))
        summary.append(f"{sid:3d} {name:<14} ch{chan:<2} ticks {h.tick:5d} writes {len(h.log):6d} {ended}")
    if skipped:
        summary.append(f"skipped (not channel sounds): {' '.join(map(str, skipped))}")
    (outdir / "index.txt").write_text("\n".join(summary) + "\n")
    return summary


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dlls", nargs="*", default=["ADLIB.DLL"])
    ap.add_argument("--max-ticks", type=int, default=6000)
    ap.add_argument("--ids", type=lambda s: [int(x) for x in s.split(",")])
    ap.add_argument("--tempo", type=lambda s: int(s, 0), default=0x80,
                    help="global tempo byte the game would set (default 0x80)")
    args = ap.parse_args()
    for dll in args.dlls:
        summary = record(CD / dll, args.ids, args.max_ticks, args.tempo)
        print(f"== {dll}: {len(summary)} sounds")
        print("\n".join(summary))


if __name__ == "__main__":
    main()
