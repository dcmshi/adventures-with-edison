#!/bin/bash
# dis.sh SEG OFF [LINES] [BITS]: disassemble WMAIN.EXE segment SEG from hex
# offset OFF (ndisasm; 32 for segments 80-82). EXE: $WMAIN, default
# D:/tools/edison-run/WMAIN.EXE.
here=$(cd "$(dirname "$0")" && pwd -W)
bin=$(mktemp)
python -c "
import sys; sys.path.insert(0, r'$here/..'); from ne import NEFile
open(r'$bin', 'wb').write(NEFile(r'${WMAIN:-D:/tools/edison-run/WMAIN.EXE}').segment_bytes($1))"
ndisasm -b ${4:-16} -o 0x$2 -e 0x$2 "$bin" | head -${3:-80}
rm -f "$bin"
