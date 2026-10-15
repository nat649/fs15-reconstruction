-- Compile/load both chunks without executing either game's top-level code.
local original, originalError = loadfile(arg[1])
if not original then
    io.stderr:write("Original bytecode: " .. tostring(originalError) .. "\n")
    os.exit(2)
end
local reconstructed, reconstructedError = loadfile(arg[2])
if not reconstructed then
    io.stderr:write("Reconstructed source: " .. tostring(reconstructedError) .. "\n")
    os.exit(3)
end
-- The pinned LuaJIT writer's d mode sorts constant-table hash entries.
-- Without it, VM hash randomization can change a result across processes even
-- when source hashes are unchanged. All instructions/constants remain compared.
local referenceDump = string.dump(original, "sd")
local rebuiltDump = string.dump(reconstructed, "sd")
io.write(referenceDump == rebuiltDump and "exact_match\n" or "syntax_only\n")
