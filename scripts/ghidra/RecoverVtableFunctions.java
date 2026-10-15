// Explicit, constructor-observed vtable slots only. Never split existing bodies.
// @category FS15
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.MemoryBlock;
import java.nio.file.*;
import java.nio.charset.StandardCharsets;
import java.io.BufferedWriter;

public class RecoverVtableFunctions extends GhidraScript {
    @Override public void run() throws Exception {
        String[] args=getScriptArgs();Path out=Paths.get(args[0]);Files.createDirectories(out);
        FunctionManager manager=currentProgram.getFunctionManager();
        int created=0,slots=0;
        try(BufferedWriter writer=Files.newBufferedWriter(out.resolve("slots.csv"),StandardCharsets.UTF_8)) {
            writer.write("table,slot,target,status,containing_function\n");
            for(int i=1;i<args.length;i++) {
                String[] spec=args[i].split(":");Address table=toAddr(spec[0]);int count=Integer.parseInt(spec[1]);
                if(table==null||count<1||count>128)throw new IllegalArgumentException("Invalid bounded table specification");
                for(int slot=0;slot<count;slot++) {
                    monitor.checkCancelled();
                    Address target=toAddr(currentProgram.getMemory().getLong(table.add(slot*8L)));
                    MemoryBlock block=currentProgram.getMemory().getBlock(target);
                    if(block==null||!block.isExecute()||!block.isInitialized())
                        throw new IllegalArgumentException("Non-executable vtable slot "+table+"+"+slot*8);
                    Function function=manager.getFunctionAt(target);String status="existing_entry";
                    if(function==null) {
                        function=manager.getFunctionContaining(target);status="inside_existing_function";
                        if(function==null) {
                            disassemble(target);function=createFunction(target,null);
                            if(function==null)throw new IllegalArgumentException("Cannot create uncovered target "+target);
                            status="created_from_explicit_vtable";created++;
                        }
                    }
                    writer.write(table+","+slot*8+","+target+","+status+","+function.getEntryPoint()+"\n");slots++;
                }
            }
        }
        Files.writeString(out.resolve("complete.txt"),slots+","+created+"\n",StandardCharsets.UTF_8);
        println("Vtable audit: "+slots+" explicit slots; "+created+" uncovered functions created. Existing bodies were not split.");
    }
}
