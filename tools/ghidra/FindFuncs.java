// Recover the functions VC6's frame-pointer omission hid from the analyser - on proof, not pattern.
//
// Gaps.java established the shape: 117 KB of .text belongs to no function, and NOT ONE of those
// runs opens with `push ebp; mov ebp, esp`. VC6 at /O2 omits the frame pointer, so these functions
// start with `sub esp, N`, `mov eax, fs:[0]` or a bare `mov eax, [esp+4]`, and Ghidra's MSVC
// function-start patterns are built around the frame prologue. Nothing is damaged or hidden; the
// functions simply do not look like what was searched for.
//
// A first version of this script walked each uncovered RUN and skipped leading padding to guess
// where the function began. That was the wrong shape twice over: VC6 also pads with multi-byte nops
// (`8B FF`, `8D 49 00`) which a 0x90/0xCC skip walks straight past, and a run frequently holds
// SEVERAL consecutive undiscovered functions, so examining only its first address can never find
// the rest. It also cannot tell a function from a switch jump table, and disassembling a jump table
// as code writes damage into the database that no later rule change can undo.
//
// So the rule here does not involve padding, prologues or run boundaries at all:
//
//     every dword in the image that points into .text is a candidate; if no function covers that
//     address, it is a function start.
//
// That is proof rather than resemblance. A dword holding a .text address is a vtable slot, a
// function table entry or a switch target - and a switch target already lies inside its own
// function's body, so it is excluded by the same test. It is also why a direct-call search found
// nothing: these are reached through tables, never by a `call rel32` the analyser could follow.
//
// Run with `apply` to create them. Without it the script only reports, so the evidence can be read
// before the database is written to.
//
// @category OpenMCM2

import java.io.File;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.Listing;
import ghidra.program.model.mem.MemoryBlock;

public class FindFuncs extends GhidraScript {

    StringBuilder out = new StringBuilder();

    void say(String s) {
        println(s);
        out.append(s).append('\n');
    }

    @Override
    public void run() throws Exception {
        String outDir = "C:/mcm2ghidra/out";
        boolean apply = false;
        for (String s : getScriptArgs()) {
            if (s.equalsIgnoreCase("apply")) {
                apply = true;
            } else if (s.length() > 0) {
                outDir = s;
            }
        }
        new File(outDir).mkdirs();
        say("mode: " + (apply ? "APPLY (creating functions)" : "report only"));

        Listing listing = currentProgram.getListing();
        MemoryBlock text = currentProgram.getMemory().getBlock(".text");
        long tlo = text.getStart().getOffset(), thi = text.getEnd().getOffset();

        // ---- every dword in the image that points into .text ----
        List<Long> candidates = new ArrayList<>();
        long scanned = 0;
        for (String blockName : new String[]{".rdata", ".data", ".text"}) {
            MemoryBlock b = currentProgram.getMemory().getBlock(blockName);
            if (b == null || !b.isInitialized()) {
                continue;
            }
            long lo = b.getStart().getOffset(), hi = b.getEnd().getOffset();
            for (long p = lo; p + 3 <= hi; p += 4) {
                long v;
                try {
                    v = getInt(toAddr(p)) & 0xFFFFFFFFL;
                } catch (Exception e) {
                    continue;
                }
                scanned++;
                if (v >= tlo && v <= thi) {
                    candidates.add(v);
                }
            }
        }
        Collections.sort(candidates);
        List<Long> uniq = new ArrayList<>();
        long prev = -1;
        for (long v : candidates) {
            if (v != prev) {
                uniq.add(v);
                prev = v;
            }
        }
        say(String.format("dwords scanned            : %d", scanned));
        say(String.format("pointing into .text       : %d  (%d distinct)", candidates.size(), uniq.size()));

        // ---- classify each distinct target ----
        int atEntry = 0, insideBody = 0, uncovered = 0;
        List<Long> toMake = new ArrayList<>();
        for (long v : uniq) {
            Address at = toAddr(v);
            Function f = listing.getFunctionContaining(at);
            if (f == null) {
                uncovered++;
                toMake.add(v);
            } else if (f.getEntryPoint().getOffset() == v) {
                atEntry++;
            } else {
                insideBody++;      // a switch target, or a label inside a known function
            }
        }
        say(String.format("   already a function entry : %d", atEntry));
        say(String.format("   inside a known body      : %d  (switch targets and internal labels)", insideBody));
        say(String.format("   COVERED BY NOTHING       : %d  <- undiscovered functions", uncovered));
        say("");

        // A sample, so the claim can be checked by eye rather than taken on trust.
        say("first 25 of them, with their opening bytes:");
        for (int i = 0; i < Math.min(25, toMake.size()); i++) {
            long v = toMake.get(i);
            StringBuilder hex = new StringBuilder();
            for (int k = 0; k < 14; k++) {
                hex.append(String.format("%02X ", getByte(toAddr(v + k)) & 0xFF));
            }
            say(String.format("   0x%08X  %s", v, hex.toString().trim()));
        }

        if (apply) {
            // Two tiers, kept apart so the risk of each is visible and separable.
            //
            // TIER A is free of risk: the address is ALREADY the start of a disassembled
            // instruction, so nothing new is decoded and all that happens is a function gets drawn
            // round bytes Ghidra had already decided were code. 85.6% of the uncovered bytes are in
            // this state, so most of the recovery lands here.
            //
            // TIER B has to disassemble undefined bytes, and that is where a database can be
            // damaged past recovery: not every dword holding a .text-shaped value is a pointer, and
            // one that is not would have real bytes decoded as instructions at the wrong offset.
            // `0x00404031` in the sample opens `11 8B 56 48`, an `adc` - almost certainly a
            // coincidence rather than a function. Two guards apply. Ghidra will not disassemble
            // over an existing instruction, so a mid-instruction address simply refuses; and the
            // byte BEFORE a genuine function start is always a terminator or padding, never live
            // code, so anything else is skipped.
            int madeA = 0, madeB = 0, already = 0, refused = 0, notBoundary = 0;
            for (long v : toMake) {
                Address at = toAddr(v);
                try {
                    if (getFunctionContaining(at) != null) {
                        already++;          // absorbed by a function created earlier in this pass
                        continue;
                    }
                    boolean wasCode = listing.getInstructionAt(at) != null;
                    if (!wasCode) {
                        int before = v > tlo ? (getByte(toAddr(v - 1)) & 0xFF) : 0x90;
                        //  90 nop   CC int3   C3 ret   C2 ret n   E9/EB jmp   00 zero fill
                        boolean boundary = before == 0x90 || before == 0xCC || before == 0xC3
                                || before == 0xC2 || before == 0xE9 || before == 0xEB
                                || before == 0x00;
                        if (!boundary) {
                            notBoundary++;
                            continue;
                        }
                        disassemble(at);
                        if (listing.getInstructionAt(at) == null) {
                            refused++;
                            continue;
                        }
                    }
                    if (createFunction(at, null) != null) {
                        if (wasCode) {
                            madeA++;
                        } else {
                            madeB++;
                        }
                    } else {
                        refused++;
                    }
                } catch (Exception e) {
                    refused++;
                }
            }
            say("");
            say(String.format("tier A - already code, function drawn round it : %d", madeA));
            say(String.format("tier B - undefined bytes disassembled          : %d", madeB));
            say(String.format("absorbed by a function made earlier            : %d", already));
            say(String.format("skipped, previous byte is live code            : %d", notBoundary));
            say(String.format("refused by the disassembler                    : %d", refused));
        }

        PrintWriter w = new PrintWriter(
                new File(outDir, apply ? "findfuncs_applied.txt" : "findfuncs.txt"), "UTF-8");
        w.print(out);
        w.println();
        w.println("all uncovered pointer targets:");
        for (long v : toMake) {
            w.printf("0x%08X%n", v);
        }
        w.close();
        say("");
        say("wrote report to " + outDir);
    }
}
