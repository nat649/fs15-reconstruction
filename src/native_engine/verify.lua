-- No loadfile/string.dump: original FS15 deliberately disables those globals.
local original, recovered = FS15NativeProbe.originalWheels, WheelsUtil
assert(not rawequal(original, recovered), 'Recovered WheelsUtil did not load into the mod environment')
local comparisons = 0
local function same(a, b)
    assert(type(a) == 'number' and type(b) == 'number' and math.abs(a-b) <= 1e-6,
        'Recovered/native wheel helper mismatch')
    comparisons = comparisons + 1
end
for _, name in ipairs({'mud', 'offRoad', 'street', 'crawler'}) do
    local a, b = original.getTireType(name), recovered.getTireType(name)
    assert(a and b, 'Original tire type is missing')
    for ground = 1, 4 do
        same(original.getTireFriction(a, ground), recovered.getTireFriction(b, ground))
        for _, wet in ipairs({0, 0.25, 0.5, 1}) do
            same(original.getTireFriction(a, ground, wet), recovered.getTireFriction(b, ground, wet))
        end
    end
end
for _, field in ipairs({false, true}) do
    for _, road in ipairs({false, true}) do
        for _, density in ipairs({0, 0.099, 0.1, 0.8, 0.801, 1}) do
            same(original.getGroundType(field, road, {0, 0, 0, density}),
                recovered.getGroundType(field, road, {0, 0, 0, density}))
        end
    end
end
FS15NativeProbe.comparisons = comparisons
print('FS15_NATIVE_RECOVERED_LUA_OK comparisons=' .. comparisons)
