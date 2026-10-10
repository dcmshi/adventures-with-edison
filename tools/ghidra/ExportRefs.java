// Writes every reference Ghidra's analysis found to an address in an
// executable block (code), one a line: "TO FROM TYPE", addresses as
// Ghidra's segment:offset. For tools/testing/ghidrarefs.py's cross-check of
// deadscan.py. Usage (headless, after the program's analysis):
//   -process -noanalysis -readOnly -postScript ExportRefs.java OUT
//@category Edison
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressIterator;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceManager;
import java.io.PrintWriter;

public class ExportRefs extends GhidraScript {
    @Override
    public void run() throws Exception {
        String out = getScriptArgs()[0];
        ReferenceManager rm = currentProgram.getReferenceManager();
        int n = 0;
        try (PrintWriter w = new PrintWriter(out, "UTF-8")) {
            AddressIterator it = rm.getReferenceDestinationIterator(currentProgram.getMemory(), true);
            while (it.hasNext()) {
                Address to = it.next();
                MemoryBlock b = currentProgram.getMemory().getBlock(to);
                if (b == null || !b.isExecute()) continue;
                for (Reference r : rm.getReferencesTo(to)) {
                    w.println(to + " " + r.getFromAddress() + " " + r.getReferenceType());
                    n++;
                }
            }
        }
        println("ExportRefs: " + n + " references to code in " + out);
    }
}
