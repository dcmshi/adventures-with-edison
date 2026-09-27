"""Writes <LIB>.txt ordinal/name tables for the DLLs on the CD, so the
Ghidra scripts can name imports Ghidra only knows by ordinal (WinG, the
sound and timer DLLs). Output: extracted/ghidra/ordinals/ (git-ignored)."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from ne import NEFile  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent.parent
out = ROOT / "extracted" / "ghidra" / "ordinals"
out.mkdir(parents=True, exist_ok=True)
for f in sorted((ROOT / "original" / "cd").glob("DSK*/*.DLL")):
    try:
        ne = NEFile(f)
    except ValueError:
        continue  # WING32.DLL is a PE file
    (out / f"{f.stem.upper()}.txt").write_text("".join(f"{o} {n}\n" for o, n in sorted(ne.names.items())))
