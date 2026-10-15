// Catalogue-level call evidence; computed calls remain an explicit open boundary.
// @category FS15
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import java.nio.file.*;
import java.nio.charset.StandardCharsets;
import java.io.BufferedWriter;
import java.util.*;

public class ExportDependencies extends GhidraScript {
    private String csv(String value){return "\""+value.replace("\"","\"\"")+"\"";}
    @Override public void run() throws Exception {
        Path out=Paths.get(getScriptArgs()[0]);Files.createDirectories(out);
        Path temporary=out.resolve("dependencies.csv.tmp");int count=0;
        try(BufferedWriter writer=Files.newBufferedWriter(temporary,StandardCharsets.UTF_8)) {
            writer.write("entry,external,thunk,call_sites,computed_call_sites,callees\n");
            FunctionIterator functions=currentProgram.getFunctionManager().getFunctions(true);
            while(functions.hasNext()) {
                monitor.checkCancelled();Function function=functions.next();
                int calls=0,computed=0;
                if(!function.isExternal()) {
                    InstructionIterator instructions=currentProgram.getListing().getInstructions(function.getBody(),true);
                    while(instructions.hasNext()) {
                        Instruction instruction=instructions.next();
                        if(instruction.getFlowType().isCall()) {
                            calls++;if(instruction.getFlowType().isComputed())computed++;
                        }
                    }
                }
                TreeSet<String> callees=new TreeSet<>();
                for(Function callee:function.getCalledFunctions(monitor))callees.add(callee.getEntryPoint().toString());
                if(function.isThunk()) {
                    Function target=function.getThunkedFunction(false);
                    if(target!=null)callees.add(target.getEntryPoint().toString());
                }
                writer.write(csv(function.getEntryPoint().toString())+","+function.isExternal()+","+function.isThunk()+","+
                    calls+","+computed+","+csv(String.join(";",callees))+"\n");count++;
            }
        }
        Files.move(temporary,out.resolve("dependencies.csv"),StandardCopyOption.REPLACE_EXISTING);
        Files.writeString(out.resolve("dependencies-complete.txt"),Integer.toString(count)+"\n",StandardCharsets.UTF_8);
        println("Dependency evidence exported for "+count+" catalogued functions. Computed calls are not resolved by this export.");
    }
}
