"""Compare recovered wheel lookup/read adapters with the pinned original code.

Only synthetic caller-owned registry/record/frame memory is used. No GE6
startup, scene, PhysX solver, Steam initialization or console execution occurs.
"""
import argparse
import ctypes as C
import hashlib
from pathlib import Path
import random
import struct
from inventory import ROOT, write_json
from native_gar import NativeGarCipher, EXPECTED_IMAGE

ADAPTERS = (0xe77e0, 0xe7850, 0xe78d0, 0xe7990, 0xe7a30, 0xe7ac0, 0xe7b90)


def pointer(buffer):
    return C.addressof(buffer)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dll', type=Path, required=True)
    args = parser.parse_args()
    core = C.CDLL(str(args.dll.resolve()))
    core.fs15_resolve_wheel_record.argtypes = [C.c_void_p, C.c_void_p, C.c_int32, C.POINTER(C.c_void_p)]
    core.fs15_resolve_wheel_record.restype = C.c_void_p
    core.fs15_query_wheel_record.argtypes = [C.c_uint, C.c_void_p, C.c_size_t, C.c_int, C.c_void_p, C.c_size_t]
    core.fs15_query_wheel_record.restype = C.c_int
    rng = random.Random(0xf515c9e50)
    lookup_cases = frame_cases = 0
    digest = hashlib.sha256()
    with NativeGarCipher() as image:
        manager = C.create_string_buffer(0x490)
        singleton = C.c_void_p.from_address(image.base + 0x162d138)
        saved_singleton = singleton.value
        singleton.value = pointer(manager)
        find = C.WINFUNCTYPE(C.c_void_p, C.c_void_p, C.c_void_p, C.c_int32, C.POINTER(C.c_void_p))(image.base + 0xc9e50)
        adapters = [C.WINFUNCTYPE(None, C.c_void_p, C.c_void_p)(image.base + address) for address in ADAPTERS]
        try:
            for iteration in range(240):
                count = (1, 3, 8, 17, 31)[iteration % 5]
                records = C.create_string_buffer(max(1, count) * 0x160)
                ids = []
                for index in range(count):
                    shape_id = rng.randrange(-8, 16)
                    ids.append(shape_id)
                    offset = index * 0x160
                    struct.pack_into('<i', records, offset + 0x13c, shape_id)
                    records[offset + 0x34] = bytes([rng.choice((0, 1, 2))])
                    struct.pack_into('<I', records, offset + 0x38, rng.getrandbits(32))
                    for field in (0x30, 0x40, 0x44, 0x48, 0x50, 0x60, 0x64, 0x68, 0x70, 0x80, 0x84, 0x88, 0x90, 0x94, 0x98):
                        struct.pack_into('<f', records, offset + field, rng.uniform(-10000, 10000))
                wheel_set = C.create_string_buffer(0x50)
                struct.pack_into('<i', wheel_set, 0x3c, count)
                struct.pack_into('<Q', wheel_set, 0x48, pointer(records))
                sentinel = C.create_string_buffer(0x30)
                sentinel[0x29] = b'\x01'
                # Three explicit tree nodes exercise both branches and unsigned keys.
                nodes = [C.create_string_buffer(0x30) for _ in range(3)]
                keys = (0, 0x8000000000000000, 0xffffffffffffffff)
                for node, key in zip(nodes, keys):
                    struct.pack_into('<QQQQQ', node, 0, pointer(sentinel), pointer(sentinel), pointer(sentinel), key, pointer(wheel_set))
                struct.pack_into('<Q', nodes[1], 0, pointer(nodes[0]))
                struct.pack_into('<Q', nodes[1], 0x10, pointer(nodes[2]))
                struct.pack_into('<Q', sentinel, 8, pointer(nodes[1]))
                struct.pack_into('<Q', manager, 0x250, pointer(sentinel))
                actor_data = C.create_string_buffer(0x38)
                actor = C.create_string_buffer(0x120)
                struct.pack_into('<Q', actor, 0x118, pointer(actor_data))
                for key in keys:
                    struct.pack_into('<Q', actor_data, 0x30, key)
                    for flags in (8, 0x88):
                        struct.pack_into('<I', actor_data, 8, flags)
                        shape_id = ids[iteration % count]
                        original_set, rebuilt_set = C.c_void_p(0x1234), C.c_void_p(0x1234)
                        expected = find(manager, actor, shape_id, C.byref(original_set))
                        actual = core.fs15_resolve_wheel_record(manager, actor, shape_id, C.byref(rebuilt_set))
                        if expected != actual or original_set.value != rebuilt_set.value:
                            raise AssertionError('Native/rebuilt wheel lookup or found-set result differs')
                        lookup_cases += 1
                        for kind, adapter in enumerate(adapters):
                            # Missing-record position returns original uninitialized stack data.
                            if kind == 5 and not expected:
                                continue
                            payload = bytearray(rng.randbytes(0x130))
                            struct.pack_into('<i', payload, 0, shape_id)
                            reference = C.create_string_buffer(bytes(payload), len(payload))
                            candidate = C.create_string_buffer(bytes(payload), len(payload))
                            adapter(actor, reference)
                            if not core.fs15_query_wheel_record(kind, actual, 0x160 if actual else 0, 1, candidate, len(payload)):
                                raise AssertionError('Rebuilt query rejected a defined native case')
                            if reference.raw != candidate.raw:
                                raise AssertionError(f'Script frame differs: adapter {hex(ADAPTERS[kind])}, iteration {iteration}')
                            digest.update(reference.raw)
                            frame_cases += 1
                for kind, adapter in enumerate(adapters):
                    payload = rng.randbytes(0x130)
                    reference = C.create_string_buffer(payload, len(payload))
                    candidate = C.create_string_buffer(payload, len(payload))
                    adapter(None, reference)
                    if not core.fs15_query_wheel_record(kind, None, 0, 0, candidate, len(payload)) or reference.raw != candidate.raw:
                        raise AssertionError('Null scene-object behavior differs')
                    frame_cases += 1
        finally:
            singleton.value = saved_singleton
    report = dict(status='passed', reference_sha256=EXPECTED_IMAGE, lookup_routine='0x1400c9e50',
                  rebuilt_dll_sha256=hashlib.sha256(args.dll.read_bytes()).hexdigest(),
                  rebuilt_source_sha256={path:hashlib.sha256((ROOT/path).read_bytes()).hexdigest()
                                         for path in ('src/core/wheel_queries.cpp','src/core/wheel_record.h','src/core/api.h')},
                  query_adapters=[hex(0x140000000 + address) for address in ADAPTERS],
                  lookup_comparisons=lookup_cases, complete_script_frame_comparisons=frame_cases,
                  original_frame_sha256=digest.hexdigest(), wheel_record_bytes=0x160,
                  scope='Recovered wheel registry lookup and seven read adapters with synthetic data; not integrated suspension/contacts, full GE6 recompilation or a Switch build',
                  excluded_native_undefined_case='Missing-record position exposes uninitialized data; missing-lookup diagnostics require unlinked CRT imports. Native comparisons use successful lookups and null-node adapters only. Authored guards are tested separately.',
                  platform='Windows x64 original-code comparison; portable core cross-platform tests are separate')
    write_json(ROOT / 'analysis/wheel-queries-validation.json', report)
    print(f'Wheel recovery: {lookup_cases} lookup comparisons / {frame_cases} complete script-frame comparisons passed')


if __name__ == '__main__':
    main()
