#!/bin/bash
# dis.sh SEG OFF [LINES] [BITS]: disassemble WMAIN.EXE segment SEG from hex
# offset OFF (ndisasm; 32 for segments 80-82). WMAIN.EXE is read from
# $EDISON_RUN (the folder with the game files), or $WMAIN.
wmain=${WMAIN:-${EDISON_RUN:?set EDISON_RUN to the folder with the game files (or WMAIN to WMAIN.EXE)}/WMAIN.EXE}
here=$(cd "$(dirname "$0")" && pwd -W)
bin=$(mktemp)
python -c "
import sys; sys.path.insert(0, r'$here/..'); from ne import NEFile
open(r'$(cygpath -w "$bin")', 'wb').write(NEFile(r'$(cygpath -w "$wmain")').segment_bytes($1))"
ndisasm -b ${4:-16} -o 0x$2 -e 0x$2 "$bin" | head -${3:-80}
rm -f "$bin"
