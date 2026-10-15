-- Authored harness reproducing the observed mod-environment source ordering.
-- This runs in the verification VM, not in the actual GIANTS engine.
assert(loadfile(arg[1]))()
local original = WheelsUtil
local mod = setmetatable({}, {__index=_G})
mod._G = mod
for _, name in ipairs({'bootstrap.lua', 'recovered/WheelsUtil.lua', 'verify.lua'}) do
    local chunk = assert(loadfile(arg[2] .. '/' .. name))
    setfenv(chunk, mod)()
end
assert(WheelsUtil == original, 'Probe changed the original global wheel namespace')
assert(mod.FS15NativeProbe.comparisons == 104)
print('AUTHORED_MOD_FIXTURE_OK (not a GIANTS engine boot)')
