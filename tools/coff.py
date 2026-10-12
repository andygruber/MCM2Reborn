# -*- coding: utf-8 -*-
"""Just enough of the COFF object format for the build: sections and the symbol table."""
import struct

SECTION_HEADER = 40
SYMBOL = 18

UNINITIALIZED = 0x00000080
INITIALIZED = 0x00000040
KEEP = 0x00F00000 | 0x00001000 | 0x01000000     # alignment, COMDAT, relocation overflow
READ_WRITE_EXECUTE = 0xE0000000


def header(data):
    """(section count, symbol table offset, symbol count) of an object file."""
    machine, sections, _, symtab, symbols, opt_size, _ = struct.unpack_from("<HHIIIHH", data, 0)
    if machine != 0x14C or opt_size != 0:
        raise ValueError("not an x86 object file")
    return sections, symtab, symbols


def symbol_name(data, offset, strings):
    raw = data[offset:offset + 8]
    if raw[:4] == b"\0\0\0\0":
        start = strings + struct.unpack_from("<I", raw, 4)[0]
        return data[start:data.index(b"\0", start)].decode()
    return raw.rstrip(b"\0").decode()


def symbols(data):
    """(name, section number, storage class) for every symbol; section 0 means undefined."""
    _, symtab, count = header(data)
    strings = symtab + count * SYMBOL
    i = 0
    while i < count:
        offset = symtab + i * SYMBOL
        section, = struct.unpack_from("<h", data, offset + 12)
        storage, aux = data[offset + 16], data[offset + 17]
        yield symbol_name(data, offset, strings), section, storage
        i += 1 + aux


def move_sections(data, rename):
    """Rename sections, making each renamed one initialised, readable, writable and executable.

    `rename` maps a section name to its new one, or returns it unchanged to leave the section
    alone. A zero-filled section gets real zero bytes appended to the file, since the linker
    places zero-filled data apart from initialised data. New names must fit the eight bytes of
    the section header.
    """
    out = bytearray(data)
    tail = bytearray()
    for i in range(header(data)[0]):
        offset = 20 + i * SECTION_HEADER
        name = out[offset:offset + 8].rstrip(b"\0").decode()
        new = rename(name)
        if new == name:
            continue
        if len(new) > 8:
            raise ValueError("section name too long: " + new)

        size, pointer = struct.unpack_from("<II", out, offset + 16)
        flags, = struct.unpack_from("<I", out, offset + 36)
        if flags & UNINITIALIZED:
            pointer = len(data) + len(tail)
            tail += bytes(size)
            struct.pack_into("<I", out, offset + 20, pointer)

        out[offset:offset + 8] = new.encode().ljust(8, b"\0")
        struct.pack_into("<I", out, offset + 36, (flags & KEEP) | INITIALIZED | READ_WRITE_EXECUTE)
    return bytes(out + tail)
