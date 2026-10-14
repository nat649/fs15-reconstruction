// Export a traceable catalogue plus selected pseudocode. This is NOT compilable source.
// @category FS15
import ghidra.app.util.headless.HeadlessScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.util.*;

public class ExportAnalysis extends HeadlessScript {
    private String csv(String value) { return "\"" + value.replace("\"", "\"\"").replace("\n", "\\n").replace("\r", "\\r") + "\""; }
    private BufferedWriter writer(Path path) throws IOException { return Files.newBufferedWriter(path, StandardCharsets.UTF_8); }
    private void select(Map<Address,String> selected, Function function, String reason) {
        if (function != null && !function.isExternal() && !function.isThunk()) {
            selected.merge(function.getEntryPoint(), reason, (a,b) -> a.contains(b) ? a : a + "; " + b);
        }
    }
    @Override public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 1) throw new IllegalArgumentException("Output directory required");
        Path out = Paths.get(args[0]);
        Files.createDirectories(out);
        int limit = args.length > 1 ? Integer.parseInt(args[1]) : 200;
        boolean targetsOnly = args.length > 2;
        FunctionManager fm = currentProgram.getFunctionManager();
        Map<Address,String> selected = new LinkedHashMap<>();
        int count = 0;
        try (BufferedWriter file = writer(out.resolve("functions.csv"))) {
            file.write("entry,name,size,thunk,external,prototype\n");
            FunctionIterator functions = fm.getFunctions(true);
            while (functions.hasNext() && !monitor.isCancelled()) {
                Function f = functions.next(); count++;
                file.write(csv(f.getEntryPoint().toString()) + "," + csv(f.getName()) + "," + f.getBody().getNumAddresses() + "," + f.isThunk() + "," + f.isExternal() + "," + csv(f.getPrototypeString(false,false)) + "\n");
                if (limit < 0 && !targetsOnly) select(selected, f, "full export");
            }
        }
        for (Address addr : currentProgram.getSymbolTable().getExternalEntryPointIterator()) {
            if (!targetsOnly) select(selected, fm.getFunctionAt(addr), "entry point");
        }
        try (BufferedWriter refs = writer(out.resolve("string-xrefs.csv"))) {
            refs.write("string_address,text,reference_address,function\n");
            DataIterator data = currentProgram.getListing().getDefinedData(true);
            while (data.hasNext() && !monitor.isCancelled()) {
                Data item = data.next(); Object value = item.getValue();
                if (!(value instanceof String)) continue;
                String text = (String)value;
                boolean relevant = text.toLowerCase().contains(".gar") || text.toLowerCase().contains("lua") ||
                    text.contains("RLEUtil") || text.contains("cmdline") || text.contains("startup") ||
                    text.contains("x64") || text.contains("x86") || text.contains("GIANTS Engine");
                ReferenceIterator ri = currentProgram.getReferenceManager().getReferencesTo(item.getAddress());
                while (ri.hasNext()) {
                    Reference ref = ri.next(); Function f = fm.getFunctionContaining(ref.getFromAddress());
                    refs.write(csv(item.getAddress().toString()) + "," + csv(text) + "," + csv(ref.getFromAddress().toString()) + "," + csv(f == null ? "" : f.getEntryPoint().toString()) + "\n");
                    if (!targetsOnly && relevant && selected.size() < 400) select(selected, f, "string: " + text.substring(0, Math.min(100,text.length())));
                }
            }
        }
        // Resolve functions around explicitly supplied native addresses, if requested.
        for (int i = 2; i < args.length; i++) {
            Address address = toAddr(args[i]);
            Function f = fm.getFunctionContaining(address);
            if (f == null) f = fm.getFunctionAt(address);
            select(selected, f, "explicit target " + args[i]);
        }
        // Export direct callees too: this keeps small helpers available for reconstruction.
        List<Address> seeds = new ArrayList<>(selected.keySet());
        for (Address seed : seeds) {
            Function f = fm.getFunctionAt(seed);
            for (Function callee : f.getCalledFunctions(monitor)) {
                if (selected.size() < 600) select(selected, callee, "called by " + seed);
            }
        }
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        int ok = 0, failed = 0, attempted = 0;
        try (BufferedWriter file = writer(out.resolve("decompiled-index.csv"))) {
            file.write("entry,name,status,reason,file\n");
            for (Map.Entry<Address,String> entry : selected.entrySet()) {
                if (monitor.isCancelled() || (limit >= 0 && attempted >= limit)) break;
                Function f = fm.getFunctionAt(entry.getKey());
                if (f == null) continue;
                attempted++;
                DecompileResults result = decompiler.decompileFunction(f, 20, monitor);
                String status = result.decompileCompleted() && result.getDecompiledFunction() != null ? "ok" : "failed";
                String filename = f.getEntryPoint().toString() + "_" + f.getName().replaceAll("[^a-zA-Z0-9_]", "_") + ".c";
                if (status.equals("ok")) {
                    String header = "/* Ghidra pseudocode; not buildable source.\n * Program: " + currentProgram.getName() + "\n * Address: " + f.getEntryPoint() + "\n * Reason: " + entry.getValue().replace("*/", "* /") + "\n */\n\n";
                    Files.writeString(out.resolve(filename), header + result.getDecompiledFunction().getC(), StandardCharsets.UTF_8); ok++;
                } else { failed++; }
                file.write(csv(f.getEntryPoint().toString()) + "," + csv(f.getName()) + "," + status + "," + csv(entry.getValue()) + "," + csv(status.equals("ok") ? filename : result.getErrorMessage()) + "\n");
                file.flush();
                if (attempted % 50 == 0) println("Exported " + attempted + " selected functions");
            }
        } finally { decompiler.dispose(); }
        String summary = "Program: " + currentProgram.getName() + "\nImage base: " + currentProgram.getImageBase() + "\nFunctions catalogued: " + count + "\nSelected: " + selected.size() + "\nDecompiled: " + ok + "\nFailed: " + failed + "\nAnalysis timed out: " + analysisTimeoutOccurred() + "\n";
        Files.writeString(out.resolve("summary.txt"), summary, StandardCharsets.UTF_8);
        println(summary);
    }
}
