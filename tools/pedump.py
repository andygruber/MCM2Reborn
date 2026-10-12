"""Read a PE32's headers straight from the file, field by field, and report what a port needs."""
import struct, sys, hashlib

CHARACTERISTICS = [
    (0x0001, "RELOCS_STRIPPED"), (0x0002, "EXECUTABLE_IMAGE"), (0x0004, "LINE_NUMS_STRIPPED"),
    (0x0008, "LOCAL_SYMS_STRIPPED"), (0x0020, "LARGE_ADDRESS_AWARE"), (0x0100, "32BIT_MACHINE"),
    (0x0200, "DEBUG_STRIPPED"), (0x1000, "SYSTEM"), (0x2000, "DLL"),
]
DIRS = ["EXPORT", "IMPORT", "RESOURCE", "EXCEPTION", "SECURITY", "BASERELOC", "DEBUG",
        "ARCHITECTURE", "GLOBALPTR", "TLS", "LOAD_CONFIG", "BOUND_IMPORT", "IAT",
        "DELAY_IMPORT", "COM_DESCRIPTOR", "RESERVED"]

path = sys.argv[1]
d = open(path, "rb").read()
print("file      : %s" % path)
print("size      : %d bytes" % len(d))
print("sha256    : %s" % hashlib.sha256(d).hexdigest())
e_lfanew = struct.unpack_from("<I", d, 0x3C)[0]
assert d[e_lfanew:e_lfanew+4] == b"PE\0\0", "not a PE"
coff = e_lfanew + 4
machine, nsec, tstamp, _psym, _nsym, optsz, chars = struct.unpack_from("<HHIIIHH", d, coff)
print("machine   : 0x%04X %s" % (machine, {0x14C: "i386"}.get(machine, "?")))
print("sections  : %d" % nsec)
print("timestamp : 0x%08X" % tstamp)
print("charact.  : 0x%04X  %s" % (chars, " ".join(n for b, n in CHARACTERISTICS if chars & b)))
opt = coff + 20
magic = struct.unpack_from("<H", d, opt)[0]
print("opt magic : 0x%04X %s" % (magic, {0x10B: "PE32", 0x20B: "PE32+"}.get(magic, "?")))
lmaj, lmin = d[opt+2], d[opt+3]
(szcode, szinit, szuninit, entry, basecode, basedata, imgbase) = struct.unpack_from("<IIIIIII", d, opt+4)
salign, falign = struct.unpack_from("<II", d, opt+32)
szimage, szhdrs = struct.unpack_from("<II", d, opt+56)
subsys = struct.unpack_from("<H", d, opt+68)[0]
ndirs = struct.unpack_from("<I", d, opt+92)[0]
print("linker    : %d.%d" % (lmaj, lmin))
print("imagebase : 0x%08X" % imgbase)
print("entry RVA : 0x%08X  (VA 0x%08X)" % (entry, imgbase + entry))
print("sect align: 0x%X   file align: 0x%X" % (salign, falign))
print("size image: 0x%X   size hdrs: 0x%X" % (szimage, szhdrs))
print("subsystem : %d %s" % (subsys, {2: "WINDOWS_GUI", 3: "WINDOWS_CUI"}.get(subsys, "?")))
print("data dirs : %d" % ndirs)
dd = opt + 96
for i in range(ndirs):
    rva, sz = struct.unpack_from("<II", d, dd + i*8)
    if rva or sz:
        print("   %-14s rva 0x%08X  size 0x%X" % (DIRS[i] if i < len(DIRS) else str(i), rva, sz))
    elif i in (5,):
        print("   %-14s ABSENT (rva 0, size 0)" % DIRS[i])
sh = opt + optsz
print("sections:")
print("   %-9s %-10s %-10s %-10s %-10s %s" % ("name", "VA", "VSize", "RawPtr", "RawSize", "flags"))
for i in range(nsec):
    o = sh + i*40
    name = d[o:o+8].rstrip(b"\0").decode("latin-1")
    vsz, va, rsz, rptr = struct.unpack_from("<IIII", d, o+8)
    fl = struct.unpack_from("<I", d, o+36)[0]
    print("   %-9s 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X" % (name, imgbase+va, vsz, rptr, rsz, fl))
