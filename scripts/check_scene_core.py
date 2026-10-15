"""Differentially check recovered GE6 affine math and cached scene transforms.

Only finite synthetic matrices, bounded node arenas and synthetic script frames
are supplied to the pinned native reference. No entry point, scene lifecycle,
renderer, Steam session or game process is started or modified.
"""
import argparse
from collections import Counter
import ctypes as C
import hashlib
import math
from pathlib import Path
import random
import struct
from inventory import ROOT, write_json
from native_gar import NativeGarCipher, EXPECTED_IMAGE

ENTRIES = {
    'multiply': 0x9da60, 'inverse': 0x9e160, 'rigid_inverse_point': 0x9e0a0,
    'quaternion_to_matrix': 0x9df40, 'matrix_to_quaternion': 0x280380,
    'scale': 0x9dcb0, 'cached_world': 0xb1a50,
    'local_to_world': 0xe5330, 'local_direction_to_world': 0xe5410,
    'world_to_local': 0xe54e0, 'world_direction_to_local': 0xe55f0,
}
STRIDE = 0x118


def buffer(payload):
    return C.create_string_buffer(payload, len(payload))


def floats(values):
    return buffer(struct.pack('<' + 'f' * len(values), *values))


def matrix(rng, index):
    m = [rng.uniform(-4, 4) for _ in range(16)]
    m[3] = m[7] = m[11] = 0
    m[15] = 1
    m[12:15] = [rng.uniform(-2048, 2048) for _ in range(3)]
    if index % 8 == 0:
        m = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -10, 20, -30, 1]
    if index % 8 == 1:
        m = [-2, 0, 0, 0, 0, 3, 0, 0, 0, 0, -4, 0, 10, -20, 30, 1]
    return m


def arena(rng, index):
    count = 1 + index % 17
    payload = bytearray(rng.randbytes(count * STRIDE))
    # Native parent pointers require two separately relocated arena copies.
    parents = []
    for i in range(count):
        parents.append(i - 1 if i else None)
        flags = rng.getrandbits(32) | 0x40
        if index % 3 == 0 and i < count // 2:
            flags &= ~0x40
        struct.pack_into('<I', payload, i * STRIDE + 0x50, flags)
        struct.pack_into('<16f', payload, i * STRIDE + 0x98, *matrix(rng, index+i))
        struct.pack_into('<16f', payload, i * STRIDE + 0xd8, *matrix(rng, index+i+1))
    copies = [buffer(bytes(payload)), buffer(bytes(payload))]
    for copy in copies:
        for i, parent in enumerate(parents):
            struct.pack_into('<Q', copy, i * STRIDE + 0x28,
                             C.addressof(copy) + parent * STRIDE if parent is not None else 0)
    return copies, count


def canonical_nodes(data, count):
    payload = bytearray(data.raw)
    for i in range(count):
        struct.pack_into('<Q', payload, i*STRIDE+0x28, 0)
    return bytes(payload)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dll', type=Path, required=True)
    args = parser.parse_args()
    core = C.CDLL(str(args.dll.resolve()))
    p, n = C.c_void_p, C.c_size_t
    signatures = {
        'fs15_affine_multiply': [p,n,p,n,p,n],
        'fs15_affine_inverse': [p,n,p,n],
        'fs15_rigid_inverse_point': [p,n,p,p,n],
        'fs15_quaternion_to_matrix': [p,n,p,n],
        'fs15_matrix_to_quaternion': [p,n,p,n],
        'fs15_matrix_scale': [p,n,p,n],
        'fs15_scene_update_world': [p,n,n,n],
        'fs15_scene_transform_frame': [C.c_uint,p,n,n,n,p,n],
    }
    for name, signature in signatures.items():
        getattr(core, name).argtypes = signature
        getattr(core, name).restype = C.c_int
    rng = random.Random(0xf5155ce6)
    counts = Counter()
    digest = hashlib.sha256()

    def check(kind, expected, actual):
        if expected != actual:
            # Index/byte offsets only; native vectors remain private.
            differences = [i for i, (a,b) in enumerate(zip(expected,actual)) if a != b]
            raise AssertionError(f'{kind} case {counts[kind]} differs at bytes {differences[:12]}')
        counts[kind] += 1
        digest.update(kind.encode()+expected)

    with NativeGarCipher() as image:
        def native(name, signature, result=None):
            return C.WINFUNCTYPE(result, *signature)(image.base + ENTRIES[name])
        mul = native('multiply',[p,p,p])
        inv = native('inverse',[p,p],C.c_ubyte)
        rigid = native('rigid_inverse_point',[p,p,p])
        qmat = native('quaternion_to_matrix',[p,p])
        matq = native('matrix_to_quaternion',[p,p])
        scale = native('scale',[p,p])
        world = native('cached_world',[p],p)
        adapters = [native(name,[p,p]) for name in list(ENTRIES)[7:]]
        for index in range(6000):
            a, b = floats(matrix(rng,index)), floats(matrix(rng,index+3))
            expected, actual = buffer(rng.randbytes(64)), buffer(bytes(64))
            mul(expected,a,b)
            if not core.fs15_affine_multiply(a,16,b,16,actual,16):
                raise AssertionError('Finite multiplication rejected')
            check('multiply',expected.raw,actual.raw)

            # Defined native singular failures preserve every destination byte.
            if index < 20:
                m = [1,0,0,0,0,1,0,0,0,0,0,0,2,3,4,1]
                m[10] = (0.0,1e-37,1e-36,1.000001e-36,-1e-36)[index % 5]
                a = floats(m)
            original = rng.randbytes(64)
            expected, actual = buffer(original), buffer(original)
            ok = inv(expected,a)
            rebuilt_ok = core.fs15_affine_inverse(a,16,actual,16)
            if ok != rebuilt_ok:
                raise AssertionError(f'Inverse status differs at {index}')
            counts['inverse_success' if ok else 'inverse_singular'] += 1
            check('inverse',expected.raw,actual.raw)

            point = floats([rng.uniform(-2048,2048) for _ in range(3)])
            expected, actual = buffer(bytes(12)), buffer(bytes(12))
            rigid(a,point,expected)
            if not core.fs15_rigid_inverse_point(a,16,point,actual,3):
                raise AssertionError('Rigid transform rejected')
            check('rigid_inverse_point',expected.raw,actual.raw)

            q = [rng.uniform(-1,1) for _ in range(4)]
            magnitude = math.sqrt(sum(v*v for v in q))
            q = [v/magnitude for v in q]
            if index < 8:
                q = ([1,0,0,0],[0,1,0,0],[0,0,1,0],[0,0,0,1],
                     [0,0.6,0.8,0],[0,0.8,0,0.6],[-1,0,-0.0,0],[0,0,0,-1])[index]
            q = floats(q)
            original = rng.randbytes(64)
            expected, actual = buffer(original), buffer(original)
            qmat(expected,q)
            if not core.fs15_quaternion_to_matrix(q,4,actual,16):
                raise AssertionError('Quaternion conversion rejected')
            check('quaternion_to_matrix',expected.raw,actual.raw)

            # Convert a valid rotation, explicitly sanitize ignored padding.
            rotation = bytearray(expected.raw)
            for lane in (3,7,11,12,13,14,15):
                struct.pack_into('<f',rotation,lane*4,0)
            rotation = buffer(bytes(rotation))
            expected, actual = buffer(bytes(16)), buffer(bytes(16))
            matq(expected,rotation)
            if not core.fs15_matrix_to_quaternion(rotation,16,actual,4):
                raise AssertionError('Matrix quaternion conversion rejected')
            check('matrix_to_quaternion',expected.raw,actual.raw)

            # Exercise near-unit open interval endpoints and reflected scales.
            if index < 12:
                m = [1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1]
                m[index%3*5] = (0,0.9999899864196777,1,1.0000100135803223,
                                -0.9999899864196777,-1.0000100135803223)[index//2]
                a = floats(m)
            expected, actual = buffer(bytes(12)), buffer(bytes(12))
            scale(a,expected)
            if not core.fs15_matrix_scale(a,16,actual,3):
                raise AssertionError('Matrix scale rejected')
            check('scale',expected.raw,actual.raw)

        for index in range(1200):
            for operation in range(5):
                (original, rebuilt), count = arena(rng,index)
                node = C.addressof(original)+(count-1)*STRIDE
                if operation == 0:
                    address = world(node)
                    if address != node+0xd8 or not core.fs15_scene_update_world(
                            rebuilt,len(rebuilt),STRIDE,count-1):
                        raise AssertionError('Cached world result differs')
                    kind = 'cached_world'
                else:
                    if operation >= 3:
                        # Long arbitrary shear chains can round to singular.
                        # The original adapters ignore the failed inverse and
                        # read uninitialized stack memory; never exercise that.
                        world_matrix = world(node)
                        inverse_check = buffer(bytes(64))
                        if not inv(inverse_check,world_matrix):
                            counts['scene_singular_inverse_excluded'] += 1
                            continue
                    payload = bytearray(rng.randbytes(0x120))
                    for lane in range(3):
                        struct.pack_into('<f',payload,lane*16,rng.uniform(-2048,2048))
                    expected, actual = buffer(bytes(payload)), buffer(bytes(payload))
                    adapters[operation-1](node,expected)
                    if not core.fs15_scene_transform_frame(operation-1,rebuilt,
                            len(rebuilt),STRIDE,count-1,actual,len(actual)):
                        raise AssertionError(f'Scene adapter rejected a defined case {index}/{operation}')
                    kind = list(ENTRIES)[operation+6]
                    check(kind+'_frame',expected.raw,actual.raw)
                check(kind+'_arena',canonical_nodes(original,count),canonical_nodes(rebuilt,count))

    pins = {str(path): hashlib.sha256((ROOT/path).read_bytes()).hexdigest() for path in
            ('src/core/scene_math.cpp','src/core/scene_math.h',
             'src/core/scene_graph.cpp','src/core/scene_graph.h')}
    report = dict(status='passed',reference_sha256=EXPECTED_IMAGE,
        native_entries=['0x'+format(0x140000000+entry,'x') for entry in ENTRIES.values()],
        rebuilt_source_sha256=pins,rebuilt_dll_sha256=hashlib.sha256(args.dll.read_bytes()).hexdigest(),
        comparisons=dict(counts),original_results_sha256=digest.hexdigest(),
        scope='Finite synthetic affine/quaternion math, dirty/cached parent chains and complete script frames. '
              'Singular native inverse outputs are excluded from scene adapters. No object lifecycle, '
              'scene loading, rendering, physics, engine startup or console execution.')
    write_json(ROOT/'analysis/scene-core-validation.json',report)
    print(__import__('json').dumps(report,indent=2))


if __name__ == '__main__':
    main()
