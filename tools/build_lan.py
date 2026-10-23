# -*- coding: utf-8 -*-
"""Build mcm2-lan.exe, the LAN helper (code/eos/lan.c), into build/eos beside mcm2-eos.exe.

LAN play talks to the game over the same pipe as the EOS helper, so it is built the same way: an
ordinary 64-bit program with the modern CRT. It needs no SDK, only Windows sockets. On Linux the
MSVC tools run under Wine (tools/toolchain.py).
"""
import os
import subprocess
import sys

import toolchain

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SRC = os.path.join(ROOT, "code", "eos")
OUT = os.path.join(ROOT, "build", "eos")


def main():
    os.makedirs(OUT, exist_ok=True)
    env = toolchain.environment("x64")
    cl, rc = toolchain.command("cl", "x64", env), toolchain.command("rc", "x64", env)
    native = toolchain.native
    resources = os.path.join(OUT, "lan.res")          # its file details and manifest (lan.rc)
    res = subprocess.run(rc + ["/nologo", "/fo", native(resources), native(os.path.join(SRC, "lan.rc"))],
                         env=env, cwd=SRC, capture_output=True, text=True)
    if res.returncode:
        sys.exit(res.stdout + res.stderr)
    res = subprocess.run(cl + ["/nologo", "/O2", "/W4", "/WX", "/std:c11", "/D_CRT_SECURE_NO_WARNINGS",
                               native(os.path.join(SRC, "lan.c")),
                               "/Fo" + native(OUT) + "\\", "/Fe" + native(os.path.join(OUT, "mcm2-lan.exe")),
                               "/link", native(resources), "ws2_32.lib"],
                         env=env, cwd=OUT, capture_output=True, text=True)
    sys.stdout.write(res.stdout + res.stderr)
    if res.returncode:
        sys.exit(res.returncode)
    print("built %s" % os.path.join(OUT, "mcm2-lan.exe"))


if __name__ == "__main__":
    main()
