-- Run the recovered curve setup and AnimCurve evaluator locally. This does not
-- execute Environment:new or fabricate unavailable mission/engine services.
local root, output = assert(arg[1]), assert(arg[2])
local scripts = root .. "/recovered/lua/dataS/scripts/"
assert(loadfile(scripts .. "shared/class.lua"))()
assert(loadfile(scripts .. "Utils.lua"))()
assert(loadfile(scripts .. "AnimCurve.lua"))()
local file = assert(io.open(scripts .. "environment/Environment.lua", "rb"))
local original = file:read("*a"); file:close()
original = original:gsub("\r\n", "\n")
local first = assert(original:find("local ambientMultiplier =", 1, true))
local last = assert(original:find("\n\tself.allowRain =", first, true))
-- The final 'end' in this source slice closes the original dayNightCycle block.
local setup = assert(loadstring("local self={}\nif true then\n" .. original:sub(first,last-1) .. "\nreturn self", "@recovered/Environment.curves"))
local environment = setup()
local ffi = require("ffi")
local values = ffi.new("float[?]",1441*15)
local at = 0
for minute=0,1440 do
    for _, item in ipairs({{"skyCurve",4},{"ambientCurve",3},{"diffuseCurve",3},{"distanceFogCurve",4},{"rotCurve",1}}) do
        local result = {environment[item[1]]:get(minute)}
        for i=1,item[2] do
            local v=assert(result[i]); assert(v==v and v~=math.huge and v~=-math.huge)
            values[at]=v;at=at+1
        end
    end
end
local target = assert(io.open(output,"wb"))
target:write("FS15ENV1", string.char(161,5,0,0), ffi.string(values,1441*15*4));target:close()
print("Recovered Environment curve setup and AnimCurve sampled at 1441 original minute positions")
