// DecompilePoints.java - descompila funciones por RVA (EXP-014 H-6).
// Uso: -postScript DecompilePoints.java <outPath>
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import java.io.FileWriter;
import java.io.PrintWriter;

public class DecompilePoints extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        String outPath = args[0];
        long[] rvas = { 0x1580L, 0x2180L, 0x21B0L, 0x1F40L, 0x2010L,
                        0x1D70L, 0x1A20L };
        String[] names = { "IsActivated", "AcvFx_IsLicenseStatus",
                           "AcvFx_IsActivationErrorDlg", "AcvFx_DispActivationToolMenu",
                           "AcvFx_DispActivationToolActivation",
                           "AcvFx_IsActivationDialog", "AcvFx_IsActivationDialog2" };
        DecompInterface dec = new DecompInterface();
        dec.openProgram(currentProgram);
        PrintWriter w = new PrintWriter(new FileWriter(outPath));
        w.println("// Descompilacion de ActivationFxMb.dll (EXP-014, H-6) - Ghidra "
                  + ghidra.framework.Application.getApplicationVersion());
        w.println("// Program: " + currentProgram.getName()
                  + "  ImageBase: " + currentProgram.getImageBase());
        for (int i = 0; i < rvas.length; i++) {
            Address addr = currentProgram.getImageBase().add(rvas[i]);
            Function f = getFunctionAt(addr);
            if (f == null) {
                f = getFunctionContaining(addr);
            }
            w.println();
            w.println("// ===== RVA 0x" + Long.toHexString(rvas[i]) + " ("
                      + names[i] + ") =====");
            if (f == null) {
                w.println("// NO FUNCTION at this address");
                continue;
            }
            w.println("// Function: " + f.getName() + " @ " + f.getEntryPoint());
            DecompileResults r = dec.decompileFunction(f, 90, monitor);
            if (r != null && r.decompileCompleted()
                    && r.getDecompiledFunction() != null) {
                w.println(r.getDecompiledFunction().getC());
            } else {
                w.println("// decompile failed: "
                          + (r != null ? r.getErrorMessage() : "null"));
            }
        }
        w.close();
        println("DecompilePoints done -> " + outPath);
    }
}
