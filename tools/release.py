"""release.py: MCM2:Reborn's release package, build/release/MCM2Reborn-<version>.zip, and the
launcher beside it (an offline install: the launcher installs from a package next to it).

The package is MCM2:Reborn's own files, laid out as in the game's folder - never the game's:
  MCM2Reborn.exe            the dev build (build/OpenMCM2.exe), named for its settings file
  MCM2Reborn.ini            dist/MCM2Reborn.ini; the launcher keeps a player's own
  mcm2-eos.exe, EOSSDK-Win64-Shipping.dll, eos.ini   online play (eos.ini from the game folder)
  mcm2-lan.exe              LAN play (tools/build_lan.py)
  UI/modern/...             the modern menus, their fonts and photos (from the game folder)
  RES/..., TERAFORM/...     the shipped content (dist/content, tools/content.py)
  MCM2Reborn Launcher.exe   the launcher (tools/build_launcher.py)
  reborn-version.txt        the version, as dev.h's GAME_VERSION

With --over <package.zip> the package is that one (an earlier release's) with this build's files
over it: what this build has not got - eos.ini, the EOS helper, the menus' photos, the content -
stays as that package has it, and so does any file of it this list does not name.

Publishing it is a GitHub release of andygruber/MCM2Reborn tagged v<version> with the zip as its asset;
this only builds it. Publishing the release is enough: .github/workflows/release-packages.yml builds
the package and adds it, with the launcher.
"""
import os
import re
import shutil
import sys
import zipfile

from paths import GAME, find_folder

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
OUT = os.path.join(REPO, "build", "release")
LAUNCHER = "MCM2Reborn Launcher.exe"


def version():
    with open(os.path.join(REPO, "code", "dev", "dev.h")) as f:
        found = re.search(r'#define GAME_VERSION "([^"]+)"', f.read())
    if not found:
        sys.exit("no GAME_VERSION in code/dev/dev.h")
    return found.group(1)


def need(path):
    if not os.path.exists(path):
        sys.exit("missing %s" % path)
    return path


def main():
    over = None
    if sys.argv[1:2] == ["--over"] and len(sys.argv) == 3:
        over = sys.argv[2]
    elif len(sys.argv) > 1:
        sys.exit(__doc__)
    # What an earlier package can stand in for: needed only without one.
    maybe = (lambda path: path if os.path.exists(path) else None) if over else need
    tag = "v" + version()
    files = [
        (need(os.path.join(REPO, "build", "OpenMCM2.exe")), "MCM2Reborn.exe"),
        (need(os.path.join(REPO, "dist", "MCM2Reborn.ini")), "MCM2Reborn.ini"),
        (maybe(os.path.join(REPO, "build", "eos", "mcm2-eos.exe")), "mcm2-eos.exe"),
        (need(os.path.join(REPO, "build", "eos", "mcm2-lan.exe")), "mcm2-lan.exe"),
        (maybe(os.path.join(REPO, "build", "eos", "EOSSDK-Win64-Shipping.dll")), "EOSSDK-Win64-Shipping.dll"),
        (maybe(find_folder(GAME, "eos.ini")), "eos.ini"),
        (need(os.path.join(REPO, "build", "launcher", LAUNCHER)), LAUNCHER),
    ]
    files = [(path, name) for path, name in files if path]
    for root, under in ((maybe(find_folder(GAME, "UI", "modern")), "UI/modern"),
                        (maybe(os.path.join(REPO, "dist", "content")), "")):
        if not root:
            continue
        for folder, _, names in os.walk(root):
            for name in names:
                path = os.path.join(folder, name)
                files.append((path, "/".join(p for p in (under, os.path.relpath(path, root).replace(os.sep, "/")) if p)))

    os.makedirs(OUT, exist_ok=True)
    package = os.path.join(OUT, "MCM2Reborn-%s.zip" % tag)
    with zipfile.ZipFile(package, "w", zipfile.ZIP_DEFLATED) as z:
        for path, name in files:
            z.write(path, name)
        z.writestr("reborn-version.txt", tag)
        if over:
            ours = {name.lower() for _, name in files} | {"reborn-version.txt"}
            with zipfile.ZipFile(over) as earlier:
                kept = [i for i in earlier.infolist() if not i.is_dir() and i.filename.lower() not in ours]
                for info in kept:
                    z.writestr(info, earlier.read(info))
            files += [(None, info.filename) for info in kept]
    shutil.copy2(os.path.join(REPO, "build", "launcher", LAUNCHER), os.path.join(OUT, LAUNCHER))
    print("%s: %d files, %.1f MB" % (package, len(files) + 1, os.path.getsize(package) / 1e6))


if __name__ == "__main__":
    main()
