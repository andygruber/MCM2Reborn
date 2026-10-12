// What the freshly imported database actually contains.
//
// This is the baseline every later claim is measured against, and it exists because "Ghidra
// analysed it" is not a fact about coverage. The number that matters most for the assemble-and-link
// harness is the LAST one: bytes of .text that no function body covers. Those bytes still have to
// be emitted, in order, or every address after them moves - they are jump tables, alignment
// padding, orphaned stubs and any code the analyser did not find. A large or growing figure there
// is the single best early warning that the export will not reproduce retail's layout.
//
// Writes census.txt next to the other output so it can be diffed after a re-import.
//
// @category OpenMCM2

import java.io.File;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSetView;
import ghidra.program.model.listing.CodeUnit;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.listing.Listing;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;
import ghidra.program.model.symbol.SymbolTable;

public class Census extends GhidraScript {

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

        say("program   : " + currentProgram.getName());
        say("format    : " + currentProgram.getExecutableFormat());
        say("compiler  : " + currentProgram.getCompilerSpec().getCompilerSpecID());
        say("image base: " + currentProgram.getImageBase());
        say("md5       : " + currentProgram.getExecutableMD5());
        say("sha256    : " + currentProgram.getExecutableSHA256());
        say("");

        say("sections:");
        for (MemoryBlock b : currentProgram.getMemory().getBlocks()) {
            say(String.format("   %-10s %s .. %s  %8d bytes  %s%s%s%s",
                    b.getName(), b.getStart(), b.getEnd(), b.getSize(),
                    b.isRead() ? "r" : "-", b.isWrite() ? "w" : "-",
                    b.isExecute() ? "x" : "-", b.isInitialized() ? "" : "  UNINITIALISED"));
        }
        say("");

        Listing listing = currentProgram.getListing();

        // ---- functions ----
        long nfunc = 0, ndefault = 0, nthunk = 0, nexternal = 0;
        long bodyBytes = 0;
        FunctionIterator fit = listing.getFunctions(true);
        while (fit.hasNext()) {
            Function f = fit.next();
            if (f.isExternal()) {
                nexternal++;
                continue;
            }
            nfunc++;
            if (f.isThunk()) {
                nthunk++;
            }
            // Ghidra names an undiscovered-purpose function FUN_<addr>. The ratio of those to named
            // ones says how much symbol recovery is still owed, and on a binary with no map file
            // and no exports it starts near 100%.
            if (f.getName().startsWith("FUN_")) {
                ndefault++;
            }
            bodyBytes += f.getBody().getNumAddresses();
        }
        say(String.format("functions          : %d  (%d thunks, %d external)", nfunc, nthunk, nexternal));
        say(String.format("  default-named    : %d  (%.1f%% still FUN_*)",
                ndefault, nfunc == 0 ? 0.0 : 100.0 * ndefault / nfunc));
        say(String.format("  bytes in bodies  : %d", bodyBytes));

        // ---- instructions and data ----
        long ninstr = 0;
        InstructionIterator iit = listing.getInstructions(true);
        while (iit.hasNext()) {
            iit.next();
            ninstr++;
        }
        long ndata = 0, ndefined = 0;
        for (Data d : listing.getDefinedData(true)) {
            ndata++;
            if (!d.getDataType().getName().equals("undefined")) {
                ndefined++;
            }
        }
        say(String.format("instructions       : %d", ninstr));
        say(String.format("defined data items : %d  (%d with a real type)", ndata, ndefined));

        // ---- symbols ----
        SymbolTable st = currentProgram.getSymbolTable();
        long nsym = 0, nprimary = 0;
        SymbolIterator sit = st.getAllSymbols(true);
        while (sit.hasNext()) {
            Symbol s = sit.next();
            nsym++;
            if (s.isPrimary()) {
                nprimary++;
            }
        }
        say(String.format("symbols            : %d  (%d primary)", nsym, nprimary));
        say("");

        // ---- the number that governs the harness ----
        // Every byte of .text has to be emitted in address order. Bytes no function covers are
        // still real content - jump tables, padding, stubs - and if the exporter drops or pads them
        // the section changes length and, with no relocation table, every absolute address baked
        // into the code afterwards points somewhere wrong.
        MemoryBlock text = currentProgram.getMemory().getBlock(".text");
        if (text != null) {
            long total = text.getSize();
            long covered = 0;
            long gaps = 0;
            Address a = text.getStart();
            Address end = text.getEnd();
            boolean inGap = false;
            long gapBytes = 0;
            List<long[]> biggest = new ArrayList<>();
            long gapStart = 0;
            while (a != null && a.compareTo(end) <= 0) {
                Function f = listing.getFunctionContaining(a);
                if (f != null) {
                    if (inGap) {
                        biggest.add(new long[]{gapStart, a.getOffset() - gapStart});
                        inGap = false;
                    }
                    long len = f.getBody().getMaxAddress().getOffset() - a.getOffset() + 1;
                    covered += len;
                    a = a.add(len);
                    continue;
                }
                if (!inGap) {
                    inGap = true;
                    gapStart = a.getOffset();
                    gaps++;
                }
                gapBytes++;
                if (a.equals(end)) {
                    break;
                }
                a = a.add(1);
            }
            if (inGap) {
                biggest.add(new long[]{gapStart, end.getOffset() - gapStart + 1});
            }
            say(String.format(".text              : %d bytes", total));
            say(String.format("  covered by funcs : %d  (%.2f%%)", covered, 100.0 * covered / total));
            say(String.format("  NOT covered      : %d in %d runs  (%.2f%%)",
                    gapBytes, gaps, 100.0 * gapBytes / total));
            biggest.sort((x, y) -> Long.compare(y[1], x[1]));
            say("  largest uncovered runs (these must still be emitted, in order):");
            for (int i = 0; i < Math.min(12, biggest.size()); i++) {
                say(String.format("     0x%08X  %6d bytes", biggest.get(i)[0], biggest.get(i)[1]));
            }
        }

        PrintWriter w = new PrintWriter(new File(outDir, "census.txt"), "UTF-8");
        w.print(out);
        w.close();
        say("");
        say("wrote " + new File(outDir, "census.txt"));
    }
}
