# -*- coding: utf-8 -*-
"""Check the build against retail: .text, .rdata and .data must match it byte for byte.

The one difference allowed is a reference to a ported function (build/ports.tsv): retail points it
at the function's address, the build at the C port in .open. That covers absolute pointers - vtable
slots, `push offset` - and relative calls and jumps. Every other differing byte fails the build,
including any in a ported function's original code, which stays in place untouched.
"""
import os
import struct
import sys

from paths import RETAIL

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
BUILT = os.environ.get("MCM2_BUILT", os.path.join(ROOT, "build", "OpenMCM2.exe"))
PORTS = os.path.join(ROOT, "build", "ports.tsv")
CHECKED = (".text", ".rdata", ".data")


def sections(path):
    """{name: (virtual address, bytes over the virtual size)}, with uninitialised tails as zeros."""
    with open(path, "rb") as f:
        image = f.read()
    pe = struct.unpack_from("<I", image, 0x3C)[0]
    count, = struct.unpack_from("<H", image, pe + 6)
    opt_size, = struct.unpack_from("<H", image, pe + 20)
    base, = struct.unpack_from("<I", image, pe + 24 + 28)
    out = {}
    for i in range(count):
        name, vsize, va, raw_size, raw = struct.unpack_from("<8sIIII", image, pe + 24 + opt_size + 40 * i)
        data = image[raw:raw + min(raw_size, vsize)]
        out[name.rstrip(b"\0").decode()] = (base + va, data + bytes(vsize - len(data)))
    return out


def main():
    retail, built = sections(RETAIL), sections(BUILT)
    open_start, open_bytes = built[".open"] if ".open" in built else (0, b"")
    in_open = lambda a: open_start <= a < open_start + len(open_bytes)

    ported = set()
    if os.path.exists(PORTS):
        with open(PORTS, encoding="utf-8") as f:
            ported = {int(line.split()[0], 16) for line in f if line.strip()}

    failed = False
    for name in CHECKED:
        va, old = retail[name]
        new = built[name][1][:len(old)]
        explained = bytearray(len(old))
        retargets = 0

        for i in range(len(old)):
            if old[i] == new[i] or explained[i]:
                continue
            for s in range(max(0, i - 3), min(i, len(old) - 4) + 1):
                was, now = struct.unpack_from("<I", old, s)[0], struct.unpack_from("<I", new, s)[0]
                absolute = was in ported and in_open(now)
                relative = ((va + s + 4 + was) & 0xFFFFFFFF) in ported and in_open((va + s + 4 + now) & 0xFFFFFFFF)
                if absolute or relative:
                    explained[s:s + 4] = b"\1\1\1\1"
                    retargets += 1
                    break

        unexplained = [i for i in range(len(old)) if old[i] != new[i] and not explained[i]]
        print("%-7s %8d bytes, %d references retargeted to ports, %d bytes unexplained"
              % (name, len(old), retargets, len(unexplained)))
        for i in unexplained[:10]:
            print("    0x%08X  retail %02X  built %02X" % (va + i, old[i], new[i]))
        failed |= bool(unexplained)

    if failed:
        sys.exit("the build differs from retail in ways no port explains")
    print("OK - the game's sections are retail's, apart from calls to the %d port(s)" % len(ported))


if __name__ == "__main__":
    main()
