// Attribute every function to the .cpp it was compiled from.
//
// Mcm2.exe carries 115 of its own source paths - `D:\aardvark\VC\krusty2\PCVideoCard.cpp` and so on
// - left behind by assert macros. Each one is referenced from the functions that live in that file,
// so the strings are a free module map: they turn a wall of FUN_004a1bf0 into "this is in
// PCVideoCard.cpp", which is the difference between searching 5,658 functions and searching twenty.
//
// Attribution spreads two ways, and both are needed:
//   DIRECT   - the function itself references the path, because it contains an assert.
//   CALLERS  - most functions contain no assert at all. But an assert helper is called from within
//              one file far more often than across files, so a function with no path of its own is
//              given the module its NEIGHBOURS agree on, by majority of the functions it calls and
//              is called by. This is a heuristic and is reported separately from the direct hits.
//
// Writes modules.txt (one line per function) and a per-module summary.
//
// @category OpenMCM2

import java.io.File;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.data.StringDataInstance;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.DataIterator;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.Listing;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.Reference;

public class Attribute extends GhidraScript {

    static String baseName(String p) {
        int i = Math.max(p.lastIndexOf('\\'), p.lastIndexOf('/'));
        return i >= 0 ? p.substring(i + 1) : p;
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

        // ---- find the source-path strings, by scanning bytes rather than trusting the listing ----
        // Ghidra has not necessarily typed these as strings, and a path that was never typed would
        // simply be invisible to getDefinedData.
        Map<Long, String> pathAt = new HashMap<>();
        for (MemoryBlock b : currentProgram.getMemory().getBlocks()) {
            if (!b.isInitialized() || b.isExecute()) {
                continue;
            }
            byte[] buf = new byte[(int) b.getSize()];
            b.getBytes(b.getStart(), buf);
            long base = b.getStart().getOffset();
            for (int i = 0; i + 8 < buf.length; i++) {
                if (buf[i] != 'a' || buf[i + 1] != 'a' || buf[i + 2] != 'r' || buf[i + 3] != 'd') {
                    continue;
                }
                if (!(buf[i + 4] == 'v' && buf[i + 5] == 'a' && buf[i + 6] == 'r' && buf[i + 7] == 'k')) {
                    continue;
                }
                int s = i;
                while (s > 0 && buf[s - 1] >= 32 && buf[s - 1] < 127 && i - s < 80) {
                    s--;
                }
                int e = i;
                while (e < buf.length && buf[e] >= 32 && buf[e] < 127 && e - i < 80) {
                    e++;
                }
                String p = new String(buf, s, e - s, "ASCII");
                if (p.toLowerCase().endsWith(".cpp") || p.toLowerCase().endsWith(".c")
                        || p.toLowerCase().endsWith(".h")) {
                    pathAt.put(base + s, baseName(p));
                }
                i = e;
            }
        }
        println("source-path strings found: " + pathAt.size());

        // ---- direct attribution: a function that references one ----
        Map<Function, String> module = new HashMap<>();
        int direct = 0;
        for (Map.Entry<Long, String> e : pathAt.entrySet()) {
            Address a = toAddr(e.getKey());
            for (Reference r : getReferencesTo(a)) {
                Function f = getFunctionContaining(r.getFromAddress());
                if (f == null) {
                    continue;
                }
                // A function can reference more than one path only if the compiler inlined across
                // files; first one wins and the count below shows how rare it is.
                if (module.putIfAbsent(f, e.getValue()) == null) {
                    direct++;
                }
            }
        }
        println("functions attributed DIRECTLY: " + direct);

        // ---- spread to neighbours, by majority vote, until it stops changing ----
        List<Function> all = new ArrayList<>();
        FunctionIterator it = currentProgram.getFunctionManager().getFunctions(true);
        while (it.hasNext()) {
            Function f = it.next();
            if (!f.isExternal()) {
                all.add(f);
            }
        }
        int spread = 0;
        for (int pass = 0; pass < 6; pass++) {
            int changed = 0;
            for (Function f : all) {
                if (module.containsKey(f)) {
                    continue;
                }
                Map<String, Integer> votes = new HashMap<>();
                for (Function c : f.getCalledFunctions(monitor)) {
                    String m = module.get(c);
                    if (m != null) {
                        votes.merge(m, 1, Integer::sum);
                    }
                }
                for (Function c : f.getCallingFunctions(monitor)) {
                    String m = module.get(c);
                    if (m != null) {
                        votes.merge(m, 2, Integer::sum);   // a caller is the stronger signal
                    }
                }
                String best = null;
                int bestN = 0;
                for (Map.Entry<String, Integer> v : votes.entrySet()) {
                    if (v.getValue() > bestN) {
                        bestN = v.getValue();
                        best = v.getKey();
                    }
                }
                if (best != null && bestN >= 2) {
                    module.put(f, best);
                    changed++;
                }
            }
            spread += changed;
            println("  pass " + (pass + 1) + ": +" + changed);
            if (changed == 0) {
                break;
            }
        }

        // ---- report ----
        TreeMap<String, List<Function>> byModule = new TreeMap<>();
        for (Map.Entry<Function, String> e : module.entrySet()) {
            byModule.computeIfAbsent(e.getValue(), k -> new ArrayList<>()).add(e.getKey());
        }
        println("");
        println("functions attributed: " + module.size() + " of " + all.size()
                + " (" + direct + " directly, " + spread + " by neighbours)");
        println("modules with functions: " + byModule.size());

        PrintWriter w = new PrintWriter(new File(outDir, "modules.txt"), "UTF-8");
        w.println("# function address, module, name");
        for (Map.Entry<String, List<Function>> e : byModule.entrySet()) {
            List<Function> fs = e.getValue();
            fs.sort((x, y) -> x.getEntryPoint().compareTo(y.getEntryPoint()));
            for (Function f : fs) {
                w.printf("0x%08X %-34s %s%n", f.getEntryPoint().getOffset(), e.getKey(), f.getName());
            }
        }
        w.close();

        // The modules this session actually needs, printed so they can be read without the file.
        String[] interesting = {"PCVideoCard.cpp", "VideoCard.cpp", "PCRenderTarget.cpp",
                                "OptionProcs.cpp", "dlgprocs.cpp", "gameui.cpp", "krustyui.cpp"};
        println("");
        for (String m : interesting) {
            List<Function> fs = byModule.get(m);
            if (fs == null) {
                println(m + ": none");
                continue;
            }
            fs.sort((x, y) -> x.getEntryPoint().compareTo(y.getEntryPoint()));
            StringBuilder sb = new StringBuilder();
            for (int i = 0; i < Math.min(fs.size(), 14); i++) {
                sb.append(String.format("0x%08X ", fs.get(i).getEntryPoint().getOffset()));
            }
            println(String.format("%-20s %3d functions: %s", m, fs.size(), sb.toString()));
        }
        println("");
        println("wrote " + new File(outDir, "modules.txt"));
    }
}
