// Audit explicit native binding targets; create functions only in uncovered executable code.
// @category FS15
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.MemoryBlock;
import java.nio.file.*;
import java.nio.charset.StandardCharsets;
import java.io.*;

public class RecoverBindingFunctions extends GhidraScript {
    private String csv(String value) { return "\""+value.replace("\"","\"\"")+"\""; }
    @Override public void run() throws Exception {
        String[] args=getScriptArgs();
        if(args.length<2)throw new IllegalArgumentException("Output directory and explicit binding targets required");
        Path out=Paths.get(args[0]); Files.createDirectories(out);
        FunctionManager manager=currentProgram.getFunctionManager();
        int created=0;
        try(BufferedWriter file=Files.newBufferedWriter(out.resolve("bindings.csv"),StandardCharsets.UTF_8)) {
            file.write("target,status,containing_function,size\n");
            for(int i=1;i<args.length;i++) {
                Address address=toAddr(args[i]);
                Function function=manager.getFunctionAt(address);
                String status="existing_entry";
                if(function==null) {
                    function=manager.getFunctionContaining(address);
                    status="inside_existing_function";
                    if(function==null) {
                        MemoryBlock block=currentProgram.getMemory().getBlock(address);
                        status="not_executable";
                        if(block!=null && block.isExecute() && block.isInitialized()) {
                            disassemble(address);
                            function=createFunction(address,null);
                            status=function==null?"creation_failed":"created_from_explicit_binding";
                            if(function!=null)created++;
                        }
                    }
                }
                file.write(csv(address.toString())+","+csv(status)+","+csv(function==null?"":function.getEntryPoint().toString())+","+(function==null?0:function.getBody().getNumAddresses())+"\n");
            }
        }
        try(BufferedWriter file=Files.newBufferedWriter(out.resolve("functions.csv"),StandardCharsets.UTF_8)) {
            file.write("entry,name,size,thunk,external,prototype\n");
            FunctionIterator functions=manager.getFunctions(true);
            while(functions.hasNext()) {
                Function function=functions.next();
                file.write(csv(function.getEntryPoint().toString())+","+csv(function.getName())+","+function.getBody().getNumAddresses()+","+function.isThunk()+","+function.isExternal()+","+csv(function.getPrototypeString(false,false))+"\n");
            }
        }
        println("Explicit binding audit: "+(args.length-1)+" targets, "+created+" newly identified functions; existing bodies were not split");
    }
}
