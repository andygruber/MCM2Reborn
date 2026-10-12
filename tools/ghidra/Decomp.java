// Decompile the functions that reference a given string, or a list of addresses.
//
// The fastest way into unfamiliar code in a binary with no symbols is a string it prints. This
// takes a substring, finds every literal containing it, and decompiles whatever references it -
// which is usually the exact function that does the thing the string describes.
//
// Usage:  -postScript Decomp.java <outDir> str:FullScreen
//         -postScript Decomp.java <outDir> 0x004CDBF0 0x004CDCA0
//
// @category OpenMCM2

import java.io.File;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.Reference;

public class Decomp extends GhidraScript {

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        String outDir = args.length > 0 ? args[0] : "C:/mcm2ghidra/out";
        new File(outDir).mkdirs();

        Set<Function> targets = new LinkedHashSet<>();

        for (int i = 1; i < args.length; i++) {
            String a = args[i];
            if (a.startsWith("str:")) {
                String needle = a.substring(4);
                byte[] pat = needle.getBytes("ASCII");
                for (MemoryBlock b : currentProgram.getMemory().getBlocks()) {
                    if (!b.isInitialized() || b.isExecute()) {
                        continue;
                    }
                    byte[] buf = new byte[(int) b.getSize()];
                    b.getBytes(b.getStart(), buf);
                    long base = b.getStart().getOffset();
                    for (int p = 0; p + pat.length <= buf.length; p++) {
                        boolean hit = true;
                        for (int k = 0; k < pat.length; k++) {
                            if (buf[p + k] != pat[k]) {
                                hit = false;
                                break;
                            }
                        }
                        if (!hit) {
                            continue;
                        }
                        // back up to the start of the literal so the reference is found
                        int s = p;
                        while (s > 0 && buf[s - 1] >= 32 && buf[s - 1] < 127 && p - s < 120) {
                            s--;
                        }
                        Address at = toAddr(base + s);
                        for (Reference r : getReferencesTo(at)) {
                            Function f = getFunctionContaining(r.getFromAddress());
                            if (f != null) {
                                println("string \"" + needle + "\" at " + at + " referenced by "
                                        + f.getName() + " @ " + f.getEntryPoint());
                                targets.add(f);
                            }
                        }
                    }
                }
            } else {
                Address at = toAddr(Long.parseLong(a.replace("0x", ""), 16));
                Function f = getFunctionContaining(at);
                if (f != null) {
                    targets.add(f);
                }
            }
        }

        if (targets.isEmpty()) {
            println("no functions matched");
            return;
        }

        DecompInterface di = new DecompInterface();
        di.openProgram(currentProgram);
        di.setSimplificationStyle("decompile");

        PrintWriter w = new PrintWriter(new File(outDir, "decomp.c"), "UTF-8");
        int n = 0;
        for (Function f : targets) {
            DecompileResults res = di.decompileFunction(f, 60, monitor);
            w.println("// ================= " + f.getName() + " @ " + f.getEntryPoint()
                      + "  (" + f.getBody().getNumAddresses() + " bytes) =================");
            if (res != null && res.decompileCompleted() && res.getDecompiledFunction() != null) {
                w.println(res.getDecompiledFunction().getC());
            } else {
                w.println("// decompilation failed: "
                          + (res == null ? "null" : res.getErrorMessage()));
            }
            w.println();
            n++;
        }
        w.close();
        di.dispose();
        println("decompiled " + n + " function(s) to " + new File(outDir, "decomp.c"));
    }
}
