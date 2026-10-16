# -*- coding: utf-8 -*-
"""Build mcm2-eos.exe, the 64-bit EOS helper (code/eos/helper.c), into build/eos.

Epic's SDK is 64-bit only, so this is an ordinary 64-bit program with the modern CRT, unlike the
game's own code. The SDK is looked for in the folder named by MCM2_EOS_SDK, else in SDK beside
the game (MCM2_GAME, else PC Games/MCM2 on the desktop). Its DLL is copied next to the exe.
"""
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SRC = os.path.join(ROOT, "code", "eos")
OUT = os.path.join(ROOT, "build", "eos")
GAME = os.environ.get("MCM2_GAME") or os.path.join(os.path.expanduser("~"), "OneDrive", "Desktop", "PC Games", "MCM2")
SDK = os.environ.get("MCM2_EOS_SDK") or os.path.join(GAME, "SDK")
DLL = "EOSSDK-Win64-Shipping.dll"


def msvc_environment():
    """The environment vcvarsall sets up for 64-bit builds."""
    vswhere = os.path.join(os.environ["ProgramFiles(x86)"], "Microsoft Visual Studio", "Installer", "vswhere.exe")
    vs = subprocess.run([vswhere, "-latest", "-property", "installationPath"],
                        capture_output=True, text=True, check=True).stdout.strip()
    vcvars = os.path.join(vs, "VC", "Auxiliary", "Build", "vcvarsall.bat")
    out = subprocess.run('"%s" x64 >nul && set' % vcvars, shell=True, capture_output=True, text=True, check=True).stdout
    return dict(line.split("=", 1) for line in out.splitlines() if "=" in line)


def main():
    if not os.path.isfile(os.path.join(SDK, "Include", "eos_sdk.h")):
        sys.exit("no EOS SDK in %s (set MCM2_EOS_SDK)" % SDK)
    os.makedirs(OUT, exist_ok=True)
    env = msvc_environment()
    cl = shutil.which("cl", path=env.get("Path") or env.get("PATH"))
    res = subprocess.run([cl, "/nologo", "/O2", "/W4", "/WX", "/std:c11", "/D_CRT_SECURE_NO_WARNINGS",
                          "/I" + os.path.join(SDK, "Include"), os.path.join(SRC, "helper.c"),
                          "/Fo" + OUT + "\\", "/Fe" + os.path.join(OUT, "mcm2-eos.exe"),
                          "/link", os.path.join(SDK, "Lib", "EOSSDK-Win64-Shipping.lib")],
                         env=env, cwd=OUT, capture_output=True, text=True)
    sys.stdout.write(res.stdout + res.stderr)
    if res.returncode:
        sys.exit(res.returncode)
    shutil.copy2(os.path.join(SDK, "Bin", DLL), os.path.join(OUT, DLL))
    print("built %s" % os.path.join(OUT, "mcm2-eos.exe"))


if __name__ == "__main__":
    main()
