#!/bin/sh
# Copy the dev build into the game folder as OpenMCM2-Dev.exe, beside the normal OpenMCM2.exe,
# and its settings file the first time. Refuses while the game is running.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$HERE/.." && pwd)
GAME="${MCM2_GAME:-/c/Users/xshxd/OneDrive/Desktop/PC Games/MCM2}"

if powershell -NoProfile -Command "if (Get-Process OpenMCM2, OpenMCM2-Dev, Mcm2 -ErrorAction SilentlyContinue) { exit 1 }"; then
    cp "$REPO/build/OpenMCM2.exe" "$GAME/OpenMCM2-Dev.exe"
    # Online play's EOS helper, when tools/build_eos.py has built it.
    [ -f "$REPO/build/eos/mcm2-eos.exe" ] && cp "$REPO/build/eos/mcm2-eos.exe" "$REPO/build/eos/EOSSDK-Win64-Shipping.dll" "$GAME/"
    [ -f "$GAME/OpenMCM2-Dev.ini" ] || cp "$REPO/dist/OpenMCM2-Dev.ini" "$GAME/OpenMCM2-Dev.ini"
    # The content that ships with MCM2:Reborn (tools/content.py), where the game folder lacks it.
    [ -d "$REPO/dist/content" ] && cp -rn "$REPO/dist/content/." "$GAME/"
    echo "deployed $GAME/OpenMCM2-Dev.exe"
else
    echo "the game is running - not deployed"
    exit 1
fi
