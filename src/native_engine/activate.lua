-- Authored integration boundary. The recovered helpers also run in the home host.
-- Keep GE6 rendering, contacts, suspension, scene loading and mission lifecycle.
assert(FS15NativeProbe.comparisons == 104, 'Recovered helper parity must pass before activation')
local original, recovered = FS15NativeProbe.originalWheels, WheelsUtil
local names = {'getTireType', 'getTireFriction', 'getGroundType'}
local saved, installed = {}, {}
local function activate()
    if FS15NativeProbe.gameplayActivated then return end
    -- Preserve tire registrations from the original game and any earlier mods.
    recovered.tireTypes = original.tireTypes
    for _, name in ipairs(names) do
        saved[name] = original[name]
        local implementation, label = assert(recovered[name]), name
        local used = false
        installed[name] = function(...)
            if not used then
                print('FS15_HYBRID_HELPER_USED name=' .. label)
                used = true
            end
            return implementation(...)
        end
        original[name] = installed[name]
    end
    FS15NativeProbe.gameplayActivated = true
    print('FS15_HYBRID_RECOVERED_ACTIVE helpers=3')
end
activate()
local listener = {}
-- FS15 loads mod scripts once; reactivate if another map follows deleteMap.
function listener:loadMap() activate() end
function listener:update() end
function listener:draw() end
function listener:keyEvent() end
function listener:mouseEvent() end
function listener:deleteMap()
    for _, name in ipairs(names) do
        -- Do not undo another mod's later wrapper.
        if original[name] == installed[name] then original[name] = saved[name] end
    end
    FS15NativeProbe.gameplayActivated = false
    print('FS15_HYBRID_RECOVERED_RESTORED')
end
addModEventListener(listener)
