"""build_all.py <game ui folder> <out folder> <preview dir>: every MCM2:Reborn menu screen, rebuilt.

Each builder writes its own screens' DATs into the out folder; together they are the whole of
UI\\modern. Rebuild them all when something every screen draws changes (the version, the kit).
"""
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).parent
BUILDERS = ["build_boxes", "build_leaders", "build_multiplayer", "build_online", "build_options",
            "build_procircuit", "build_race", "build_records", "build_singleplayer"]


def main(ui, out, previews):
    Path(out).mkdir(parents=True, exist_ok=True)
    for builder in BUILDERS:
        print(builder, flush=True)
        subprocess.run([sys.executable, str(HERE / (builder + ".py")), ui, out, previews], check=True)
    print("build_mainmenu", flush=True)
    subprocess.run([sys.executable, str(HERE / "build_mainmenu.py"), ui, str(Path(out) / "MAINMENU.DAT"), previews],
                   check=True)


if __name__ == "__main__":
    main(*sys.argv[1:4])
