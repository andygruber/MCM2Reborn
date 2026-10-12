# -*- coding: utf-8 -*-
"""List the data words that hold an image address and were NOT relocated.

This is the one class of defect that a differential check cannot see, because it produces no
difference at all: a real pointer that nothing flagged keeps its retail value, and since `.data`
moved it now points to the wrong place. `verify_data.py` counts the population; this says what each
one is, so the judgement can be made per word instead of in bulk.

Relocating an integer that merely looks like an address is just as damaging as missing a pointer, so
each candidate is shown with what it points AT - the bytes there, and whether they read as text -
plus whether its neighbours are pointers too. A word sitting in a run of pointers, aimed at a string
or at a structure, is a pointer. One standing alone aimed at the middle of nothing is probably a
number.
"""
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

RETAIL = os.environ.get("MCM2_EXE", r"C:\mcm2ghidra\bin\Mcm2.exe")
BUILT = os.environ.get("MCM2_BUILT", os.path.join(ROOT, "build", "OpenMCM2.exe"))


def load(path):
    with open(path, "rb") as f:
        d = f.read()
    pe = struct.unpack_from("<I", d, 0x3C)[0]
    n = struct.unpack_from("<H", d, pe + 6)[0]
    optsz = struct.unpack_from("<H", d, pe + 20)[0]
    base = struct.unpack_from("<I", d, pe + 24 + 28)[0]
    secs = {}
    for i in range(n):
        o = pe + 24 + optsz + i * 40
        name = d[o:o + 8].rstrip(b"\0").decode("latin-1")
        vsz, va, rsz, rp = struct.unpack_from("<IIII", d, o + 8)
        secs[name] = (base + va, vsz, rp, rsz)
    return d, base, secs


def main():
    rd, rbase, rsec = load(RETAIL)
    bd, bbase, bsec = load(BUILT)
    disp = {".data": bsec[".data"][0] - rsec[".data"][0], ".rdata": 0, ".text": 0}

    def sect(v):
        for nm, (va, vsz, _, _) in rsec.items():
            if va <= v < va + vsz:
                return nm
        return None

    def peek(v, k=16):
        """The bytes at an address in retail, and whether they read as text."""
        nm = sect(v)
        if nm is None:
            return "?", ""
        va, vsz, rp, rsz = rsec[nm]
        o = rp + (v - va)
        if o + k > len(rd) or (v - va) >= rsz:
            return nm, "(uninitialised)"
        by = rd[o:o + k]
        txt = "".join(chr(c) if 32 <= c < 127 else "." for c in by)
        return nm, "%s  |%s|" % (" ".join("%02X" % c for c in by[:8]), txt)

    total = 0
    for name in (".rdata", ".data"):
        rva, rvsz, rrp, rrsz = rsec[name]
        bva, bvsz, brp, brsz = bsec[name]
        off = 0
        n = min(rrsz, rvsz)
        a = rd[rrp:rrp + n]
        b = bd[brp + off:brp + off + n]

        stuck = []
        for o_ in range(0, n - 3, 4):
            rv = struct.unpack_from("<I", a, o_)[0]
            tgt = sect(rv)
            if tgt is None or disp.get(tgt, 0) == 0:
                continue                     # only a MOVED target can be silently wrong
            if struct.unpack_from("<I", b, o_)[0] == rv:
                stuck.append((rva + o_, rv))

        print("=== %s: %d words hold a moved-section address but did not move ===" % (name, len(stuck)))
        total += len(stuck)
        for va, v in stuck:
            # are the neighbours pointers too?
            o_ = va - rva
            neigh = 0
            for d_ in (-8, -4, 4, 8):
                q = o_ + d_
                if 0 <= q < n - 3:
                    nv = struct.unpack_from("<I", a, q)[0]
                    if sect(nv) is not None:
                        neigh += 1
            tgt, what = peek(v)
            print("   0x%08X -> 0x%08X (%-6s) neighbours-that-are-addresses=%d  %s"
                  % (va, v, tgt, neigh, what))
        print()
    print("total: %d" % total)
    return 0


if __name__ == "__main__":
    sys.exit(main())
