"""Differential check of reconstructed GE6 tire-force leaf 0x1400c24c0.

Runs in this disposable process, with a synthetic wheel record. Only inspected
math paths are exercised; no scene, PhysX startup, game entry point or Steam
code executes. The normalized original image remains a test-only dependency.
"""
import argparse
import ctypes as C
import hashlib
import math
from pathlib import Path
import random
import struct
from inventory import ROOT, write_json
from native_gar import NativeGarCipher, EXPECTED_IMAGE

FLOAT = C.c_float
POINTER = C.POINTER(FLOAT)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dll', type=Path, required=True)
    args = parser.parse_args()
    rebuilt = C.CDLL(str(args.dll.resolve())).fs15_compute_tire_forces
    rebuilt.argtypes = [POINTER, FLOAT, FLOAT, FLOAT, POINTER]
    rebuilt.restype = C.c_int
    rng = random.Random(0xf515c24c0)
    cases = []
    # radius, lateral stiffness, reference load, longitudinal stiffness, friction.
    for params in ((0.5, 2, 1, 8, 1.1), (0.85, 20, 4, 30, 0.95), (0.35, 5, 2, 12, 1.2)):
        for load in (0, 0.019, 0.02, 0.021, 0.5, 0.999, 1, 4, 12):
            for sx, sy in ((0, 0), (1e-6, -1e-6), (1e-5, 0), (0, 1e-5), (1.01e-5, 0),
                           (0, -1.01e-5), (0.05, 0), (0, 0.1), (-0.2, 0.15), (0.3, -0.2), (1, 1)):
                cases.append((params, sx, sy, load))
    for _ in range(3000):
        params = (rng.uniform(0.2, 1.2), rng.uniform(0.5, 25), rng.uniform(0.5, 8), rng.uniform(1, 35), rng.uniform(0.3, 1.5))
        cases.append((params, rng.uniform(-1.5, 1.5), rng.uniform(-1, 1), rng.uniform(0.02, 15)))
    for combined in (6.2831755, 6.2831855, 6.2831955):
        cases.append(((0.5, 2, 1, 8, 1.1), math.sqrt((combined*1.1)**2-(2*0.2)**2)/8, 0.2, 1))
    digest = hashlib.sha256()
    max_error = [0.0] * 4
    with NativeGarCipher(math_helpers=True) as image:
        native = C.WINFUNCTYPE(None, C.c_void_p, FLOAT, FLOAT, FLOAT, POINTER, POINTER, POINTER, POINTER)(image.base + 0xc24c0)
        for i, (params, sx, sy, load) in enumerate(cases):
            record = C.create_string_buffer(0x160)
            for offset, value in zip((0xdc, 0x110, 0x114, 0x118, 0x11c), params):
                struct.pack_into('<f', record, offset, value)
            original = (FLOAT * 4)()
            native(record, sx, sy, load, *[C.cast(C.byref(original, k * 4), POINTER) for k in range(4)])
            candidate = (FLOAT * 4)()
            if not rebuilt((FLOAT * 5)(*params), sx, sy, load, candidate):
                raise AssertionError(f'Reconstructed routine rejected fixture {i}')
            for k, (expected, actual) in enumerate(zip(original, candidate)):
                error = abs(expected - actual)
                if not math.isfinite(expected) or not math.isfinite(actual) or error > 2e-5 * max(1, abs(expected)):
                    raise AssertionError(f'Case {i}, output {k}: {expected} != {actual}; params={params}, slips={sx,sy}, load={load}')
                max_error[k] = max(max_error[k], error / max(1, abs(expected)))
            digest.update(bytes(original))
    # Authored API validation, outside the original routine's observed domain.
    for params, sx, sy, load in [((0, 2, 1, 8, 1), 0.1, 0, 1), ((0.5, 2, 0, 8, 1), 0.1, 0, 1),
                                 ((0.5, 2, 1, 8, 1), float('nan'), 0, 1), ((0.5, 2, 1, 8, 1), 0.1, 0, -1)]:
        if rebuilt((FLOAT * 5)(*params), sx, sy, load, (FLOAT * 4)()):
            raise AssertionError('Invalid authored API input accepted')
    report = dict(routine='0x1400c24c0', reference_sha256=EXPECTED_IMAGE,
                  comparisons=len(cases), scalar_outputs=len(cases)*4, maximum_scaled_absolute_error=max_error,
                  tolerance='abs(error) <= 2e-5 * max(1, abs(original)) for each float output',
                  original_output_sha256=digest.hexdigest(),
                  scope='Standalone tire-force math using synthetic wheel records; no integrated native contacts, suspension, drivetrain or rigid-body physics',
                  outputs=['wheel torque', 'longitudinal force', 'lateral force', 'aligning quantity'],
                  parameter_names_and_units='Inferred from native wheel-record fields; complete API semantics remain under investigation')
    write_json(ROOT / 'analysis/tire-forces-validation.json', report)
    print(f'Tire-force reconstruction: {len(cases)} native comparisons / {len(cases)*4} float outputs match')


if __name__ == '__main__':
    main()
