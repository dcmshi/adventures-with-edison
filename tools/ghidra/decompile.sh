#!/bin/sh
# Decompiles game executables with Ghidra (headless) into extracted/ghidra/<name>.c.
# Needs Ghidra (set GHIDRA to its folder) and a JDK 21 (JAVA_HOME, or java on
# the PATH).
# Usage: tools/ghidra/decompile.sh original/cd/DSK3/EDISON.EXE [...]
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
GHIDRA=${GHIDRA:?set GHIDRA to the Ghidra folder}
native() { if command -v cygpath >/dev/null; then cygpath -w "$1"; else echo "$1"; fi; }
headless() {
    if [ -f "$GHIDRA/support/analyzeHeadless.bat" ]; then
        cmd //c "$(native "$GHIDRA/support/analyzeHeadless.bat")" "$@"
    else
        "$GHIDRA/support/analyzeHeadless" "$@"
    fi
}
mkdir -p "$ROOT/extracted/ghidra/proj"
PY="$ROOT/.venv/Scripts/python"; [ -x "$PY" ] || PY="$ROOT/.venv/bin/python"
"$PY" "$ROOT/tools/ghidra/ordinals.py"
"$PY" "$ROOT/tools/nedis.py" "$@" >/dev/null   # function starts for seeding
for exe in "$@"; do
    name=$(basename "$exe" | sed 's/\.[^.]*$//' | tr 'A-Z' 'a-z')
    headless "$(native "$ROOT/extracted/ghidra/proj")" "$name" -import "$(native "$(cd "$(dirname "$exe")" && pwd)/$(basename "$exe")")" \
        -overwrite -scriptPath "$(native "$ROOT/tools/ghidra")" -preScript EnableParamId.java \
        -postScript DecompileAll.java "$(native "$ROOT/extracted/ghidra/$name.c")"         "$(native "$ROOT/extracted/ghidra/ordinals")" "$(native "$ROOT/extracted/disasm/$name.entries")"         "$(native "$ROOT/tools/ghidra/volatile.txt")" 2>&1 | grep -E "DecompileAll|ERROR" || true
done
