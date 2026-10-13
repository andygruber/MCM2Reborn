"""
Write a copy of an executable that REA can see every function of:

    py -3.12 tools/rea_view.py <out.exe>                        the retail game
    py -3.12 tools/rea_view.py <out.exe> build/OpenMCM2.exe     a build, from its map

REA's Ghidra session runs auto-analysis only, which never reaches code that is called through a
vtable alone, and on Windows it cannot be told to make a function. Ghidra does make one at every
export, though. So the copy gains an export table naming every function, in a new section after
the others: for retail, every function the export knows, by its port's name once it has one; for
a build, every function its map lists, the C ports by their C names. Nothing else moves: every
byte and address of the original is where it was, so what REA says of the copy holds for it.
"""

import os
import re
import struct
import sys

from paths import RETAIL

ASM = r"C:\mcm2ghidra\out\game.asm.pristine"
PORTS = "build/ports.tsv"


def align(value, to):
    return (value + to - 1) // to * to


def retail_functions():
    """(address, name) for every function, named by its port where it has one."""
    ported = {}
    for line in open(PORTS, encoding="utf-8"):
        if line.strip():
            address, _, c_name = line.split()
            ported[int(address, 16)] = c_name
    found, address = [], None
    for line in open(ASM, encoding="latin-1"):
        m = re.match(r"^; @([0-9A-F]{8})$", line)
        if m:
            address = int(m.group(1), 16)
            continue
        m = re.match(r"^(\S+) PROC", line)
        if m and address is not None:
            found.append((address, ported.get(address, m.group(1))))
            address = None
    return found


def build_functions(map_path):
    """(address, name) for every function in a build's map, a C name over a game label."""
    names = {}
    for line in open(map_path, encoding="latin-1"):
        parts = line.split()
        if len(parts) < 4 or parts[3] != "f" or not re.match(r"^[0-9a-f]{8}$", parts[2]):
            continue
        address = int(parts[2], 16)
        m = re.match(r"^[_@]?(\w+?)(@\d+)?$", parts[1])
        if not m or m.group(1).startswith("game_"):
            continue
        name = m.group(1)
        # A C port shares its address with the game label bound to it; the C name wins.
        if address not in names or re.match(r"^(FUN|vf)_", names[address]):
            names[address] = name
    return list(names.items())


def export_section(base_rva, image_base, module, entries):
    """An export directory at `base_rva`, with names sorted as the loader requires."""
    entries = sorted(entries, key=lambda e: e[1].encode())
    count = len(entries)
    functions_at = base_rva + 40
    names_at = functions_at + 4 * count
    ordinals_at = names_at + 4 * count
    strings_at = ordinals_at + 2 * count

    strings = bytearray(module.encode() + b"\0")
    name_rvas = []
    for _, name in entries:
        name_rvas.append(strings_at + len(strings))
        strings += name.encode() + b"\0"

    body = struct.pack("<IIHHIIIIIII", 0, 0, 0, 0, strings_at, 1, count, count,
                       functions_at, names_at, ordinals_at)
    body += b"".join(struct.pack("<I", address - image_base) for address, _ in entries)
    body += b"".join(struct.pack("<I", rva) for rva in name_rvas)
    body += b"".join(struct.pack("<H", i) for i in range(count))
    return body + bytes(strings)


def main():
    source = sys.argv[2] if len(sys.argv) > 2 else RETAIL
    entries = build_functions(os.path.splitext(source)[0] + ".map") if len(sys.argv) > 2 \
        else retail_functions()

    image = bytearray(open(source, "rb").read())
    pe = struct.unpack_from("<I", image, 0x3C)[0]
    count = struct.unpack_from("<H", image, pe + 6)[0]
    optional = pe + 24
    headers = optional + struct.unpack_from("<H", image, pe + 20)[0]
    image_base, section_align, file_align = struct.unpack_from("<III", image, optional + 28)
    headers_size = struct.unpack_from("<I", image, optional + 60)[0]
    if headers + 40 * (count + 1) > headers_size:
        sys.exit("no room for another section header")
    if struct.unpack_from("<I", image, optional + 96)[0]:
        sys.exit("it already has an export table")

    last = struct.unpack_from("<8sIIII", image, headers + 40 * (count - 1))
    rva = align(last[2] + last[1], section_align)
    raw = align(len(image), file_align)
    body = export_section(rva, image_base, os.path.basename(source), entries)
    size = align(len(body), file_align)

    struct.pack_into("<8sIIIIIIHHI", image, headers + 40 * count, b".edata", len(body), rva, size,
                     raw, 0, 0, 0, 0, 0x40000040)   # initialised data, readable
    struct.pack_into("<H", image, pe + 6, count + 1)
    struct.pack_into("<I", image, optional + 56, align(rva + len(body), section_align))
    struct.pack_into("<II", image, optional + 96, rva, len(body))
    image += bytes(raw - len(image)) + body + bytes(size - len(body))
    open(sys.argv[1], "wb").write(image)
    print("%s: %d functions exported" % (sys.argv[1], len(entries)))


if __name__ == "__main__":
    main()
