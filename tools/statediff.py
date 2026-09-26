"""Find where the native driver's state first diverges from the original's.

Replays one differential case (from oplfuzz.py) through the ORIGINAL driver
under Unicorn, dumping driver variables and channel blocks (01E0-052F) after
every tick, then compares with a native trace from `seqtest --trace`.
Prints the first tick and the addresses (with symbol names) that differ.

Usage: python tools/statediff.py CASE_FILE NATIVE_TRACE
"""
import sys
from pathlib import Path

from cvsyms import load_symbols
from oplref import CD, DriverHarness

TRACE_START, TRACE_END = 0x1E0, 0x530


def original_trace(case):
    lines = Path(case).read_text().splitlines()
    fields = {}
    pokes, sends = [], []
    for line in lines:
        if line == "expect":
            break
        key, _, rest = line.partition(" ")
        if key == "poke":
            addr, *data = rest.split()
            pokes.append((int(addr, 16), bytes(int(b, 16) for b in data)))
        elif key == "send":
            sends = [int(x) for x in rest.split()]
        else:
            fields[key] = rest
    h = DriverHarness(CD / fields["dll"])
    h.call_export("INIT_ADLIB")
    h.poke(h.layout["global_tempo"], int(fields["tempo"]))
    for addr, data in pokes:
        h.uc.mem_write(h.data_base + addr, data)
    for sid in sends:
        h.call_export("SENDSND", sid)
    states = []
    while h.tick < int(fields["maxticks"]):
        h.tick += 1
        h.call_export("UPDATE_ADLIB")
        states.append(bytes(h.uc.mem_read(h.data_base + TRACE_START, TRACE_END - TRACE_START)))
        if h.idle():
            break
    return fields["dll"], states


def describe(addr, symbols, chan_bases):
    for ch, base in enumerate(chan_bases):
        if base <= addr < base + 0x43:
            return f"channel {ch} +{addr - base:02X}"
    best = max((a for (s, a) in symbols if s == 2 and a <= addr), default=None)
    if best is None:
        return ""
    name = symbols[(2, best)]
    return name if best == addr else f"{name}+{addr - best}"


def main():
    case, native_path = sys.argv[1], sys.argv[2]
    dll, orig = original_trace(case)
    native = []
    for line in Path(native_path).read_text().splitlines():
        parts = line.split()
        native.append(bytes(int(b, 16) for b in parts[1:]))
    symbols = load_symbols(CD / dll)
    chan_bases = [0x239 + 0x43 * i for i in range(10)]
    for tick, (a, b) in enumerate(zip(orig, native), start=1):
        if a != b:
            print(f"first divergence after tick {tick}:")
            for i, (x, y) in enumerate(zip(a, b)):
                if x != y:
                    addr = TRACE_START + i
                    print(f"  {addr:04X} {describe(addr, symbols, chan_bases):24} original {x:02x}  native {y:02x}")
            return
    print(f"no state divergence in {min(len(orig), len(native))} ticks "
          f"(original ran {len(orig)}, native {len(native)})")


if __name__ == "__main__":
    main()
