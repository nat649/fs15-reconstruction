// Resumable full catalogue export. Output remains local recovered pseudocode.
// @category FS15
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import java.nio.file.*;
import java.nio.charset.StandardCharsets;
import java.io.*;

public class ExportFullProgram extends GhidraScript {
    private void replace(Path temp, Path target) throws Exception {
        for (int attempt=0; ; attempt++) {
            try { Files.move(temp,target,StandardCopyOption.REPLACE_EXISTING); return; }
            catch (FileSystemException busy) {
                if (attempt>=39) throw busy;
                Thread.sleep(25); // A Windows progress reader can briefly deny rename.
            }
        }
    }
    private String csv(String s) { return "\"" + s.replace("\"", "\"\"").replace("\n", " ").replace("\r", " ") + "\""; }
    private void progress(Path out, int total, int eligible, int processed, int ok, int failed, int reused, boolean complete) throws Exception {
        String json = "{\n  \"program\": " + csv(currentProgram.getName()) + ",\n" +
            "  \"catalogued\": " + total + ",\n  \"eligible\": " + eligible + ",\n  \"processed\": " + processed +
            ",\n  \"exported\": " + ok + ",\n  \"failed\": " + failed + ",\n  \"reused\": " + reused +
            ",\n  \"complete_pass\": " + complete + ",\n  \"scope\": \"Ghidra pseudocode; not compilable recovered engine source\"\n}\n";
        Path temp = out.resolve("progress.tmp");
        Files.writeString(temp, json, StandardCharsets.UTF_8);
        replace(temp, out.resolve("progress.json"));
    }
    @Override public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 1) throw new IllegalArgumentException("Output directory required");
        Path out = Paths.get(args[0]); Files.createDirectories(out);
        int timeout = args.length > 1 ? Integer.parseInt(args[1]) : 10;
        if (timeout < 1 || timeout > 600) throw new IllegalArgumentException("Timeout must be 1..600 seconds");
        int total = 0, eligible = 0, processed = 0, ok = 0, failed = 0, reused = 0;
        FunctionManager fm = currentProgram.getFunctionManager();
        FunctionIterator count = fm.getFunctions(true);
        try(BufferedWriter catalogue=Files.newBufferedWriter(out.resolve("functions.csv"),StandardCharsets.UTF_8)) {
            catalogue.write("entry,name,size,thunk,external,prototype\n");
            while(count.hasNext()) {
                Function f=count.next();total++;if(!f.isExternal()&&!f.isThunk())eligible++;
                catalogue.write(csv(f.getEntryPoint().toString())+","+csv(f.getName())+","+f.getBody().getNumAddresses()+","+f.isThunk()+","+f.isExternal()+","+csv(f.getPrototypeString(false,false))+"\n");
            }
        }
        DecompInterface decompiler = new DecompInterface();
        // Large registration functions exceeded Ghidra's default response cap.
        // Increase transport capacity; retain normal decompilation semantics.
        DecompileOptions options = new DecompileOptions();
        options.setMaxPayloadMBytes(128);
        decompiler.setOptions(options);
        // This exporter only consumes C text, not HighFunction/AST metadata.
        decompiler.toggleSyntaxTree(false);
        if (!decompiler.openProgram(currentProgram)) throw new IOException("Cannot open decompiler program");
        progress(out,total,eligible,0,0,0,0,false);
        try (BufferedWriter index = Files.newBufferedWriter(out.resolve("decompiled-index.csv"), StandardCharsets.UTF_8)) {
            index.write("entry,name,size,status,file,diagnostic\n");
            FunctionIterator functions = fm.getFunctions(true);
            while (functions.hasNext() && !monitor.isCancelled()) {
                Function f=functions.next();
                if (f.isExternal() || f.isThunk()) continue;
                String filename = f.getEntryPoint() + "_" + f.getName().replaceAll("[^a-zA-Z0-9_]", "_") + ".c";
                Path target=out.resolve(filename);
                String identity="/* Ghidra pseudocode; not buildable source.\n * Program: " + currentProgram.getName() +
                    "\n * Address: " + f.getEntryPoint() + "\n * Export: full catalogue\n */\n\n";
                String status="failed", diagnostic="";
                if (Files.isRegularFile(target) && Files.readString(target).startsWith(identity)) {
                    status="ok"; reused++;
                } else {
                    try {
                        DecompileResults result=decompiler.decompileFunction(f,timeout,monitor);
                        if (result.decompileCompleted() && result.getDecompiledFunction()!=null) {
                            Path temp=out.resolve(filename+".tmp");
                            Files.writeString(temp,identity+result.getDecompiledFunction().getC(),StandardCharsets.UTF_8);
                            replace(temp,target);
                            status="ok";
                        } else diagnostic=result.getErrorMessage();
                    } catch (Exception error) { diagnostic=error.toString(); }
                }
                processed++;if (status.equals("ok")) ok++;else failed++;
                index.write(csv(f.getEntryPoint().toString())+","+csv(f.getName())+","+f.getBody().getNumAddresses()+","+status+","+csv(status.equals("ok")?filename:"")+","+csv(diagnostic)+"\n");
                index.flush();
                if (processed%100==0) {
                    progress(out,total,eligible,processed,ok,failed,reused,false);
                    println("Full export: "+processed+"/"+eligible+"; ok="+ok+" failed="+failed);
                }
            }
        } finally { decompiler.dispose(); }
        progress(out,total,eligible,processed,ok,failed,reused,processed==eligible);
        println("Full export ended: "+ok+" exported, "+failed+" failed, "+reused+" reused");
    }
}
