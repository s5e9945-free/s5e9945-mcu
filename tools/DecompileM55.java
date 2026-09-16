import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.lang.Register;
import java.math.BigInteger;

public class DecompileM55 extends GhidraScript {
    public void run() throws Exception {
        Register tmode = currentProgram.getLanguage().getRegister("TMode");
        if (tmode != null)
            currentProgram.getProgramContext().setValue(tmode, toAddr(0x8000), toAddr(0x9347), BigInteger.ONE);
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        String[] args = getScriptArgs();
        int[] starts;
        if (args.length == 0) {
            starts = new int[] {0x88fc, 0x89cc, 0x8a18};
        } else {
            starts = new int[args.length];
            for (int i = 0; i < args.length; ++i)
                starts[i] = Integer.decode(args[i]);
        }
        for (int start : starts) {
            Address address = toAddr(start);
            disassemble(address);
            Function f = getFunctionAt(address);
            if (f == null) f = createFunction(address, String.format("dm_probe_%04x", start));
            println(String.format("FUNCTION %04x %s", start, f));
            if (f != null) {
                DecompileResults result = decompiler.decompileFunction(f, 60, monitor);
                println(result.decompileCompleted() ? result.getDecompiledFunction().getC() : result.getErrorMessage());
            }
        }
        decompiler.dispose();
    }
}
