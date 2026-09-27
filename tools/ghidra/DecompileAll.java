// Ghidra headless post-script: decompiles every function of the current
// program into one C file.
//
// Usage (see tools/ghidra/decompile.sh):
//   analyzeHeadless <project dir> <name> -import FILE -postScript DecompileAll.java OUT.c [ORDINALS DIR [ENTRIES]]
// Imports Ghidra knows only by ordinal are renamed first from
// ORDINALS DIR/<LIB>.txt ("ordinal name" lines, see ordinals.py).
// Functions Ghidra's analysis missed (only reached through pointers) are
// created from ENTRIES ("segment offset" lines from tools/nedis.py), if given.
//@category Edison

import java.io.File;
import java.io.PrintWriter;
import java.nio.file.Files;
import java.util.HashMap;
import java.util.Map;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.ExternalLocation;
import ghidra.program.model.symbol.SourceType;

public class DecompileAll extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String out = getScriptArgs().length > 0 ? getScriptArgs()[0] : "decompiled.c";
        if (getScriptArgs().length > 1) nameOrdinals(new File(getScriptArgs()[1]));
        if (getScriptArgs().length > 2) seedFunctions(new File(getScriptArgs()[2]));
        DecompInterface ifc = new DecompInterface();
        ifc.setOptions(new DecompileOptions());
        ifc.openProgram(currentProgram);
        int ok = 0, failed = 0;
        try (PrintWriter w = new PrintWriter(out, "UTF-8")) {
            for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
                if (monitor.isCancelled()) break;
                DecompileResults r = ifc.decompileFunction(f, 120, monitor);
                w.println("// ==== " + f.getName() + " @ " + f.getEntryPoint());
                if (r != null && r.decompileCompleted()) {
                    w.println(r.getDecompiledFunction().getC());
                    ok++;
                } else {
                    w.println("// decompilation failed: " + (r == null ? "?" : r.getErrorMessage()));
                    failed++;
                }
            }
        }
        ifc.dispose();
        println("decompiled " + ok + " functions (" + failed + " failed) -> " + out);
    }

    private void nameOrdinals(File dir) throws Exception {
        Map<String, Map<Integer, String>> tables = new HashMap<>();
        int renamed = 0;
        for (Function f : currentProgram.getFunctionManager().getExternalFunctions()) {
            ExternalLocation loc = f.getExternalLocation();
            String name = f.getName();
            if (!name.startsWith("Ordinal_")) continue;
            String lib = loc.getLibraryName().toUpperCase();
            Map<Integer, String> table = tables.computeIfAbsent(lib, l -> {
                Map<Integer, String> t = new HashMap<>();
                File file = new File(dir, l + ".txt");
                try {
                    if (file.exists())
                        for (String line : Files.readAllLines(file.toPath())) {
                            String[] p = line.trim().split(" ");
                            if (p.length == 2) t.put(Integer.parseInt(p[0]), p[1]);
                        }
                } catch (Exception e) { /* no table */ }
                return t;
            });
            String real = table.get(Integer.parseInt(name.substring(8)));
            if (real != null) {
                f.setName(lib + "_" + real, SourceType.IMPORTED);
                renamed++;
            }
        }
        println("named " + renamed + " imports by ordinal");
    }

    // nedis numbers segments from 1; Ghidra's NE loader puts segment n at
    // selector 0x1000 + 8 * (n - 1).
    private void seedFunctions(File entries) throws Exception {
        if (!entries.exists()) return;
        int created = 0;
        for (String line : Files.readAllLines(entries.toPath())) {
            String[] p = line.trim().split(" ");
            if (p.length != 2) continue;
            int seg = 0x1000 + 8 * (Integer.parseInt(p[0]) - 1);
            ghidra.program.model.address.Address a =
                toAddr(String.format("%04x:%s", seg, p[1]));
            if (a == null || getFunctionAt(a) != null) continue;
            if (getInstructionAt(a) == null) disassemble(a);
            if (createFunction(a, null) != null) created++;
        }
        println("created " + created + " functions from " + entries.getName());
    }
}
