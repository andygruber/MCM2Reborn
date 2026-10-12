#!/bin/sh
# Rebuild the MCM2 Ghidra database from scratch, to the state everything else assumes.
#
# WHY THIS SCRIPT HAS TO EXIST, AND HAS TO STAY CORRECT
#
# Ghidra's database is state, and every pass that clears or creates a listing writes back into it.
# On the MM2 port an experimental rule cleared roughly 32,000 listings; the clears persisted after
# the rule was reverted, and much later the exporter read the middle of a live instruction as a
# pointer and relocated it. No rule change could undo that - the evidence it needed had been deleted
# from the database. So: measure any rule change against a freshly imported database, and keep this
# script able to produce one.
#
# WHY THE RECOVERY PASS IS PART OF THE IMPORT, NOT AN EXTRA
#
# Ghidra's own analysis leaves 8.41% of .text belonging to no function, in runs up to 5,984 bytes.
# That is not a defect in Ghidra: Mcm2.exe was built by VC6 at /O2, which OMITS THE FRAME POINTER,
# so its functions open with `sub esp, N`, `mov eax, fs:[0]` or a bare `mov eax, [esp+4]`, and the
# MSVC function-start patterns are built around `push ebp; mov ebp, esp`. There is not one of those
# in the whole uncovered set. The functions are reached only through vtables, so no direct call
# leads to them either.
#
# FindFuncs.java recovers them on proof - every dword in the image that points into .text and that
# no function covers - and takes coverage from 91.59% to 98.54%, 4,170 functions to 5,658. A
# database without that pass is a different database, so the pass belongs here.
#
# The scripts in tools/ghidra are the master copies; they are copied to C:/mcm2ghidra/scripts first,
# because analyzeHeadless.bat splits its arguments on spaces and this tree's path has one.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
GH="${GHIDRA_HEADLESS:-$GHIDRA_INSTALL_DIR/support/analyzeHeadless.bat}"
[ -f "$GH" ] || { echo "set GHIDRA_INSTALL_DIR to a Ghidra 12.1 install, or GHIDRA_HEADLESS to its analyzeHeadless.bat"; exit 1; }
PROJ=C:/mcm2ghidra/proj
SCRIPTS=C:/mcm2ghidra/scripts
OUT=C:/mcm2ghidra/out

mkdir -p /c/mcm2ghidra/scripts /c/mcm2ghidra/out
cp "$HERE"/*.java /c/mcm2ghidra/scripts/

if [ -d /c/mcm2ghidra/proj.old ]; then rm -rf /c/mcm2ghidra/proj.old; fi
if [ -d /c/mcm2ghidra/proj ]; then mv /c/mcm2ghidra/proj /c/mcm2ghidra/proj.old; fi
mkdir -p /c/mcm2ghidra/proj

echo "=== 1/4 importing and analysing Mcm2.exe ==="
"$GH" "$PROJ" MCM2 -import C:/mcm2ghidra/bin/Mcm2.exe 2>&1 \
    | grep -iE "ERROR|Exception|Analysis succeeded|Import succeeded" | tail -10

echo "=== 2/4 census before recovery ==="
"$GH" "$PROJ" MCM2 -process Mcm2.exe -noanalysis -scriptPath "$SCRIPTS" \
    -postScript Census.java "$OUT" >/dev/null 2>&1
cp "$OUT/census.txt" "$OUT/census_before.txt"
grep -E "^functions|covered by funcs|NOT covered" "$OUT/census_before.txt"

echo "=== 3/4 recovering the frame-pointer-omitted functions ==="
"$GH" "$PROJ" MCM2 -process Mcm2.exe -noanalysis -scriptPath "$SCRIPTS" \
    -postScript FindFuncs.java "$OUT" apply >/dev/null 2>&1
grep -E "^tier |^absorbed|^skipped|^refused" "$OUT/findfuncs_applied.txt"

echo "=== 4/4 census after recovery ==="
"$GH" "$PROJ" MCM2 -process Mcm2.exe -noanalysis -scriptPath "$SCRIPTS" \
    -postScript Census.java "$OUT" >/dev/null 2>&1
grep -E "^functions|covered by funcs|NOT covered" "$OUT/census.txt"

echo "=== import done ==="
echo "RTTI is NOT run here - it takes ~10 minutes and changes no listings."
echo "Run it separately when class names are wanted:"
echo "  \"\$GH\" $PROJ MCM2 -process Mcm2.exe -noanalysis -scriptPath $SCRIPTS \\"
echo "      -postScript RttiMcm2.java $OUT"
