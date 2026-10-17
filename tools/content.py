r"""MCM2:Reborn's shipped content: content.py <game folder with the content> <out folder>

Gathers what ships with MCM2:Reborn beside its code, laid out as it goes in the game folder:

  RES\ADDONS\CommunityGlobal.res, CommunityEco.res
      the files the custom-track community added to the ends of its own GLOBAL.RES (from entry
      28) and ECO.RES (from entry 97): detail textures and ecosystem models custom tracks use.
      MCM2:Reborn mounts RES\ADDONS\*.res after the game's own archives (code/mcm2/archives.c),
      so the player's retail GLOBAL.RES and ECO.RES stay as they are.
  TERAFORM\NATIONAL\MCM1*
      the Motocross Madness 1 tracks, ported to MCM2 nationals, with their pictures and ghosts.

The out folder is not kept in git (it is game content, shipped with the release); tools/deploy_dev.sh
copies it into a game folder, leaving any file already there alone.
"""
import shutil
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import res  # noqa: E402

# The community's archives: (file, entries in it, the first one it added, the addon it becomes).
COMMUNITY_ARCHIVES = (("GLOBAL.RES", 42, 28, "CommunityGlobal.res"),
                      ("ECO.RES", 284, 97, "CommunityEco.res"))


def find(folder, name):
    """`name` in `folder`, whatever its case."""
    for path in folder.iterdir():
        if path.name.lower() == name.lower():
            return path
    raise SystemExit(f"no {name} in {folder}")


def gather_addons(game, out):
    addons = out / "RES" / "ADDONS"
    addons.mkdir(parents=True, exist_ok=True)
    for name, entries, first, addon in COMMUNITY_ARCHIVES:
        source = find(game / "RES", name)
        found = res.read_archive(source)
        if len(found) != entries:
            raise SystemExit(f"{source} has {len(found)} entries, not the community's {entries}: is it the retail one?")
        files = [(entry_name, data) for entry_name, _, _, data in found[first:]]
        res.write_archive(addons / addon, files, addon)
        print(f"{addon}: {len(files)} entries from {source.name}")


def gather_tracks(game, out):
    national = find(game / "TERAFORM", "NATIONAL")
    target = out / "TERAFORM" / "NATIONAL"
    target.mkdir(parents=True, exist_ok=True)
    tracks = sorted(path for path in national.iterdir() if path.name.lower().startswith("mcm1"))
    for path in tracks:
        shutil.copy2(path, target / path.name)
    print(f"{sum(p.suffix.lower() == '.env' for p in tracks)} MCM1 tracks ({len(tracks)} files)")


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__.split("\n\n")[0])
    game, out = Path(sys.argv[1]), Path(sys.argv[2])
    gather_addons(game, out)
    gather_tracks(game, out)


if __name__ == "__main__":
    main()
