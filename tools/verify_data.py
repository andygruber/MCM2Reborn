# -*- coding: utf-8 -*-
"""Compare the built .data and .rdata contents against retail, and account for every difference.

`.text` verifies clean, so if the built executable behaves differently from retail the cause is in
the data - which is exactly where this binary is hardest, because it has no relocation table and
nothing in the file says which four-byte words are pointers.

The rules are the same as for .text: a word that was correctly identified as a pointer is re-emitted
by the linker and moves by its target section's displacement. Anything else must be IDENTICAL to
retail. Two failure shapes matter and they are opposite:

  * a word that changed but should not have - a float, a string, or an integer that merely looked
    like an address, rewritten as a pointer. Its value is now wrong.
  * a word that did NOT change but should have - a real pointer into .data that nothing flagged, so
    it kept its retail value and now points 0x5000 low. This one produces NO difference at all,
    which is why it cannot be found by diffing; it is counted separately by looking for retail
    values that land in .data and are still byte-identical.

The second class is the known risk on this port, and it is reported as a population rather than a
verdict: some of those words really are integers.
"""
import collections
import os
import struct
import sys

from paths import RETAIL

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

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

    # Displacement of each section's CONTENTS, taken from the .text verification: .data +0x5000,
    # .rdata +0x410. Recomputed here from the headers where possible, and stated where not.
    disp = {".text": 0, ".data": bsec[".data"][0] - rsec[".data"][0], ".rdata": 0}

    def section_of(v, table):
        for nm, (va, vsz, _, _) in table.items():
            if va <= v < va + vsz:
                return nm
        return None

    rc = 0
    for name in (".rdata", ".data"):
        rva, rvsz, rrp, rrsz = rsec[name]
        bva, bvsz, brp, brsz = bsec[name]
        # Only the bytes retail actually stores are comparable; the rest of .data is BSS the loader
        # zero-fills, and .rdata's built copy has the linker's import table prepended.
        n = min(rrsz, rvsz)
        off = 0
        a = rd[rrp:rrp + n]
        b = bd[brp + off:brp + off + n]
        if len(b) < n:
            print("%s: built section is shorter than retail's; skipping" % name)
            continue

        diff = [i for i in range(n) if a[i] != b[i]]
        print("%s: compared %d stored bytes, %d differ (%.4f%%)"
              % (name, n, len(diff), 100.0 * len(diff) / n))

        # group into runs and read the dword each sits in
        if diff:
            runs = []
            s = p = diff[0]
            for i in diff[1:]:
                if i != p + 1:
                    runs.append((s, p - s + 1))
                    s = i
                p = i
            runs.append((s, p - s + 1))

            good = collections.Counter()
            bad = []
            for o_, ln in runs:
                st = o_ - (o_ % 4)
                if st + 4 > n or st + 4 < o_ + ln:
                    bad.append((rva + o_, ln, None, None))
                    continue
                rv = struct.unpack_from("<I", a, st)[0]
                bv = struct.unpack_from("<I", b, st)[0]
                sc = section_of(rv, rsec)
                if sc is not None and bv - rv == disp.get(sc):
                    good[sc] += 1
                else:
                    bad.append((rva + st, ln, rv, bv))
            print("   relocated correctly : %s" % dict(good))
            print("   UNEXPLAINED         : %d" % len(bad))
            for va, ln, rv, bv in bad[:15]:
                if rv is None:
                    print("      VA 0x%08X len %d  (unaligned run)" % (va, ln))
                else:
                    print("      VA 0x%08X  retail 0x%08X -> built 0x%08X" % (va, rv, bv))
            if bad:
                rc = 1

        # words that point into .data and did NOT move - the silent class
        if name == ".data":
            dva, dvsz, _, _ = rsec[".data"]
            stuck = 0
            for o_ in range(0, n - 3, 4):
                rv = struct.unpack_from("<I", a, o_)[0]
                if dva <= rv < dva + dvsz and struct.unpack_from("<I", b, o_)[0] == rv:
                    stuck += 1
            print("   words holding a .data address that did NOT move: %d" % stuck)
            print("      (some are integers; any that are real pointers now point 0x%X low)"
                  % disp[".data"])
    return rc


if __name__ == "__main__":
    sys.exit(main())
