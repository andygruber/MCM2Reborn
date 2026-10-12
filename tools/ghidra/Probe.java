// Print everything Ghidra knows about a handful of addresses. Used to settle "what is actually
// there" questions without inferring them from the exported assembly.
//
// @category OpenMCM2

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressRange;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.Symbol;

public class Probe extends GhidraScript {

    @Override
    public void run() throws Exception {
        for (String s : getScriptArgs()) {
            long v = Long.parseLong(s.replace("0x", ""), 16);
            Address a = toAddr(v);
            println("---- 0x" + Long.toHexString(v) + " ----");
            Function f = getFunctionContaining(a);
            if (f == null) {
                println("  no function contains it");
            } else {
                println("  function        : " + f.getName());
                println("  entry           : " + f.getEntryPoint());
                println("  body            : " + f.getBody().getNumAddressRanges() + " range(s)");
                for (AddressRange r : f.getBody()) {
                    println("      " + r.getMinAddress() + " .. " + r.getMaxAddress());
                }
            }
            Symbol[] syms = getSymbols(null, null) == null ? null : null;
            Symbol prim = getSymbolAt(a);
            println("  primary symbol  : " + (prim == null ? "(none)" : prim.getName()));
            int nref = 0, ncall = 0;
            for (Reference r : getReferencesTo(a)) {
                nref++;
                if (r.getReferenceType().isCall()) {
                    ncall++;
                }
            }
            println("  references to it: " + nref + " (" + ncall + " calls)");
        }
    }
}
