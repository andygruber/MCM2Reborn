"""List a PE32's imports exactly as its own import table states them."""
import struct, sys

path = sys.argv[1]
d = open(path, "rb").read()
pe = struct.unpack_from("<I", d, 0x3C)[0]
nsec = struct.unpack_from("<H", d, pe + 6)[0]
optsz = struct.unpack_from("<H", d, pe + 20)[0]
imgbase = struct.unpack_from("<I", d, pe + 24 + 28)[0]
imp_rva, imp_sz = struct.unpack_from("<II", d, pe + 24 + 96 + 8)
secs = []
for i in range(nsec):
    o = pe + 24 + optsz + i * 40
    vsz, va, rsz, rptr = struct.unpack_from("<IIII", d, o + 8)
    secs.append((va, va + max(vsz, rsz), rptr))


def off(rva):
    for va, ve, rp in secs:
        if va <= rva < ve:
            return rp + (rva - va)
    return None


def cstr(o):
    e = d.index(b"\0", o)
    return d[o:e].decode("latin-1")


total = 0
i = 0
rows = []
while True:
    o = off(imp_rva) + i * 20
    oft, tstamp, fwd, name_rva, first = struct.unpack_from("<IIIII", d, o)
    if not name_rva:
        break
    dll = cstr(off(name_rva))
    thunk = off(oft or first)
    n, ords = 0, 0
    j = 0
    while True:
        v = struct.unpack_from("<I", d, thunk + j * 4)[0]
        if not v:
            break
        if v & 0x80000000:
            ords += 1
        n += 1
        j += 1
    rows.append((dll, n, ords, imgbase + first))
    total += n
    i += 1
rows.sort(key=lambda r: -r[1])
print("%-20s %6s %6s  %s" % ("DLL", "funcs", "byord", "IAT VA"))
for dll, n, o_, iat in rows:
    print("%-20s %6d %6d  0x%08X" % (dll, n, o_, iat))
print("-- %d DLLs, %d imported functions" % (len(rows), total))
