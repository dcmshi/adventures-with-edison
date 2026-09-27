// Ghidra headless post-script: decompiles every function of the current
// program into one C file.
//
// Usage (see tools/ghidra/decompile.sh):
//   analyzeHeadless <project dir> <name> -import FILE -postScript DecompileAll.java OUT.c [ORDINALS DIR [ENTRIES [VOLATILE]]]
// Imports Ghidra knows only by ordinal are renamed first from
// ORDINALS DIR/<LIB>.txt ("ordinal name" lines, see ordinals.py).
// Functions Ghidra's analysis missed (only reached through pointers) are
// created from ENTRIES ("segment offset" lines from tools/nedis.py), if given.
// Data-segment variables listed in VOLATILE (see volatile.txt) are marked
// volatile, so busy-waits on timer-driven variables decompile as loops.
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
        if (getScriptArgs().length > 3) markVolatile(new File(getScriptArgs()[3]));
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

    // Each listed range of the data segment is split off into its own
    // memory block, marked volatile.
    private void markVolatile(File list) throws Exception {
        if (!list.exists()) return;
        String program = currentProgram.getName().toLowerCase().replaceAll("[.][^.]*$", "");
        ghidra.program.model.mem.Memory mem = currentProgram.getMemory();
        // DGROUP is the last NE segment; the loader puts segment n at
        // selector 0x1000 + 8 * (n - 1), and the segment count is in the
        // NE header (offset 0x1C), found through the MZ header at 0x3C.
        String path = currentProgram.getExecutablePath();
        if (path.matches("/[A-Za-z]:.*")) path = path.substring(1);  // "/D:/..." on Windows
        byte[] exe = Files.readAllBytes(new File(path).toPath());
        int ne = (exe[0x3C] & 0xFF) | (exe[0x3D] & 0xFF) << 8;
        int segments = (exe[ne + 0x1C] & 0xFF) | (exe[ne + 0x1D] & 0xFF) << 8;
        int selector = 0x1000 + 8 * (segments - 1);
        int marked = 0;
        for (String line : Files.readAllLines(list.toPath())) {
            String[] p = line.trim().split("\s+");
            if (p.length < 3 || p[0].startsWith("#") || !p[0].equals(program)) continue;
            int offset = Integer.parseInt(p[1], 16), length = Integer.parseInt(p[2]);
            ghidra.program.model.address.Address a = toAddr(String.format("%04x:%04x", selector, offset));
            ghidra.program.model.address.Address end = a.add(length);
            ghidra.program.model.mem.MemoryBlock b = mem.getBlock(a);
            if (b == null) continue;
            if (!b.getStart().equals(a)) { mem.split(b, a); b = mem.getBlock(a); }
            if (b.contains(end)) mem.split(b, end);
            mem.getBlock(a).setVolatile(true);
            marked++;
        }
        println("marked " + marked + " volatile ranges in " + String.format("%04x", selector));
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
