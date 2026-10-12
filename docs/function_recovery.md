# Ghidra misses a quarter of this binary's functions, and why

This is the first substantive finding, and it had to be settled before anything else, because a
function that is not in the database cannot be exported with symbolised operands, cannot be named,
and cannot be ported.

## The symptom

A census of the freshly analysed database:

```
functions          : 4170  (51 thunks)
.text              : 1392640 bytes
  covered by funcs : 1275513  (91.59%)
  NOT covered      : 117127 in 1070 runs  (8.41%)
```

117 KB of executable section belonging to no function, in runs as long as 5,984 bytes. A jump table
that size would hold 1,496 entries, so the large runs were never plausibly tables.

## What is in the gaps

`Gaps.java` classifies every uncovered byte instead of guessing:

```
uncovered .text: 117127 bytes in 1070 runs
   already instructions :   100284  (85.6%)
   defined data         :     2497  (2.1%)
   UNDEFINED bytes      :    14346  (12.2%)
   'push ebp; mov ebp,esp' sequences inside them: 0
```

The last line is the finding. **Not one frame-pointer prologue in 117 KB.** The runs instead open
like this:

| bytes | meaning |
|---|---|
| `64 A1 00 00 00 00` | `mov eax, fs:[0]` - the MSVC SEH frame setup |
| `83 EC 20` / `81 EC AC 00 00 00` | `sub esp, N` - a frame-pointer-omitted prologue |
| `8B 44 24 04` / `8A 44 24 04` | `mov eax, [esp+4]` - reading an argument directly |
| `8B 41 04 C3` | `mov eax, [ecx+4]; ret` - a thiscall accessor |
| `B8 01 00 00 00 C2 04 00` | `mov eax, 1; ret 4` - a virtual returning true |

**`Mcm2.exe` was built by VC6 at `/O2`, which implies `/Oy` - frame pointer omission.** Ghidra's
MSVC function-start patterns are built around `push ebp; mov ebp, esp`, so they match none of these.
Nothing is damaged or hidden; the functions simply do not look like what was searched for.

That explains the shape. It does not explain why a call-reference search finds them either - and it
does not: **zero** of the uncovered runs are the target of a `call`. They are reached through
vtables, so no direct call instruction leads to any of them.

## The rule that works, and the two that did not

The first attempt walked each uncovered run, skipped leading padding and looked for a prologue. It
was wrong in three ways at once, and all three matter:

1. **VC6 pads with multi-byte nops.** `8B FF` (`mov edi, edi`) and `8D 49 00` (`lea ecx, [ecx+0]`)
   are alignment padding, and a skip that only knows `0x90` and `0xCC` walks straight past the end
   of the padding into what it thinks is the function.
2. **A run usually holds several consecutive undiscovered functions.** Examining only the first
   address in a 5,696-byte run can never find the other twenty.
3. **It cannot tell a function from a jump table.** Several runs are plainly tables - ascending
   dwords such as `DF 7D 42 00, FD 7D 42 00, 37 7E 42 00` - and disassembling a jump table as code
   writes damage into the database that no later rule change can undo, because the evidence it would
   need has been overwritten.

The rule that replaced it involves no padding, no prologue and no run boundaries:

> Every dword in the image that points into `.text` is a candidate. If no function covers that
> address, it is a function start.

This is proof rather than resemblance. A dword holding a `.text` address is a vtable slot, a
function-table entry or a switch target - and a switch target already lies inside its own function's
body, so the same test excludes it. Measured over the whole image:

```
dwords scanned            : 673144
pointing into .text       : 10899  (4071 distinct)
   already a function entry : 1282
   inside a known body      : 1194  (switch targets and internal labels)
   COVERED BY NOTHING       : 1595  <- undiscovered functions
```

## Creating them, in two tiers

Not every dword that looks like a `.text` address is one. `0x00404031` appears in the candidate list
and opens `11 8B 56 48` - an `adc` - which is a coincidence, not a function. Creating a function
there would decode real bytes at the wrong offset. So creation is split, and the two halves are
counted separately so their risk is visible:

- **Tier A** - the address is *already* the start of a disassembled instruction. Nothing new is
  decoded; a function is simply drawn round bytes Ghidra had already ruled were code. No risk.
- **Tier B** - undefined bytes, which must be disassembled. Two guards: Ghidra refuses to
  disassemble over an existing instruction, so a mid-instruction address rejects itself; and the
  byte *before* a real function start is always a terminator or padding (`90 CC C3 C2 E9 EB 00`),
  never live code, so anything else is skipped untouched.

```
tier A - already code, function drawn round it : 1104
tier B - undefined bytes disassembled          :   48
absorbed by a function made earlier            :  370
skipped, previous byte is live code            :   24
refused by the disassembler                    :   49
```

73 candidates were refused rather than forced. That is the right outcome: in a binary with no
relocation table there is no recovering from a wrong guess written into the database.

## Result

| | before | after |
|---|--:|--:|
| functions | 4,170 | **5,658** |
| thunks | 51 | 401 |
| `.text` covered | 91.59% | **98.54%** |
| uncovered bytes | 117,127 | **20,322** |
| largest uncovered run | 5,984 | **1,001** |

The remaining 1.46% is padding and genuine jump tables, which is what should be left.

`reimport.sh` runs this pass as part of building the database, because a database without it is a
different database and every later measurement would silently depend on which one was in use.

## What is still owed

4,410 of the 5,658 functions are still `FUN_*`. Names have to come from RTTI - this binary does
carry MSVC RTTI, unlike MM2 - and from the string and call-graph work that follows.
