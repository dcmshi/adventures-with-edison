"""Reads the ORIGINAL game's memory while it runs under winevdm (otvdm), to
check the port's state against it (positions, velocities, counters).

The games keep their data, and (in WMAIN.EXE) all their objects, in the
program's automatic data segment (DGROUP: near pointers). winevdm runs the
16-bit program inside otvdmw.exe, where that segment is a plain 64 KB block
of the process's memory; it's found by searching for the executable's own
static data (its longer strings), so no addresses have to be known.

    memwatch.py find EXE
        print where DGROUP is (and the process)
    memwatch.py peek EXE EXPR...
        print each expression's value once
    memwatch.py watch EXE [--every MS] [--for S] [--out FILE] EXPR...
        poll and print a line (milliseconds since the start, then each
        value) whenever a value changes
    memwatch.py dump EXE FILE
        save the whole of DGROUP (64 KB) to FILE

EXE is the program's file (its DGROUP's initial bytes are read from it):
for example D:/tools/edison-run/WMAIN.EXE.

Expressions read DGROUP: `[x]` is the word at DS:x (unsigned), so pointers
can be followed; a prefix picks how the final address is read: `w:` a
signed word (the default), `u:` unsigned, `b:` a byte, `d:` a signed
dword. Numbers are hex. A name may be given with `name=`. For example, in
WMAIN.EXE (the player object is at DS:5FFC, its room at +AE, the room's
ball at +F77; a ball's +0 is its motion part, whose sphere is at +2, and
its +2 its core, whose velocity is at +62):

    vx=w:[[[5ffc+ae]+f77]+2]+62
    cx=w:[[[5ffc+ae]+f77]]+2

Only reads; the game isn't touched.
"""
import argparse
import ctypes
import ctypes.wintypes as wt
import re
import struct
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from ne import NEFile  # noqa: E402

PROCESS_VM_READ = 0x10
PROCESS_QUERY_INFORMATION = 0x400
MEM_COMMIT = 0x1000
PAGE_NOACCESS = 0x01
PAGE_GUARD = 0x100
TH32CS_SNAPPROCESS = 0x2

kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)


class MEMORY_BASIC_INFORMATION(ctypes.Structure):
    _fields_ = [("BaseAddress", ctypes.c_void_p), ("AllocationBase", ctypes.c_void_p),
                ("AllocationProtect", wt.DWORD), ("PartitionId", wt.WORD), ("RegionSize", ctypes.c_size_t),
                ("State", wt.DWORD), ("Protect", wt.DWORD), ("Type", wt.DWORD)]


class PROCESSENTRY32W(ctypes.Structure):
    _fields_ = [("dwSize", wt.DWORD), ("cntUsage", wt.DWORD), ("th32ProcessID", wt.DWORD),
                ("th32DefaultHeapID", ctypes.c_void_p), ("th32ModuleID", wt.DWORD), ("cntThreads", wt.DWORD),
                ("th32ParentProcessID", wt.DWORD), ("pcPriClassBase", ctypes.c_long), ("dwFlags", wt.DWORD),
                ("szExeFile", ctypes.c_wchar * 260)]


kernel32.OpenProcess.restype = wt.HANDLE
kernel32.VirtualQueryEx.argtypes = [wt.HANDLE, ctypes.c_void_p, ctypes.POINTER(MEMORY_BASIC_INFORMATION), ctypes.c_size_t]
kernel32.VirtualQueryEx.restype = ctypes.c_size_t
kernel32.ReadProcessMemory.argtypes = [wt.HANDLE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
kernel32.CreateToolhelp32Snapshot.restype = wt.HANDLE
kernel32.Process32FirstW.argtypes = [wt.HANDLE, ctypes.POINTER(PROCESSENTRY32W)]
kernel32.Process32NextW.argtypes = [wt.HANDLE, ctypes.POINTER(PROCESSENTRY32W)]


def find_processes(name="otvdmw.exe"):
    snap = kernel32.CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)
    entry = PROCESSENTRY32W()
    entry.dwSize = ctypes.sizeof(entry)
    pids = []
    ok = kernel32.Process32FirstW(snap, ctypes.byref(entry))
    while ok:
        if entry.szExeFile.lower() == name:
            pids.append(entry.th32ProcessID)
        ok = kernel32.Process32NextW(snap, ctypes.byref(entry))
    kernel32.CloseHandle(snap)
    return pids


class Process:
    def __init__(self, pid):
        self.pid = pid
        self.handle = kernel32.OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, False, pid)
        if not self.handle:
            raise OSError(f"can't open process {pid} (error {ctypes.get_last_error()})")

    def regions(self):
        mbi = MEMORY_BASIC_INFORMATION()
        address = 0
        while kernel32.VirtualQueryEx(self.handle, ctypes.c_void_p(address), ctypes.byref(mbi), ctypes.sizeof(mbi)):
            base = mbi.BaseAddress or 0
            if mbi.State == MEM_COMMIT and not (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)):
                yield base, mbi.RegionSize
            address = base + mbi.RegionSize
            if address >= 1 << 47:
                break

    def read(self, address, size):
        buf = ctypes.create_string_buffer(size)
        got = ctypes.c_size_t()
        if not kernel32.ReadProcessMemory(self.handle, ctypes.c_void_p(address), buf, size, ctypes.byref(got)):
            return None
        return buf.raw[:got.value]


def signatures(data, count=4, length=32):
    """Offsets of a few long printable runs in the segment's initial data."""
    runs = [(m.start(), m.end()) for m in re.finditer(rb"[\x20-\x7e]{%d,}" % length, data)]
    runs.sort(key=lambda r: r[0] - r[1])  # longest first
    picked = []
    for start, _ in runs:
        if all(abs(start - p) > 256 for p in picked):
            picked.append(start)
        if len(picked) == count:
            break
    return [(p, data[p:p + length]) for p in picked]


def dgroup_of(exe_path):
    exe = NEFile(exe_path)
    (auto,) = struct.unpack_from("<H", exe.data, exe.ne + 0x0E)  # the automatic data segment
    return exe.segment_bytes(auto)


def locate(exe_path):
    """(process, base): this program's live DGROUP in an otvdmw process. The
    executable's image is in memory too, with the same data: of the blocks
    that match, the live one is the one that differs most from the file."""
    data = dgroup_of(exe_path)
    sigs = signatures(data)
    if not sigs:
        raise SystemExit("no signature in the data segment")
    first_off, first = sigs[0]
    best = None
    for pid in find_processes():
        proc = Process(pid)
        for base, size in proc.regions():
            if size > 64 << 20:
                continue
            chunk = proc.read(base, size)
            if not chunk:
                continue
            at = chunk.find(first)
            while at >= 0:
                seg = base + at - first_off
                if all(proc.read(seg + off, len(s)) == s for off, s in sigs[1:]):
                    live = proc.read(seg, len(data)) or b""
                    changed = sum(1 for x, y in zip(live, data) if x != y)
                    if best is None or changed > best[0]:
                        best = (changed, proc, seg)
                at = chunk.find(first, at + 1)
    if best is None:
        raise SystemExit("the program's data segment wasn't found (is it running under otvdm?)")
    return best[1], best[2]


TOKEN = re.compile(r"\s*(\[|\]|\+|-|[0-9a-fA-F]+)")


def evaluate(expr, mem):
    """`w:[[5ffc+ae]+f77]+2`: [x] reads an unsigned word at DS:x."""
    kind = "w"
    if len(expr) > 2 and expr[1] == ":":
        kind, expr = expr[0], expr[2:]
    tokens = [t for t in TOKEN.findall(expr)]
    pos = 0

    def word(a):
        a &= 0xFFFF
        return mem[a] | mem[(a + 1) & 0xFFFF] << 8

    def sum_():
        nonlocal pos
        value = atom()
        while pos < len(tokens) and tokens[pos] in "+-":
            op = tokens[pos]
            pos += 1
            value = value + atom() if op == "+" else value - atom()
        return value

    def atom():
        nonlocal pos
        t = tokens[pos]
        pos += 1
        if t == "[":
            v = word(sum_())
            pos += 1  # ]
            return v
        return int(t, 16)

    address = sum_() & 0xFFFF
    if kind == "b":
        return mem[address]
    if kind == "u":
        return word(address)
    if kind == "d":
        return struct.unpack_from("<i", bytes(mem[address:address + 4]).ljust(4, b"\0"))[0]
    return struct.unpack("<h", struct.pack("<H", word(address)))[0]


def parse(specs):
    out = []
    for s in specs:
        name, _, expr = s.rpartition("=")
        out.append((name or expr, expr))
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("find"); p.add_argument("exe")
    p = sub.add_parser("peek"); p.add_argument("exe"); p.add_argument("expr", nargs="+")
    p = sub.add_parser("dump"); p.add_argument("exe"); p.add_argument("file")
    p = sub.add_parser("watch"); p.add_argument("exe"); p.add_argument("expr", nargs="+")
    p.add_argument("--every", type=float, default=2.0, help="milliseconds between reads (default 2)")
    p.add_argument("--for", dest="seconds", type=float, default=10.0, help="seconds to watch (default 10)")
    p.add_argument("--out", help="also write the lines to this file")
    args = ap.parse_args()

    proc, base = locate(args.exe)
    if args.cmd == "find":
        print(f"process {proc.pid}, DGROUP at {base:#x}")
        return
    if args.cmd == "dump":
        Path(args.file).write_bytes(proc.read(base, 0x10000) or b"")
        print(f"saved {args.file}")
        return
    exprs = parse(args.expr)
    if args.cmd == "peek":
        mem = proc.read(base, 0x10000)
        for name, e in exprs:
            print(f"{name} = {evaluate(e, mem)}")
        return
    out = open(args.out, "w") if args.out else None
    print("ms " + " ".join(n for n, _ in exprs), file=out or sys.stdout)
    if out:
        print("ms " + " ".join(n for n, _ in exprs))
    start = time.perf_counter()
    last = None
    while time.perf_counter() - start < args.seconds:
        mem = proc.read(base, 0x10000)
        if mem is None:
            break
        values = []
        for _, e in exprs:
            try:
                values.append(evaluate(e, mem))
            except (IndexError, ValueError):
                values.append(None)
        if values != last:
            line = f"{int((time.perf_counter() - start) * 1000)} " + " ".join(str(v) for v in values)
            print(line)
            if out:
                print(line, file=out)
                out.flush()
            last = values
        time.sleep(args.every / 1000)


if __name__ == "__main__":
    main()
