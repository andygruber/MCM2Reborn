# The assemble-and-link harness, ported from OpenMM2

The design is not restated here - `OpenMM2/docs/harness.md` is the spec, and it holds unchanged
because `Mcm2.exe` has the same shape in every respect that matters (see [`binary.md`](binary.md)).
This records what the port needed, which is only interesting where MCM2 differs.

In short: emit the whole original executable as MASM, link it, and get a runnable binary at 0%
reimplemented. Then port one function at a time - write the C++, flip `IMPORT` to `EXPORT`, strip
that `PROC` out of `game.asm`, rebuild, run. Each function is verified against the real game the day
it is written, and "how much is left" is literally the size of `game.asm`.

## What the exporter needed

`ExportAsm.java` came across almost intact. Its output directory is already a script argument, and
none of the MM2 addresses in it are load-bearing - they appear only in comments. So the port was the
default path, the category tag, and three real defects.

**All three were caused by the same difference: `Mcm2.exe` carries MSVC RTTI and `midtown2.exe` does
not.** RTTI gives Ghidra 256 type descriptors, 253 vtables and a set of demangled names, and those
names break assumptions the MM2 exporter was never asked to hold.

### 1. `~` is not a MASM identifier character

Ghidra names a destructor `~Name`. `ml.exe` answers with

```
game.asm(492524) : error A2044:invalid character in file
... 100 consecutive lines ...
game.asm(492548) : fatal error A1012:error count exceeds 100; stopping assembly
```

None of the reported lines is wrong, none of them contains the offending character, and the message
names neither the symbol nor the character. The whole of it was one symbol - `~exception`, six lines
out of 21 MB.

`sanitize()` now replaces anything outside MASM's set (`A-Z a-z 0-9 _ $ ? @ .`), with a leading `~`
becoming `dtor_` so the name still says what it is. Mangled names starting with `?` are returned
untouched, as before: they are the interface the ported C++ links against, and they are legal
already.

Scanning the whole 21 MB for illegal identifiers - in labels, `EXTERN`s and operands alike - found
exactly one, which is worth knowing because it means the rule is cheap.

### 2. Reading the raw symbol map instead of the disambiguated one

Ghidra reuses a name freely across addresses. With RTTI present, `RTTI_Type_Descriptor` occurs
**256 times** and `vftable` likewise. `assignUniqueNames()` already resolves that - it is where the
"1,294 disambiguated" in the log comes from - and writes the result to `finalName`, which `nameFor()`
reads.

The exporter read `symbolAt` **directly in ten places**, bypassing it, and emitted 256 identical
labels:

```
game.asm(566995) : error A2005:symbol redefinition : RTTI_Type_Descriptor
```

Every site that turns an address into emitted text now goes through `nameFor()`; only
`buildSymbolIndex`, `assignUniqueNames` and `nameFor` itself touch the raw map. `nameFor()` carries
a comment saying so, because the next person to add an emit site will otherwise reach for the map.

This one deserves a note beyond the fix. **The bug was harmless in MM2 purely by luck** - its data
symbols happened to be unique. And it failed loudly here only because duplicate *definitions* are an
assembly error. The same bug in a *reference* would have quietly resolved to whichever duplicate the
assembler saw first, which is the failure mode the OpenMM2 notes describe as invisible to a
differential verifier: the bytes differ from retail by exactly the amount a correct relocation
would.

### 3. A label that has to land inside a pointer

`.rdata` holds a `.text` pointer at `0x5555FC`, and the code does `push offset D_5555FF` - a pointer
to that pointer's own last byte, which happens to be zero. It is an ordinary VC6 economy: rather
than allocate an empty string, the compiler points at a zero byte it already has.

Emitted as a single `dd offset ...` there is nowhere to put the label, so it was never written and
the two instructions using it failed with `A2006: undefined symbol : D_5555FF`.

The pointer branch now declines when a needed label falls at `a+1`, `a+2` or `a+3`, and the dword
falls through to byte emission with the label between the bytes. This is the data counterpart of a
rule `.text` already had - *an instruction with a label inside it keeps its bytes* - and the count is
reported per export.

The trade is that such a dword keeps its literal retail value rather than being relocated. Here that
is exactly right, because it points into `.text` and the link places `.text` at its original
address. A pointer into `.rdata` or `.data` would be wrong by that section's displacement, which is
why the number is printed rather than passed over in silence.

## Export figures

From the first clean export of `Mcm2.exe`:

| | |
|---|--:|
| functions emitted | 5,720 |
| absolute relocations | 22,530 |
| relative relocations | 29,013 |
| import relocations | 959 |
| data pointers symbolised | 10,987 |
| data labels emitted | 6,529 |
| **references with no symbol** | **1** |
| imports | 244 |
| instructions | 388,681 |
| `game.asm` | 21.2 MB |

One unresolved reference in the whole binary, against three for MM2.

Everything is currently emitted as `db`. That is the correct bootstrap state: `verify_encodings.py`
derives the mnemonic table *from* an export, so the order is export-as-bytes, measure, re-export.
`ExportAsm` handles the missing table on its own ("no encodings.tsv next to the output - emitting
all instructions as db").

## It links, and every byte of .text is accounted for

`sh C:/mcm2ghidra/cycle.sh` runs export, assemble, link and verify as one command - together on
purpose, because doing them separately twice let a stale executable be measured as if it were fresh,
and that failure is silent: the stale build is internally consistent, so every gate passes.

```
game.obj          3,339,479 bytes, assembled with zero errors
OpenMCM2.exe      2,713,088 bytes, linked with zero errors
```

| | retail | built |
|---|---|---|
| entry RVA | `0x0013D6F5` | **`0x0013D6F5`** |
| `.text` VA | `0x00401000` | **`0x00401000`** |
| `.rdata` VA | `0x00555000` | **`0x00555000`** |
| `.data` VA | `0x0056C000` | `0x00571000` (+0x5000) |

`tools/verify_text.py` compares **all 1,391,230 bytes** of `.text` against retail and demands an
account of every difference:

```
differing bytes: 34982 (2.5145%) in 22918 runs

measured displacement per target section:
   .data     +20480 (0x5000)  agreed by 15932 of 15932 references (100.0%)
   .rdata     +1040 (0x410)   agreed by  6049 of  6986 references ( 86.6%)

references agreeing with their section's displacement : 21981
IAT slots resolving into our own import table         :   937
UNEXPLAINED                                           :     0
```

The `.rdata` figure is not a shortfall: the 937 that disagree are `call dword ptr [__imp__X]`, which
name an import slot. The linker builds its own import table, so ours sits at a different offset and
holds the slots in a different order - the delta is per-symbol, and no single number describes it.
Those are checked by landing inside our import table instead.

That `.rdata`'s *contents* move `+0x410` while its header does not is the same effect, and it is why
the displacement is measured rather than read from the section headers. Assuming the header value
would flag 6,049 correct references as corruption.

## Three more defects, all found only by comparing every byte

The assembler and linker were happy through all three. Layout checks were happy. Only a
byte-for-byte comparison that asks *where each difference points* found them.

**A raw name in `nearest()`.** `sortedSymbols` is filled from `symbolAt` inside `buildSymbolIndex`,
which runs *before* `assignUniqueNames`, so its values are Ghidra's raw names. `nearest()` returned
one, emitting `offset RTTI_Type_Descriptor + 0` for a reference to `0x578C84`; the linker resolved it
to the first symbol of that name, 48 KB away. 144 sites in `.text` held that one address, and across
the image **387 built addresses were being reached from more than one retail address**, one of them
from 48 distinct sources.

**Disambiguation that depended on sort order.** When several addresses share a name, the first in
ascending order kept the plain name. That is not always the address the name belongs to: Ghidra had
`FUN_0053b910` at `0x53B570` *and* at `0x53B910`, so the plain name went to the wrong one. A
colliding name is now suffixed at *every* address, including the first, which removes the ordering
dependence entirely. 91 names collide here.

**An entry point that is not the lowest address of its body.** Two separate cases, and the exporter
assumed neither could happen:

- `FUN_0053b910`'s body is non-contiguous - a five-byte fragment at `0x53B570` plus the real body at
  `0x53B910`. The walk met the fragment first and named it, so all **32 calls** to that function
  landed `0x3A0` short, in five bytes that are not a function. The PROC name is now keyed on the
  entry point, not on which piece the walk reaches first.
- `caseD_0`'s body starts at `0x4EBDD5` but its entry is `0x4EBDE5`, sixteen bytes *later*. The PROC
  opens at the body start, so by the time the walk reaches the entry it is inside an open procedure
  and defines nothing there. The entry now gets a label whenever the open PROC was not opened at it.

The second of those only became visible *because* the first was fixed - it turned from a jump table
silently pointing 16 bytes low into an honest `A2006: undefined symbol`. That is the argument for
fixing the loud failures first.

## The assembly is readable, and that changed no bytes

`tools/verify_encodings.py` settles which mnemonics MASM reproduces exactly, by measurement rather
than by reasoning about encoding tables: it recovers every distinct encoding in the binary, assembles
each in its own 16-byte `ALIGN` slot with the real `ml.exe`, and keeps only those that come back
byte-identical. The hazard it exists for is narrow and sharp - x86 has redundant encodings, so a
correctly-spelled mnemonic can assemble to different bytes of a *different length*, which moves every
address after it and breaks the layout guarantee, silently.

Over 363,788 testable instruction occurrences in 36,241 unique forms:

| | count | share |
|---|--:|--:|
| emit as mnemonic (byte-identical) | 362,477 | **99.64%** |
| must stay `db` (MASM re-encodes) | 1,302 | 0.36% |
| must stay `db` (syntax unsupported) | 9 | 0.00% |

The 1,302 are genuine ambiguities of exactly the kind the tool was built to catch: `fadd st, st` is
`DC C0` in retail but `D8 C0` from MASM; `mov dword ptr [esp], eax` is `89 44 24 00` but MASM emits
the shorter `89 04 24`; `test bl, ah` and its operand-swapped twin differ only in the direction bit.
Those keep their bytes forever, and the fallback is per *instruction*, not per file.

`operator new` now reads as itself:

```asm
??2@YAPAXI@Z PROC
    push 1
    push dword ptr [esp + 8]
    db 0E8h
    dd __nh_malloc - ($ + 4)
    pop ecx
    pop ecx
    ret
??2@YAPAXI@Z ENDP
```

**Crucially, `verify_text.py` still reports 0 unexplained after the switch.** That is the proof the
table is doing its job: 362,135 instructions changed from raw bytes to text and not one byte of the
linked `.text` moved.

## Retail layout: every section byte-identical, and it boots

The link used to build its own import table at the front of `.rdata`, which pushed `.data` 0x5000
high. Code references into `.data` were relocated, but any pointer stored in data that Ghidra had
not flagged kept its retail value and pointed 0x5000 low. The first boot crashed on exactly such a
stale string pointer, 11 seconds in.

That whole class of problem is now gone, rather than patched one pointer at a time:

- **No import libraries.** Retail's import table is part of `.rdata`, so the exporter emits it as
  data, with each IAT slot labelled `__imp__Name` (raw bytes, since the slots hold RVAs or
  ordinals). Every import is called through its retail slot. With nothing to import, the linker
  builds no table of its own.
- **Sections end at their virtual size.** Ghidra's blocks run on through the file padding, and the
  linker's few bytes of its own (debug directory, volatile metadata) then pushed `.data` a page
  high. `trimToSection` cuts each block at retail's virtual size and refuses if anything is
  addressed in the padding.
- **`.rsrc` is its own section**, so it lands at its retail address and its directory, which holds
  RVAs, works as it stands.
- **`tools/link.py` adopts retail's import, IAT and resource directories** into the header, and
  refuses the build if any section is not at its retail address.

Result: `.text`, `.rdata`, `.data` and `.rsrc` all match retail byte for byte, 0 differing bytes in
2.7 MB. Launched from the game folder, `OpenMCM2.exe` plays the intro, reaches the menus and runs
a race.

## Porting a function to C

A C function marked `PORT(0x004890B0)` on the line before it replaces the game's function at that
address. `tools/port.py` does the work in the assembly, between export and assembling:

- the original is renamed `orig_<name>`, its bytes left where they were, so `.text` keeps its
  layout and the original stays callable from C as `game_orig_<name>`;
- every call and jump to it now goes to the C function. The exporter writes those as opcode bytes
  and `dd name - ($ + 4)`, which MASM refuses for an external target, so they are rewritten as the
  `call`, `jmp` or conditional jump they encode, which assembles to the same five or six bytes;
- a port is refused if code elsewhere jumps to a label inside the function, since that code would
  go on running half of the original under the port.

The calling convention has to be the original's exactly. A `__thiscall` method is written as
`__fastcall` with an unused second parameter, which takes EDX.

Win32 calls in ported code go through the game's own import slots, and anything the game does not
import is looked up at run time, so no import table is built and `.rdata` does not grow.

`tools/verify.py` then compares every section with retail. The only differences it accepts are
4-byte references whose retail target is a ported function and whose built target is in `.open`;
anything else fails the build. That proves the game's code is untouched and the redirection is
complete. It proves nothing about the port itself, so each one is also checked against the
original's instructions (disassembling the linked function) and in game.

## Still to do

- **Checks on what ported code does.** A port is compared with the original by reading both; a
  test that runs the two side by side on the same input, through `game_orig_<name>`, is owed.
