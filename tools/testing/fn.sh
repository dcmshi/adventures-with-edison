#!/bin/bash
# fn.sh SEL_OFF... : print Ghidra functions (e.g. 1060_01ce) from wmain.c
for f in "$@"; do awk -v n="FUN_$f " '/^\/\/ ==== /{p=index($0,n)>0} p' $(dirname "$0")/../../extracted/ghidra/wmain.c; done
