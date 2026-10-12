# Asset formats

Separate from the executable work, and not started yet. Recorded here so the groundwork is not
redone later.

## What the install contains

`Desktop\PC Games\MCM2` holds the game data in per-mode directories - `BAJA`, `ENDURO`,
`NATIONAL`, `QUARRIES`, `SX`, `TAG`, `TERAFORM` (the track editor's data), plus `BIKES`, `RIDERS`,
`UI` and `RECORD`. Roughly 1.5 GB in total.

## A published reference exists

**https://github.com/AlexKimov/motocross-madness-file-formats** (MIT, archived read-only since
2023-09-15, 8 commits). It carries 010 Editor binary templates (`.bt`) and scripts (`.1sc`) rather
than prose specifications:

| | |
|---|---|
| MCM2 | `DAT` game resources - `DecodeRES.1sc`, `unpackDAT2.1sc` |
| MCM1 | `DAT` resources (`unpackDAT.1sc`), `TRN` track data, `VUB` animations |

Only the `DAT` container is covered for MCM2; MCM1's track and animation formats are documented but
MCM2's are not, so its `TRN` template is a starting point rather than an answer.

Two things to keep in mind when it is picked up:

- **A 010 template is a parser, not a specification.** It encodes field offsets and sizes, which is
  most of what is needed, but nothing about meaning or invariants. Read it as evidence, then confirm
  against the binary - `Mcm2.exe`'s own loader is the authority, and after the RTTI pass there are
  named classes to read it from.
- **It describes the retail formats.** This port targets a specific patched image (see
  [`binary.md`](binary.md)); nothing suggests the data files differ, but that is an assumption until
  a file is round-tripped.

The executable side does not depend on any of this. It matters for the modding and asset work that
follows a running binary.
