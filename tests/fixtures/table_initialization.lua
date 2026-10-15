-- Authored regression cases: no recovered game source.
local function global_order(observe)
    RecoveryProbe = {}
    RecoveryProbe.value = observe()
    return RecoveryProbe
end

local function local_fields(value)
    local record = {}
    record.value = value
    record.enabled = true
    return record
end

local function nested_fields(value)
    local record = {}
    record.child = {}
    record.child.value = value
    return record
end

local function templates(value)
    return {constant = 17, value = value}, {value, 23}
end

return global_order, local_fields, nested_fields, templates
