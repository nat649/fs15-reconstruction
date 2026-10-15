-- Compile source into a bytecode file without executing the source chunk.
local chunk, err
if arg[3] then
    local source = assert(io.open(arg[1], "rb"))
    local text = source:read("*a")
    source:close()
    chunk, err = loadstring(text, arg[3])
else
    chunk, err = loadfile(arg[1])
end
assert(chunk, err)
local output = assert(io.open(arg[2], "wb"))
output:write(string.dump(chunk, "d"))
output:close()
