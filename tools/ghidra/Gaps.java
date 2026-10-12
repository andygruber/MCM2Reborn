// What is actually inside the parts of .text that no function covers?
//
// The census says 8.41% of .text belongs to no function, in runs as long as 5,984 bytes. That
// number on its own does not say what the bytes ARE, and the three possibilities need completely
// different work:
//
//   * already disassembled, just not bundled into a function - a cheap fix, create the function
//   * defined data - jump tables and constant pools, which is normal and wants no action
//   * undefined bytes - code the analyser never found, which is the expensive case and the one that
//     matters, because an unanalysed instruction's address operands are never symbolised
//
// A 5,984-byte jump table would hold 1,496 entries, so the large runs are very unlikely to be
// tables. This measures it rather than assuming, and dumps the head of the biggest runs plus a
// count of MSVC-6 prologues (55 8B EC = push ebp / mov ebp, esp) found inside them, which is the
// direct evidence for "this is undiscovered code".
//
// @category OpenMCM2

import java.io.File;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.List;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.CodeUnit;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.Listing;
import ghidra.program.model.mem.MemoryBlock;

public class Gaps extends GhidraScript {

    StringBuilder out = new StringBuilder();

    void say(String s) {
        println(s);
        out.append(s).append('\n');
    }

    @Override
    public void run() throws Exception {
        String outDir = "C:/mcm2ghidra/out";
        String[] args = getScriptArgs();
        if (args.length > 0 && args[0].length() > 0) {
            outDir = args[0];
        }
        new File(outDir).mkdirs();

        Listing listing = currentProgram.getListing();
        MemoryBlock text = currentProgram.getMemory().getBlock(".text");
        Address start = text.getStart();
        Address end = text.getEnd();

        // ---- collect the runs no function covers ----
        List<long[]> runs = new ArrayList<>();
        Address a = start;
        long gapStart = -1;
        while (a != null && a.compareTo(end) <= 0) {
            Function f = listing.getFunctionContaining(a);
            if (f != null) {
                if (gapStart >= 0) {
                    runs.add(new long[]{gapStart, a.getOffset() - gapStart});
                    gapStart = -1;
                }
                long len = f.getBody().getMaxAddress().getOffset() - a.getOffset() + 1;
                a = a.add(len);
                continue;
            }
            if (gapStart < 0) {
                gapStart = a.getOffset();
            }
            if (a.equals(end)) {
                break;
            }
            a = a.add(1);
        }
        if (gapStart >= 0) {
            runs.add(new long[]{gapStart, end.getOffset() - gapStart + 1});
        }

        // ---- classify every byte inside them ----
        long asInstr = 0, asData = 0, asUndef = 0;
        long prologues = 0;
        for (long[] r : runs) {
            long off = r[0];
            long remaining = r[1];
            while (remaining > 0) {
                Address at = toAddr(off);
                CodeUnit cu = listing.getCodeUnitContaining(at);
                long step;
                if (cu instanceof Instruction) {
                    step = Math.min(remaining, cu.getLength());
                    asInstr += step;
                } else if (cu instanceof Data && ((Data) cu).isDefined()) {
                    step = Math.min(remaining, cu.getLength());
                    asData += step;
                } else {
                    step = 1;
                    asUndef++;
                }
                if (step <= 0) {
                    step = 1;
                }
                off += step;
                remaining -= step;
            }
            // MSVC 6 frame prologue, the strongest cheap signal that a run holds real code
            for (long p = r[0]; p + 2 < r[0] + r[1]; p++) {
                if ((getByte(toAddr(p)) & 0xFF) == 0x55
                        && (getByte(toAddr(p + 1)) & 0xFF) == 0x8B
                        && (getByte(toAddr(p + 2)) & 0xFF) == 0xEC) {
                    prologues++;
                }
            }
        }

        long total = 0;
        for (long[] r : runs) {
            total += r[1];
        }
        say(String.format("uncovered .text: %d bytes in %d runs", total, runs.size()));
        say(String.format("   already instructions : %8d  (%.1f%%)", asInstr, 100.0 * asInstr / total));
        say(String.format("   defined data         : %8d  (%.1f%%)", asData, 100.0 * asData / total));
        say(String.format("   UNDEFINED bytes      : %8d  (%.1f%%)", asUndef, 100.0 * asUndef / total));
        say(String.format("   'push ebp; mov ebp,esp' sequences inside them: %d", prologues));
        say("");

        runs.sort((x, y) -> Long.compare(y[1], x[1]));
        say("the 20 largest runs, with the first 24 bytes of each:");
        for (int i = 0; i < Math.min(20, runs.size()); i++) {
            long o = runs.get(i)[0], n = runs.get(i)[1];
            StringBuilder hex = new StringBuilder();
            for (int k = 0; k < Math.min(24, n); k++) {
                hex.append(String.format("%02X ", getByte(toAddr(o + k)) & 0xFF));
            }
            CodeUnit cu = listing.getCodeUnitContaining(toAddr(o));
            String kind = cu instanceof Instruction ? "instr"
                    : (cu instanceof Data && ((Data) cu).isDefined()) ? "data" : "undef";
            say(String.format("   0x%08X %6d  %-5s  %s", o, n, kind, hex.toString().trim()));
        }

        PrintWriter w = new PrintWriter(new File(outDir, "gaps.txt"), "UTF-8");
        w.print(out);
        // the full list, for whatever fixes it
        w.println();
        w.println("all runs (address, length):");
        List<long[]> byAddr = new ArrayList<>(runs);
        byAddr.sort((x, y) -> Long.compare(x[0], y[0]));
        for (long[] r : byAddr) {
            w.printf("0x%08X %d%n", r[0], r[1]);
        }
        w.close();
        say("");
        say("wrote " + new File(outDir, "gaps.txt"));
    }
}
