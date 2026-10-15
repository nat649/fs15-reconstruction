"""Compare seven newly recovered native bit-operation adapters with portable C++."""
import argparse
import ctypes as C
import hashlib
from pathlib import Path
import random
import struct
from inventory import ROOT, write_json
from native_gar import NativeGarCipher, EXPECTED_IMAGE

ADDRESSES = (0xdc960,0xdc980,0xdc9a0,0xdc9c0,0xdc9e0,0xdca00,0xdca20)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dll',type=Path,required=True)
    args=parser.parse_args()
    rebuilt=C.CDLL(str(args.dll.resolve())).fs15_script_bit_operation
    rebuilt.argtypes=[C.c_uint,C.c_void_p,C.c_size_t]
    rebuilt.restype=C.c_int
    rng=random.Random(0xf515dc960)
    inputs=[(a,b) for a in (0,1,2,3,0x7fffffff,0x80000000,0xffffffff)
            for b in (0,1,7,31,32,33,63,64,255,256,0xffffffff)]
    inputs += [(1<<bit,bit) for bit in range(32)]
    inputs += [(rng.getrandbits(32),rng.getrandbits(32)) for _ in range(3000)]
    digest=hashlib.sha256()
    cases=0
    with NativeGarCipher() as image:
        for operation,address in enumerate(ADDRESSES):
            native=C.WINFUNCTYPE(None,C.c_void_p,C.c_void_p)(image.base+address)
            for a,b in inputs:
                payload=bytearray(rng.randbytes(0x100))
                struct.pack_into('<I',payload,0,a);struct.pack_into('<I',payload,16,b)
                original=C.create_string_buffer(bytes(payload),len(payload))
                candidate=C.create_string_buffer(bytes(payload),len(payload))
                native(None,original)
                if not rebuilt(operation,candidate,len(payload)) or original.raw!=candidate.raw:
                    raise AssertionError(f'Native bit operation differs: {operation}, inputs {a:x}/{b:x}')
                digest.update(original.raw);cases+=1
    write_json(ROOT/'analysis/bit-operations-validation.json',dict(status='passed',reference_sha256=EXPECTED_IMAGE,
               rebuilt_dll_sha256=hashlib.sha256(args.dll.read_bytes()).hexdigest(),
               rebuilt_source_sha256={path:hashlib.sha256((ROOT/path).read_bytes()).hexdigest()
                                      for path in ('src/core/bit_operations.cpp','src/core/api.h')},
               native_entries=[hex(0x140000000+a) for a in ADDRESSES],
               complete_script_frame_comparisons=cases,original_frame_sha256=digest.hexdigest(),
               scope='Seven portable reconstructed 32-bit scripting operations; Windows x64 native comparison only, no GE6 startup or Switch execution'))
    print(f'Bit operations: {cases} complete native script frames match')


if __name__=='__main__':
    main()
