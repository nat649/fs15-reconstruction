// Inspect callers, data references, and nearby pointer tables without modifying code.
// @category FS15
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import java.nio.file.*;
import java.nio.charset.StandardCharsets;

public class InspectAddresses extends GhidraScript {
    @Override public void run() throws Exception {
        String[] args = getScriptArgs();
        Path out = Paths.get(args[0]);
        Files.createDirectories(out);
        FunctionManager fm = currentProgram.getFunctionManager();
        for (int i = 1; i < args.length; i++) {
            Address address = toAddr(args[i]);
            StringBuilder text = new StringBuilder("Address: " + address + "\n");
            Function targetFunction = fm.getFunctionContaining(address);
            if (targetFunction != null) {
                text.append("Function: ").append(targetFunction.getEntryPoint()).append(" ").append(targetFunction.getName()).append("\n");
                for (Function callee : targetFunction.getCalledFunctions(monitor)) {
                    text.append("CALLS ").append(callee.getEntryPoint()).append(" ").append(callee.getName()).append("\n");
                }
            }
            ReferenceIterator refs = currentProgram.getReferenceManager().getReferencesTo(address);
            while (refs.hasNext()) {
                Reference r = refs.next();
                Function f = fm.getFunctionContaining(r.getFromAddress());
                text.append("REF ").append(r.getFromAddress()).append(" ").append(r.getReferenceType()).append(" FROM ")
                    .append(f == null ? "data" : f.getEntryPoint() + " " + f.getName()).append("\n");
            }
            for (int offset = 0; offset < 256; offset += 8) {
                Address at = address.add(offset);
                try {
                    long value = currentProgram.getMemory().getLong(at);
                    Address pointer = toAddr(value);
                    Function f = fm.getFunctionAt(pointer);
                    text.append("QWORD ").append(at).append(" = ").append(Long.toUnsignedString(value,16));
                    if (f != null) text.append(" FUNCTION ").append(f.getName());
                    Data d = getDataAt(pointer);
                    if (d != null && d.getValue() instanceof String) text.append(" STRING ").append(d.getValue());
                    text.append("\n");
                } catch (Exception e) { break; }
            }
            Files.writeString(out.resolve(address + ".txt"), text, StandardCharsets.UTF_8);
            println("Inspection written: " + out.resolve(address + ".txt"));
        }
    }
}
