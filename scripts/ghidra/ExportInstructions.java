// Local-only instruction evidence for recovering exact math order and calls.
// @category FS15
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import java.nio.file.*;
import java.nio.charset.StandardCharsets;

public class ExportInstructions extends GhidraScript {
    @Override public void run() throws Exception {
        String[] args=getScriptArgs();
        Path out=Paths.get(args[0]);Files.createDirectories(out);
        for(int i=1;i<args.length;i++) {
            Address address=toAddr(args[i]);
            if(address==null)throw new IllegalArgumentException("Invalid address: "+args[i]);
            Function function=currentProgram.getFunctionManager().getFunctionAt(address);
            if(function==null||function.isExternal()||function.isThunk())
                throw new IllegalArgumentException("Expected catalogued native function: "+address);
            StringBuilder text=new StringBuilder("Program: "+currentProgram.getName()+"\nFunction: "+address+"\n");
            InstructionIterator instructions=currentProgram.getListing().getInstructions(function.getBody(),true);
            while(instructions.hasNext()) {
                monitor.checkCancelled();
                Instruction instruction=instructions.next();
                text.append(instruction.getAddress()).append("  ").append(instruction).append("\n");
            }
            Files.writeString(out.resolve(address+".asm"),text.toString(),StandardCharsets.UTF_8);
            println("Local instruction evidence: "+address);
        }
        Files.writeString(out.resolve("instructions-complete.txt"),String.join("\n",args)+"\n",StandardCharsets.UTF_8);
    }
}
