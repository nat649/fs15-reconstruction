-- Authored comparison fixture; this is not an actual GE6 session.
assert(loadfile(arg[1]))()
local original = WheelsUtil
local saved = {getTireType=original.getTireType, getTireFriction=original.getTireFriction, getGroundType=original.getGroundType}
local listener
local mod = setmetatable({addModEventListener=function(value) listener=value end}, {__index=_G})
mod._G = mod
local function source(name)
    local chunk = assert(loadfile(arg[2] .. '/' .. name))
    return setfenv(chunk, mod)()
end
source('bootstrap.lua')
source('recovered/WheelsUtil.lua')
assert(not pcall(source, 'activate.lua'), 'Activation accepted missing parity evidence')
assert(original.getTireType == saved.getTireType, 'Rejected activation changed gameplay')
source('verify.lua')
source('activate.lua')
assert(mod.FS15NativeProbe.gameplayActivated and listener)
assert(WheelsUtil == original, 'Activation replaced the global class table')
local comparisons = 0
for _, name in ipairs({'mud','offRoad','street','crawler'}) do
    local tire = original.getTireType(name)
    assert(tire == saved.getTireType(name))
    for ground=1,4 do
        for _,wet in ipairs({0,0.25,0.5,1}) do
            assert(original.getTireFriction(tire,ground,wet) == saved.getTireFriction(tire,ground,wet))
            comparisons = comparisons+1
        end
    end
end
assert(original.getGroundType(false,false,{0,0,0,0.5}) == saved.getGroundType(false,false,{0,0,0,0.5}))
original.registerTireType('authoredCustom',{1.1,1.2,1.3,1.4},{0.7,0.8,0.9,1.0})
local custom = original.getTireType('authoredCustom')
assert(custom == saved.getTireType('authoredCustom'))
assert(original.getTireFriction(custom,4,0.5) == saved.getTireFriction(custom,4,0.5))
listener:deleteMap()
for name,fn in pairs(saved) do assert(original[name] == fn, 'Unload failed to restore original helper') end
listener:loadMap()
assert(mod.FS15NativeProbe.gameplayActivated, 'A second map did not reactivate recovered helpers')
assert(original.getTireType('authoredCustom') == custom)
listener:deleteMap()
listener:loadMap()
local laterMod = function() return 999 end
original.getGroundType = laterMod
listener:deleteMap()
assert(original.getGroundType == laterMod, 'Unload clobbered another mod')
print('AUTHORED_HYBRID_FIXTURE_OK samples=' .. comparisons .. ' custom_tires=true unload=true (not a GIANTS engine boot)')
