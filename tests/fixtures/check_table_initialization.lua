local global_order, local_fields, nested_fields, templates = assert(loadfile(arg[1]))()
RecoveryProbe = nil
local calls = 0
local result = global_order(function()
    calls = calls + 1
    assert(type(RecoveryProbe) == "table", "Global table must exist before evaluating its field")
    assert(RecoveryProbe.value == nil)
    return 37
end)
assert(result == RecoveryProbe and result.value == 37 and calls == 1)
local record = local_fields(41)
assert(record.value == 41 and record.enabled == true)
assert(nested_fields(43).child.value == 43)
local keyed, array = templates(47)
assert(keyed.constant == 17 and keyed.value == 47 and array[1] == 47 and array[2] == 23)
print("Table initialization: evaluation order and field values pass")
