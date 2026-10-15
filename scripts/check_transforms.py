"""Compare the recovered point-transform leaf with untouched original code."""
import argparse
import ctypes as C
import hashlib
from pathlib import Path
import random
import struct
from inventory import ROOT, write_json
from native_gar import NativeGarCipher, EXPECTED_IMAGE


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dll',type=Path,required=True)
    args=parser.parse_args()
    rebuilt=C.CDLL(str(args.dll.resolve())).fs15_transform_point
    rebuilt.argtypes=[C.c_void_p,C.c_size_t,C.c_void_p,C.c_void_p,C.c_size_t]
    rebuilt.restype=C.c_int
    rng=random.Random(0xf515bfec0)
    digest=hashlib.sha256()
    cases=0
    with NativeGarCipher() as image:
        native=C.WINFUNCTYPE(None,C.c_void_p,C.c_void_p)(image.base+0xbfec0)
        for index in range(12000):
            obj=C.create_string_buffer(rng.randbytes(0xa0),0xa0)
            values=[rng.uniform(-1000,1000) for _ in range(16)]
            point=[rng.uniform(-2048,2048) for _ in range(3)]
            if index<8:
                values=[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,0]
                point=[(-0.0 if index&1 else 0.0),float(index),-float(index)]
            struct.pack_into('<16f',obj,0x60,*values)
            payload=struct.pack('<4f',*point,99)
            # Native uses MOVDQA for its final store; ctypes small buffers are
            # not guaranteed to have the required 16-byte alignment.
            storage=C.create_string_buffer(32)
            expected=C.c_void_p((C.addressof(storage)+15)&~15)
            C.memmove(expected,payload,16)
            actual=C.create_string_buffer(payload,len(payload))
            native(obj,expected)
            result=C.string_at(expected,16)
            if not rebuilt(obj,0xa0,actual,actual,4) or result!=actual.raw:
                raise AssertionError(f'Native transform differs at case {index}')
            digest.update(result);cases+=1
    pins={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest()
          for p in ('src/core/transform.cpp','src/core/transform.h')}
    write_json(ROOT/'analysis/transform-validation.json',dict(status='passed',
        reference_sha256=EXPECTED_IMAGE,native_entries=['0x1400bfec0'],
        rebuilt_source_sha256=pins,rebuilt_dll_sha256=hashlib.sha256(args.dll.read_bytes()).hexdigest(),
        complete_vector_comparisons=cases,original_result_sha256=digest.hexdigest(),
        scope='Native affine point-transform leaf on finite synthetic data, bit-exact four-float outputs; no complete scene graph, physics solver or engine startup'))
    print(f'Transforms: {cases} complete original-code vector comparisons passed')


if __name__=='__main__':
    main()
