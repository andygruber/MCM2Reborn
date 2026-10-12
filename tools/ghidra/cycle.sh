#!/bin/sh
# Export game.asm from Ghidra, assemble it, compile code/mcm2, link, and check the result against
# retail - as ONE command.
#
# These run together on purpose. Run separately, a stale executable has twice been measured as if
# it were fresh, and that failure is silent: every check passes, because the stale build is
# internally consistent. It is simply not the build the change was meant to produce.
#
# The scripts in tools/ghidra are the master copies. Ghidra runs them from C:/mcm2ghidra/scripts,
# because analyzeHeadless.bat splits its arguments on spaces and this tree's path has one.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$HERE/../.." && pwd)
WORK=/c/mcm2ghidra
GH="${GHIDRA_HEADLESS:-$GHIDRA_INSTALL_DIR/support/analyzeHeadless.bat}"
[ -f "$GH" ] || { echo "set GHIDRA_INSTALL_DIR to a Ghidra 12.1 install, or GHIDRA_HEADLESS to its analyzeHeadless.bat"; exit 1; }
ML=$(ls -d "/c/Program Files/Microsoft Visual Studio"/*/*/VC/Tools/MSVC/*/bin/Hostx64/x86/ml.exe | tail -1)

mkdir -p "$WORK/scripts" "$WORK/out"
cp "$HERE"/*.java "$WORK/scripts/"

echo "=== 1/5 export ==="
"$GH" C:/mcm2ghidra/proj MCM2 -process Mcm2.exe -noanalysis \
    -scriptPath C:/mcm2ghidra/scripts -postScript ExportAsm.java C:/mcm2ghidra/out 2>&1 \
    | grep -iE "ExportAsm.java>|ERROR|Exception|error:" | sed 's/^INFO *//' \
    | grep -viE "^ExportAsm.java>   (code|data): " || true

echo "=== 2/5 assemble ==="
cd "$WORK/out"
"$ML" -c -coff -Fogame.obj game.asm 2>&1 | grep -viE "^(Microsoft|Copyright|$| Assembling)" || true
ls -l "$WORK/out/game.obj"

echo "=== 3/5 compile ==="
cd "$REPO"
py -3.12 tools/compile.py
[ -f build/game.res ] || py -3.12 tools/resources.py

echo "=== 4/5 link ==="
py -3.12 tools/link.py

echo "=== 5/5 verify .text against retail ==="
py -3.12 tools/verify_text.py
echo CYCLE_DONE
