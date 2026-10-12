# OpenMCM2

A native rebuild of **Motocross Madness 2** (Rainbow Studios, 2000). The retail executable is
disassembled, reassembled and relinked by our own toolchain, with code of our own linked in beside
it, so there is a running game from the first day and any function can be replaced by C and
checked against the real one.

It already runs on its own OpenGL renderer, with no dgVoodoo and no injected hook, and keeps its
settings in the game folder.

No public MCM2 decompilation exists.

## Layout

| | |
|---|---|
| `code/mcm2/` | the code linked into the game: start-up, settings, and the OpenGL renderer |
| `docs/` | measured facts and findings. Start with [`binary.md`](docs/binary.md). |
| `tools/` | the build: compile, link, resources, and the checks against retail |
| `tools/ghidra/` | the Ghidra scripts, including the exporter, and the two build scripts |
| `build/` | build output (not in the repository) |
| `C:\mcm2ghidra\` | the Ghidra workspace - **outside OneDrive and space-free, deliberately** |

OneDrive corrupts Ghidra project files, and `analyzeHeadless.bat` re-splits its arguments on spaces,
so the Ghidra database cannot live next to this tree. The scripts in `tools/ghidra` are the master
copies and are copied into the workspace each time they run. `tools/ghidra/reimport.sh` rebuilds
the database from the binary and is the only supported way to get one.

## Why the OpenMM2 harness carries over unchanged

The approach is only sound if the binary has the shape it assumes, so that was checked before
anything was built. It does, in every respect that matters:

| | MM2 | MCM2 |
|---|---|---|
| format | PE32, i386 | PE32, i386 |
| linker | MSVC | MSVC **6.0** |
| image base | `0x400000` | `0x400000` |
| `RELOCS_STRIPPED` | set | **set** |
| base-relocation table | absent | **absent** |
| sections | `.text` `.rdata` `.data` `.rsrc` | same four |
| separate `.idata` | no | no |

The absent relocation table is the important one. It means the image can only ever load at
`0x400000`, so the link must pin it there - and nothing in the file says which four-byte words in
`.data` are pointers rather than integers or floats that merely look like addresses. That single
fact is what shapes the whole exporter, and it is identical here.

`.text` is 1,391,230 bytes against MM2's ~1.76 MB, so "smaller game" holds, but only by about a
fifth - not an order of magnitude.

Full measurements, including the 15 DLLs and 244 imports and the three that have no modern import
library, are in [`docs/binary.md`](docs/binary.md).

## Where it stands

**The rebuilt game is retail, byte for byte.** `.text`, `.rdata` and `.data` all match the original
exactly, so every pointer in the game's data is right without being found and fixed. Two things made
that possible: the image links with no import libraries, so retail's own import table is used, and
each section ends at its real size ([`docs/harness.md`](docs/harness.md)).

**Our code lives beside it.** `tools/compile.py` builds `code/mcm2` with no CRT and moves every
section into one of our own, `.open`, after the game's; `tools/link.py` binds its Win32 calls to the
game's import slots and refuses the build if any of the game's sections moved or grew.

**It runs on OpenGL.** `code/mcm2` gives the game DirectDraw 7 and Direct3D 7 objects of its own,
written on OpenGL, with the version-1 interfaces DirectShow needs for the videos. The intro, the
menus and races all render through it. Render targets are framebuffers at the game's resolution,
scaled to the window keeping their aspect.

**Settings are self-contained.** `mcm2_profile.json` beside the game is loaded into the registry at
start-up and saved back on exit, as the portable copy's launcher does.

| | |
|---|--:|
| functions recovered | 5,658 (from Ghidra's 4,170) |
| `.text` covered by functions | 98.54% |
| classes named from RTTI | 253 |
| sections differing from retail | **0** |

Next: widescreen (the game's field of view and its 640x480 menus), vertex fog, and porting
functions to C where behaviour should change.

## Running it

```sh
sh tools/ghidra/reimport.sh    # rebuild the Ghidra database from the binary
sh tools/ghidra/cycle.sh       # export -> assemble -> compile -> link -> verify
```

Copy `build/OpenMCM2.exe` into the game folder and run it there. `OpenMCM2.exe -ddraw` uses the
original DirectDraw instead of the renderer, and logs every call it makes, for comparison.

`reimport.sh` imports, analyses, censuses, runs the function recovery and censuses again, printing
the numbers at each step. The recovery pass is part of the import on purpose: a database without it
is a different database, and every later measurement would silently depend on which one was loaded.
RTTI recovery is separate because it takes about ten minutes and changes no listings.

`cycle.sh` runs the build steps together, deliberately. Doing them separately has twice let a stale
executable be measured as if it were fresh, and that failure is silent - the stale build is
internally consistent, so every check passes.
