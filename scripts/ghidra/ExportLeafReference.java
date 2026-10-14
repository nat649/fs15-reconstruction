// Validate and export one self-contained native leaf for local differential tests.
// @category FS15
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import java.nio.file.*;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.HexFormat;

public class ExportLeafReference extends GhidraScript {
    @Override public void run() throws Exception {
        String[] args = getScriptArgs();
        Address entry = toAddr(args[0]);
        Path out = Paths.get(args[1]);
        Files.createDirectories(out);
        Function f = currentProgram.getFunctionManager().getFunctionAt(entry);
        if (f == null || f.isThunk() || f.isExternal()) throw new IllegalArgumentException("Not a local native function");
        Address end = f.getBody().getMaxAddress();
        int length = (int)end.subtract(entry) + 1;
        StringBuilder listing = new StringBuilder();
        InstructionIterator instructions = currentProgram.getListing().getInstructions(f.getBody(), true);
        int count = 0;
        while (instructions.hasNext()) {
            Instruction instruction = instructions.next();
            if (instruction.getFlowType().isCall()) throw new IllegalArgumentException("Reference calls another function: " + instruction);
            if (instruction.toString().toUpperCase().contains("RIP")) throw new IllegalArgumentException("RIP-relative reference requires relocation: " + instruction);
            for (Reference ref : instruction.getReferencesFrom()) {
                Address target = ref.getToAddress();
                if (target.isMemoryAddress() && !f.getBody().contains(target)) {
                    throw new IllegalArgumentException("Reference leaves function: " + instruction + " -> " + target);
                }
            }
            listing.append(instruction.getAddress()).append("  ").append(instruction.toString()).append("\n");
            count++;
        }
        byte[] code = new byte[length];
        currentProgram.getMemory().getBytes(entry, code);
        String hash = HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(code));
        Files.write(out.resolve("rle8-reference.bin"), code);
        Files.writeString(out.resolve("rle8-reference.asm"), listing.toString(), StandardCharsets.UTF_8);
        String json = "{\n  \"program\": \"" + currentProgram.getName() + "\",\n  \"entry\": \"" + entry + "\",\n  \"length\": " + length + ",\n  \"instruction_count\": " + count + ",\n  \"sha256\": \"" + hash + "\",\n  \"external_calls\": false,\n  \"external_memory_references\": false\n}\n";
        Files.writeString(out.resolve("rle8-reference.json"), json, StandardCharsets.UTF_8);
        println(json);
    }
}
