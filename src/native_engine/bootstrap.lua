-- Authored probe, executed by the original GE6 mod source loader.
-- Capture the engine's original namespace before the recovered file shadows it
-- in this mod's environment. Do not replace native gameplay classes here.
FS15NativeProbe = {originalWheels = WheelsUtil}
assert(type(FS15NativeProbe.originalWheels) == 'table', 'Original engine WheelsUtil is missing')
assert(rawget(_G, 'WheelsUtil') == nil, 'Expected the isolated FS15 mod environment')
print('FS15_NATIVE_PROBE_BEGIN')
