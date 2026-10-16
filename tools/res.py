"""Motocross Madness 2 RES archive tool: res.py list | extract | pack | split.

  res.py list ARCHIVE                      the entries, with their places and sizes
  res.py extract ARCHIVE OUTDIR            every entry as a file
  res.py pack OUT.res FILE_OR_FOLDER...    loose files packed into an addon archive
  res.py split ARCHIVE FIRST OUT.res       entries FIRST.. of ARCHIVE into an addon archive

MCM2:Reborn mounts every RES\ADDONS\*.res after the game's own archives (code/mcm2/archives.c), a
later entry taking the place of an earlier one of the same name. The community's custom tracks
need files it added to the ends of its own GLOBAL.RES (from entry 28) and ECO.RES (from 97):
`split` packs those into an addon archive, so a player keeps the retail archives as they are.

Format, read from Mcm2.exe (FUN_00461ec0 open, FUN_00461d20 header check,
FUN_004625b0 read, FUN_004622b0 seek, FUN_004eddc0 directory load):

  file = "FAOE" + body.  Every position below is a body position
  (file offset - 4).  An archive may also be stored unencrypted: then the
  file starts with "RS2\\0" and the body is the whole file (ENGINE.RES).
  Track .ENV files in TERAFORM\\ are the same kind of archive.

  cipher: running key, plain = key ^ cipher; key = (key + cipher) & 0xff.
  The initial key comes from the name the exe opens the archive by
  (name + ext, e.g. "Global.res"; case matters):
      key = 0xFA; for ch in name: key = (((key + 3) & 0xff) ^ ch) + key
  Every absolute seek resets the key to that initial value, so the
  directory (at body 0) and each entry (at its offset) are encrypted
  separately, each starting from the initial key.  The directory always
  starts with "RS2\\0", so the key is also body[0] ^ ord('R') (0x80 for
  every name the retail exe uses).

  directory (decrypted from body 0):
      char  magic[4] = "RS2\\0"
      int32 count
      count x { int32 name_len (includes NUL); char name[name_len];
                int32 offset; int32 size }
  Entry data is stored uncompressed.  Retail archives leave 4 bytes
  between entries; the 2023 community archives are packed tight.
  A name already mounted from an earlier archive is retargeted to the
  later one (FUN_004edcd0), so the last mount wins.
"""
import os
import struct
import sys

MAGIC = b"FAOE"
INNER = b"RS2\0"


def name_key(name):
    key = 0xFA
    for ch in name.encode("latin-1"):
        key = ((((key + 3) & 0xFF) ^ ch) + key) & 0xFF
    return key


def decrypt(data, key):
    out = bytearray(len(data))
    for i, cipher in enumerate(data):
        out[i] = key ^ cipher
        key = (key + cipher) & 0xFF
    return bytes(out)


def encrypt(data, key):
    out = bytearray(len(data))
    for i, plain in enumerate(data):
        out[i] = cipher = key ^ plain
        key = (key + cipher) & 0xFF
    return bytes(out)


def parse_directory(directory):
    """Returns the entry list, or None if the buffer ends mid-directory."""
    try:
        (count,) = struct.unpack_from("<i", directory, 4)
        pos = 8
        entries = []
        for _ in range(count):
            (name_len,) = struct.unpack_from("<i", directory, pos)
            pos += 4
            if pos + name_len + 8 > len(directory):
                return None
            name = directory[pos:pos + name_len].split(b"\0", 1)[0].decode("latin-1")
            pos += name_len
            offset, size = struct.unpack_from("<ii", directory, pos)
            pos += 8
            entries.append((name, offset, size))
        return entries
    except struct.error:
        return None


def read_archive(path):
    """Returns a list of (name, offset, size, data)."""
    raw = open(path, "rb").read()
    if raw[:4] == INNER:  # unencrypted archive (ENGINE.RES): body starts at 0
        def decrypt_part(data):
            return data
        body = raw
    elif raw[:4] == MAGIC:
        body = raw[4:]
        key = body[0] ^ INNER[0]

        def decrypt_part(data):
            return decrypt(data, key)
    else:
        raise ValueError(f"{path}: not a RES archive")

    length = 0x10000
    while True:
        directory = decrypt_part(body[:length])
        if directory[:4] != INNER:
            raise ValueError(f"{path}: bad directory magic {directory[:4]!r}")
        entries = parse_directory(directory)
        if entries is not None:
            break
        if length >= len(body):
            raise ValueError(f"{path}: truncated directory")
        length *= 2

    return [(name, offset, size, decrypt_part(body[offset:offset + size]))
            for name, offset, size in entries]


def write_archive(path, files, game_name):
    """files: list of (name, bytes); game_name is the spelling the exe opens
    the archive by (e.g. "Global.res")."""
    key = name_key(game_name)
    directory = bytearray(INNER + struct.pack("<i", len(files)))
    offset = len(directory) + sum(4 + len(name) + 1 + 8 for name, _ in files)
    for name, data in files:
        encoded = name.encode("latin-1") + b"\0"
        directory += struct.pack("<i", len(encoded)) + encoded
        directory += struct.pack("<ii", offset, len(data))
        offset += len(data)
    with open(path, "wb") as out:
        out.write(MAGIC + encrypt(bytes(directory), key))
        for _, data in files:
            out.write(encrypt(data, key))


def loose_files(paths):
    """(name, bytes) for each file, and for each file under each folder by its path inside it."""
    files = []
    for path in paths:
        if os.path.isdir(path):
            for root, _, names in os.walk(path):
                for name in sorted(names):
                    full = os.path.join(root, name)
                    files.append((os.path.relpath(full, path).replace("/", "\\"), open(full, "rb").read()))
        else:
            files.append((os.path.basename(path), open(path, "rb").read()))
    return files


def main():
    usage = __doc__.split("\n\n")[0]
    if len(sys.argv) < 3 or sys.argv[1] not in ("list", "extract", "pack", "split"):
        sys.exit(usage)
    if sys.argv[1] == "pack":
        files = loose_files(sys.argv[3:])
        write_archive(sys.argv[2], files, os.path.basename(sys.argv[2]))
        print(f"{len(files)} files packed into {sys.argv[2]}")
        return
    if sys.argv[1] == "split":
        if len(sys.argv) < 5:
            sys.exit(usage)
        files = [(name, data) for name, _, _, data in read_archive(sys.argv[2])[int(sys.argv[3]):]]
        write_archive(sys.argv[4], files, os.path.basename(sys.argv[4]))
        print(f"{len(files)} entries of {sys.argv[2]} packed into {sys.argv[4]}")
        return
    if sys.argv[1] == "extract" and len(sys.argv) < 4:
        sys.exit(usage)
    entries = read_archive(sys.argv[2])
    for name, offset, size, data in entries:
        print(f"{offset:10d} {size:10d}  {name}")
        if sys.argv[1] == "extract":
            target = os.path.join(sys.argv[3], *name.replace("\\", "/").split("/"))
            os.makedirs(os.path.dirname(target) or ".", exist_ok=True)
            with open(target, "wb") as out:
                out.write(data)
    print(f"{len(entries)} entries")


if __name__ == "__main__":
    main()
