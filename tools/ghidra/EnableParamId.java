// Ghidra headless pre-script: turns on the analyses that recover function
// parameters (off by default), which matter for Borland cdecl code where
// arguments are pushed and the caller pops them.
//@category Edison

import ghidra.app.script.GhidraScript;

public class EnableParamId extends GhidraScript {
    @Override
    protected void run() throws Exception {
        setAnalysisOption(currentProgram, "Decompiler Parameter ID", "true");
    }
}
