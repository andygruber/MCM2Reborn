# -*- coding: utf-8 -*-
"""Compile code/mcm2/*.c into build/obj for the link.

The objects are built without the CRT (the game carries its own, and its startup runs, not the
modern one). Their sections are then moved into one section of our own, .open, so that none of
our code, constants or data lands in the game's .text, .rdata or .data: those three must keep
their retail sizes and addresses.

Every part of .open is marked as initialised data that can also be executed. The linker places
sections by kind, and only initialised data goes after the game's .data: a code section would go
straight after .text, and zero-filled data before .rdata, moving every section the game addresses.
"""
import glob
import os
import shutil
import subprocess
import sys

import coff

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SRC = os.path.join(ROOT, "code", "mcm2")
OBJ = os.path.join(ROOT, "build", "obj")

# /arch:IA32: floating point on the x87, as the game's own code does it, so ported arithmetic
# rounds as the original did instead of in SSE registers.
FLAGS = ["/nologo", "/c", "/O2", "/W4", "/WX", "/GS-", "/Zl", "/Gy", "/std:c11", "/arch:IA32",
         "/D_CRT_SECURE_NO_WARNINGS"]

# Where each kind of section goes within .open. Section names are limited to eight characters,
# so the compiler's own $ suffixes (.text$mn) are dropped.
SECTIONS = {".text": ".open$t", ".rdata": ".open$r", ".data": ".open$d", ".bss": ".open$z"}

# Sections whose contents need the modern CRT's startup, which never runs here.
UNSUPPORTED = (".CRT", ".tls")


def msvc_environment():
    """The environment vcvarsall sets up for 32-bit builds."""
    vswhere = os.path.join(os.environ["ProgramFiles(x86)"], "Microsoft Visual Studio", "Installer", "vswhere.exe")
    vs = subprocess.run([vswhere, "-latest", "-property", "installationPath"],
                        capture_output=True, text=True, check=True).stdout.strip()
    vcvars = os.path.join(vs, "VC", "Auxiliary", "Build", "vcvarsall.bat")
    out = subprocess.run('"%s" x86 >nul && set' % vcvars, shell=True, capture_output=True, text=True, check=True).stdout
    return dict(line.split("=", 1) for line in out.splitlines() if "=" in line)


def rename(name):
    base = name.partition("$")[0]
    if base in UNSUPPORTED:
        raise ValueError("section %s needs CRT startup, which does not run in this image" % name)
    return SECTIONS.get(base, name)


def main():
    sources = sorted(glob.glob(os.path.join(SRC, "*.c")))
    os.makedirs(OBJ, exist_ok=True)
    for stale in glob.glob(os.path.join(OBJ, "*.obj")):
        os.remove(stale)
    if not sources:
        print("no sources in %s" % SRC)
        return

    env = msvc_environment()
    cl = shutil.which("cl", path=env.get("Path") or env.get("PATH"))
    res = subprocess.run([cl] + FLAGS + ["/Fo" + OBJ + "\\"] + sources, env=env, cwd=SRC,
                         capture_output=True, text=True)
    print((res.stdout + res.stderr).strip())
    if res.returncode != 0:
        sys.exit("compile failed")

    for path in glob.glob(os.path.join(OBJ, "*.obj")):
        with open(path, "rb") as f:
            data = f.read()
        with open(path, "wb") as f:
            f.write(coff.move_sections(data, rename))
    print("compiled %d file(s) -> %s" % (len(sources), OBJ))


if __name__ == "__main__":
    main()
