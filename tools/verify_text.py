# -*- coding: utf-8 -*-
"""Compare EVERY byte of the built .text against retail, and account for each difference.

This is the gate. A harness that assembles and links proves only that the text was syntactically
acceptable; it says nothing about whether the bytes are the game's. ~5,700 functions with ~52,000
relocations have a great deal of room for a single mis-encoded operand that assembles, links, and
then misbehaves - and checking the entry point and a couple of anchor functions catches a wholesale
shift and nothing else.

HOW A DIFFERENCE IS JUDGED. Most differences are supposed to be there: an address operand that was
symbolised is re-emitted by the linker, and if the section it points into moved, the value changes
by exactly that section's displacement. So the displacement per target section is MEASURED rather
than read out of the headers - `.rdata`'s contents can move by a different amount than its header
does, because the linker puts its own import table at the front of the section. Measuring the mode
of (built - retail) is self-calibrating: a real displacement is shared by thousands of references,
a defect by a handful.

WHAT THIS CANNOT SEE, and it matters. A differential check asks whether each difference is
explainable, so it is blind to a wrong value that happens to be explainable, and to a value that
should have changed and did not. Both bit the MM2 port. Gates that assert positive facts about
content - this structure has seven entries, this string says "tga" - are what catch those, and they
are still owed here.
"""
import collections
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

    # Data directories, as absolute addresses. Only the IAT is used, but reading them all keeps the
    # indexing honest rather than hard-coding an offset.
    ndirs = struct.unpack_from("<I", d, pe + 24 + 92)[0]
    names = ["EXPORT", "IMPORT", "RESOURCE", "EXCEPTION", "SECURITY", "BASERELOC", "DEBUG",
             "ARCHITECTURE", "GLOBALPTR", "TLS", "LOAD_CONFIG", "BOUND_IMPORT", "IAT"]
    dirs = {}
    for i in range(min(ndirs, len(names))):
        rva, sz = struct.unpack_from("<II", d, pe + 24 + 96 + i * 8)
        if rva:
            dirs[names[i]] = (base + rva, sz)
    return d, base, secs, dirs


def main():
    for p in (RETAIL, BUILT):
        if not os.path.exists(p):
            sys.exit("missing %s" % p)

    rd, rbase, rsec, rdirs = load(RETAIL)
    bd, bbase, bsec, bdirs = load(BUILT)

    print("retail : %s" % RETAIL)
    print("built  : %s" % BUILT)
    print()
    print("%-8s %-22s %-22s" % ("section", "retail VA / vsize", "built VA / vsize"))
    for nm in sorted(set(rsec) | set(bsec)):
        r = rsec.get(nm)
        b = bsec.get(nm)
        fr = "0x%08X / 0x%06X" % (r[0], r[1]) if r else "-"
        fb = "0x%08X / 0x%06X" % (b[0], b[1]) if b else "-"
        flag = "" if (r and b and r[0] == b[0]) else "   <-- MOVED" if (r and b) else "   <-- ABSENT"
        print("%-8s %-22s %-22s%s" % (nm, fr, fb, flag))
    print()

    if ".text" not in rsec or ".text" not in bsec:
        sys.exit("no .text in one of them")

    rva, rvsz, rrp, _ = rsec[".text"]
    bva, bvsz, brp, _ = bsec[".text"]
    if rva != bva:
        print("!! .text is at a different address: retail 0x%08X, built 0x%08X" % (rva, bva))

    n = min(rvsz, bvsz)
    a = rd[rrp:rrp + n]
    b = bd[brp:brp + n]
    print(".text compared over %d bytes (retail vsize 0x%X, built 0x%X)" % (n, rvsz, bvsz))

    diff = [i for i in range(n) if a[i] != b[i]]
    if not diff:
        print("\nIDENTICAL - every byte of .text matches retail.")
        return 0
    print("differing bytes: %d  (%.4f%%)" % (len(diff), 100.0 * len(diff) / n))

    # Group into runs, then recover the 4-byte ADDRESS FIELD each run sits inside.
    #
    # The operand is not 4-byte aligned - `mov ecx, [addr]` puts it two bytes into the instruction -
    # so taking the aligned dword straddles the boundary and produces byte-shifted nonsense. The
    # first attempt did exactly that and reported deltas of 0x410, 0x41000 and 0x4100000, which are
    # the same value read at three different offsets. The window is instead the one, of the at most
    # four containing the run, whose RETAIL value is a plausible image address.
    runs = []
    s = diff[0]
    p = diff[0]
    for i in diff[1:]:
        if i != p + 1:
            runs.append((s, p - s + 1))
            s = i
        p = i
    runs.append((s, p - s + 1))
    print("in %d runs" % len(runs))

    lo_img, hi_img = rbase, rbase + 0x400000

    def section_of(v):
        for nm, (va, vsz, _, _) in rsec.items():
            if va <= v < va + vsz:
                return nm
        return None

    # Displacement per TARGET SECTION, measured rather than read from the headers.
    #
    # `.rdata`'s contents move even though its header does not: the linker builds its own import
    # table and puts it at the front of that section. Reading the header value would flag thousands
    # of correct references as corruption. The mode is self-calibrating - a real displacement is
    # shared by thousands of references, a defect by a handful.
    seen = collections.defaultdict(collections.Counter)
    sites = []
    unreadable = 0
    unreadable_sites = []
    for off, ln in runs:
        # BOTH sides must look like an image address, not just retail's.
        #
        # Requiring only retail matched the wrong window on 13 sites: `mov dword ptr [addr], imm`
        # is `C7 05 <addr32> <imm32>`, and the opcode bytes `C7 05` themselves read as 0x005805C7 -
        # squarely inside the image. The built side then came out 0x505805C7, a delta of
        # +0x50000000, which is not a relocation at all but the operand's own first byte read as an
        # address's last. Insisting the built value is also an address rejects that window and
        # finds the real one.
        found = None
        for st in range(off - 3, off + 1):
            if st < 0 or st + 4 > n or st + 4 < off + ln:
                continue
            rv = struct.unpack_from("<I", a, st)[0]
            bv = struct.unpack_from("<I", b, st)[0]
            if lo_img <= rv < hi_img and lo_img <= bv < hi_img:
                found = (st, rv, bv)
                break
        if found is None:
            unreadable += 1
            unreadable_sites.append((rva + off, ln))
            continue
        st, rv, bv = found
        sc = section_of(rv)
        seen[sc][bv - rv] += 1
        sites.append((rva + off, sc, rv, bv))

    print()
    print("measured displacement per target section:")
    expect = {}
    for sc in sorted(seen, key=lambda k: -sum(seen[k].values())):
        d_, c = seen[sc].most_common(1)[0]
        expect[sc] = d_
        total = sum(seen[sc].values())
        print("   %-8s %+7d (0x%X)  agreed by %d of %d references (%.1f%%)"
              % (sc, d_, d_ & 0xFFFFFFFF, c, total, 100.0 * c / total))

    # The IAT is its own explanation, and it is not a displacement.
    #
    # `call dword ptr [__imp__X]` names an import slot. The linker builds its OWN import table and
    # puts it at the front of .rdata, so ours is at a different offset and holds the slots in a
    # different order - the delta is per-symbol, not per-section, and no single number can describe
    # it. Every such reference must land inside our import table, which is what is checked instead.
    iat_va, iat_sz = rdirs.get("IAT", (0, 0))
    biat_va, biat_sz = bdirs.get("IAT", (0, 0))
    iat_hits = 0
    rest = []
    for va, sc, rv, bv in sites:
        if expect.get(sc) is not None and (bv - rv) == expect[sc]:
            continue
        if iat_sz and iat_va <= rv < iat_va + iat_sz:
            if biat_va <= bv < biat_va + biat_sz:
                iat_hits += 1
                continue
        rest.append((va, sc, rv, bv))
    unexplained = rest
    print()
    print("references agreeing with their section's displacement : %d"
          % (len(sites) - iat_hits - len(unexplained)))
    print("IAT slots resolving into our own import table         : %d" % iat_hits)
    print("UNEXPLAINED                                           : %d" % len(unexplained))
    if unreadable:
        print("runs with no readable address field                   : %d" % unreadable)
    for va, ln in unreadable_sites[:12]:
        lo = va - rva
        print("   unreadable VA 0x%08X len %d  retail %s  built %s"
              % (va, ln, " ".join("%02X" % x for x in a[lo - 4:lo + ln + 4]),
                 " ".join("%02X" % x for x in b[lo - 4:lo + ln + 4])))
    for va, sc, rv, bv in unexplained[:20]:
        print("   VA 0x%08X  %-7s retail 0x%08X -> built 0x%08X  (delta %+d)"
              % (va, sc, rv, bv, bv - rv))
    return 1 if unexplained or unreadable else 0


if __name__ == "__main__":
    sys.exit(main())
