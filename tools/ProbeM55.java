import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.lang.Register;
import java.math.BigInteger;

public class ProbeM55 extends GhidraScript {
    public void run() throws Exception {
        Register tmode = currentProgram.getLanguage().getRegister("TMode");
        if (tmode != null) {
            currentProgram.getProgramContext().setValue(tmode, toAddr(0x8000), toAddr(0x9347), BigInteger.ONE);
        }
        int[] starts = {0x8000, 0x80ac, 0x88fc, 0x8a18, 0x8d74, 0x8e26,
                        0x8f90, 0x9088, 0x925e, 0x9312};
        for (int start : starts) {
            Address address = toAddr(start);
            boolean ok = disassemble(address);
            println(String.format("START %04x disassemble=%s", start, ok));
            for (int off = 0; off < 24; off += 2) {
                Address at = toAddr(start + off);
                Instruction insn = getInstructionAt(at);
                if (insn != null)
                    println(String.format("%04x %s", start + off, insn.toString()));
            }
        }
        byte[] b = new byte[0x10a8];
        currentProgram.getMemory().getBytes(toAddr(0x9410), b);
        println(String.format("CONFIG count=%d first_field=%02x%02x%02x%02x",
            (b[0]&255)|((b[1]&255)<<8)|((b[2]&255)<<16)|((b[3]&255)<<24),
            b[0x7b]&255,b[0x7a]&255,b[0x79]&255,b[0x78]&255));
    }
}
