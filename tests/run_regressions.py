"""Build/run native regression fixtures against production C helpers (Windows/MSVC).
The menu and movie helpers are extracted verbatim so changes are always tested.
"""
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import compile as compiler


def function(source, signature):
    start = source.index(signature)
    begin = source.index("{", start)
    depth = 1
    end = begin + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end] + "\n"


def main():
    out = ROOT / "build" / "regressions"
    out.mkdir(parents=True, exist_ok=True)
    source = (ROOT / "code/mcm2/ui.c").read_text(encoding="utf-8")
    start = source.index("BOOL pad_navigation_active = FALSE;")
    (out / "menu_under_test.inc").write_text(source[start:source.rindex("#endif")], encoding="utf-8")
    (out / "movie_under_test.inc").write_text(function(
        (ROOT / "code/mcm2/dlgprocs.c").read_text(encoding="utf-8"),
        "static void menu_movie_pause("), encoding="utf-8")
    (out / "align_under_test.inc").write_text(function(source, "static DWORD reader_alignment("), encoding="utf-8")
    (out / "frame_under_test.inc").write_text(function(source, "static void dialog_pad_frame("), encoding="utf-8")
    counts = (ROOT / "code/mcm2/selectgamepicprocs.c").read_text(encoding="utf-8")
    begin = counts.index("int g_track_counts[6][3]")
    end = counts.index("#endif", begin)
    (out / "counts_under_test.inc").write_text(counts[begin:end], encoding="utf-8")
    env = compiler.msvc_environment()
    cl = shutil.which("cl", path=env.get("Path") or env.get("PATH"))
    command = [cl, "/nologo", "/std:c11", "/O2", "/Gy", "/W4", "/WX", "/wd4100", "/wd4505", "/arch:IA32",
               "/DOPENMCM2_DEV", "/D_CRT_SECURE_NO_WARNINGS", "/I" + str(ROOT / "code/mcm2"),
               "/I" + str(out), str(ROOT / "tests/native_regressions.c"), str(ROOT / "code/dev/padsony.c"),
               "/Fe" + str(out / "native.exe"), "/Fo" + str(out) + os.sep,
               "/link", "/OPT:REF", "winmm.lib", "user32.lib"]
    subprocess.run(command, cwd=ROOT, env=env, check=True)
    subprocess.run([str(out / "native.exe")], check=True)
    input_command = command[:]
    source_at = input_command.index(str(ROOT / "tests/native_regressions.c"))
    input_command[source_at:source_at + 2] = [str(ROOT / "tests/input_regressions.c")]
    input_command[input_command.index("/Fe" + str(out / "native.exe"))] = "/Fe" + str(out / "input.exe")
    subprocess.run(input_command, cwd=ROOT, env=env, check=True)
    subprocess.run([str(out / "input.exe")], check=True)
    counts_command = input_command[:]
    counts_command[counts_command.index(str(ROOT / "tests/input_regressions.c"))] = str(ROOT / "tests/count_regressions.c")
    counts_command[counts_command.index("/Fe" + str(out / "input.exe"))] = "/Fe" + str(out / "counts.exe")
    subprocess.run(counts_command, cwd=ROOT, env=env, check=True)
    subprocess.run([str(out / "counts.exe")], check=True)


if __name__ == "__main__":
    main()
