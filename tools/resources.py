# -*- coding: utf-8 -*-
"""Write Mcm2.exe's resources out as a .res file for the link.

Given a .res, the linker builds .rsrc itself and places it after every other section, fixing up
the RVAs inside it. That lets sections of our own sit between .data and .rsrc without moving
anything the game addresses: nothing in the game refers into .rsrc except through the resource
directory, which the header points at.
"""
import os
import struct
import sys

from paths import RETAIL

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

OUT = os.path.join(ROOT, "build", "game.res")


def resource_section(image):
    """(raw bytes of the resource section, its RVA)."""
    pe = struct.unpack_from("<I", image, 0x3C)[0]
    count, = struct.unpack_from("<H", image, pe + 6)
    opt_size, = struct.unpack_from("<H", image, pe + 20)
    rva, _ = struct.unpack_from("<II", image, pe + 24 + 96 + 8 * 2)
    for i in range(count):
        _, vsize, va, raw_size, raw_ptr = struct.unpack_from("<8sIIII", image, pe + 24 + opt_size + 40 * i)
        if va <= rva < va + vsize:
            return image[raw_ptr + rva - va:raw_ptr + raw_size], rva
    sys.exit("no resource section in %s" % RETAIL)


def entries(rsrc, offset):
    """(id or name, offset, is_directory) for each entry of the directory at `offset`."""
    named, numbered = struct.unpack_from("<HH", rsrc, offset + 12)
    for i in range(named + numbered):
        key, target = struct.unpack_from("<II", rsrc, offset + 16 + 8 * i)
        if key & 0x80000000:
            length, = struct.unpack_from("<H", rsrc, key & 0x7FFFFFFF)
            start = (key & 0x7FFFFFFF) + 2
            key = rsrc[start:start + 2 * length].decode("utf-16-le")
        yield key, target & 0x7FFFFFFF, bool(target & 0x80000000)


def res_name(key):
    """A type or name field: 0xFFFF and an ordinal, or a NUL-terminated UTF-16 string."""
    if isinstance(key, int):
        return struct.pack("<HH", 0xFFFF, key)
    return key.upper().encode("utf-16-le") + b"\0\0"


def pad4(data):
    return data + b"\0" * (-len(data) % 4)


def res_entry(type_key, name_key, language, data):
    names = pad4(res_name(type_key) + res_name(name_key))
    header_size = 8 + len(names) + 16
    header = struct.pack("<II", len(data), header_size) + names
    header += struct.pack("<IHHII", 0, 0x1030, language, 0, 0)   # MOVEABLE | PURE | DISCARDABLE
    return header + pad4(data)


def main():
    with open(RETAIL, "rb") as f:
        image = f.read()
    rsrc, base = resource_section(image)

    out = res_entry(0, 0, 0, b"")      # every .res opens with an empty entry
    count = 0
    for type_key, type_dir, _ in entries(rsrc, 0):
        for name_key, name_dir, _ in entries(rsrc, type_dir):
            for language, leaf, is_dir in entries(rsrc, name_dir):
                if is_dir:
                    sys.exit("unexpected fourth level in the resource tree")
                data_rva, size = struct.unpack_from("<II", rsrc, leaf)
                out += res_entry(type_key, name_key, language, rsrc[data_rva - base:data_rva - base + size])
                count += 1

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "wb") as f:
        f.write(out)
    print("%d resources -> %s" % (count, OUT))


if __name__ == "__main__":
    main()
