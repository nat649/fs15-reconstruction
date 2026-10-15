"""Compare the reconstructed 16-bit decoder with original GE6 0x14028f770.

Only the inspected leaf runs in this disposable process. It has no calls or
external globals. The original game entry point and physics are not started.
"""
import argparse
from array import array
import ctypes as C
import hashlib
import json
from pathlib import Path
import random
import struct
from native_gar import NativeGarCipher, EXPECTED_IMAGE
from inventory import ROOT

BYTE, WORD = C.c_uint8, C.c_uint16
PAGE = 65536


class Stream(C.Structure):
    _fields_ = [('data', C.POINTER(WORD)), ('words', C.c_uint32), ('cursor', C.c_uint32)]


class BitVector(C.Structure):
    _fields_ = [('segments', C.POINTER(C.POINTER(BYTE)))]


def encode(values):
    out = array('H')
    cursor = 0
    while cursor < len(values):
        end = cursor+1
        while end < len(values) and values[end] == values[cursor]:
            end += 1
        count = end-cursor
        out.append(values[cursor])
        if count > 1:
            out.append(values[cursor])
            extra = count-2
            while extra >= 65535:
                out.append(65535)
                extra -= 65535
            out.append(extra)
        cursor = end
    return out.tobytes()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dll', required=True, type=Path)
    parser.add_argument('--grle', action='append', type=Path, default=[])
    args = parser.parse_args()
    rebuilt = C.CDLL(str(args.dll.resolve())).fs15_decode_rle16
    rebuilt.argtypes = [C.POINTER(WORD), C.c_size_t, C.c_uint, C.c_uint, C.POINTER(BYTE), C.c_size_t]
    rebuilt.restype = C.c_size_t
    rng = random.Random(0xf51516)
    comparisons, samples, real = 0, 0, []
    with NativeGarCipher() as image:
        native = C.WINFUNCTYPE(None, C.POINTER(Stream), C.c_int, BYTE, C.POINTER(BitVector))(image.base+0x28f770)

        def compare(encoded, count, bits, stride, label, expected=None):
            nonlocal comparisons, samples
            source = (WORD*max(1, len(encoded)//2)).from_buffer_copy(encoded or b'\0\0')
            stream = Stream(source, len(encoded)//2, 0)
            size = max(4, (count*stride+7)//8+4)
            segments = [(BYTE*(PAGE+4))() for _ in range((size+PAGE-1)//PAGE)]
            pointers = (C.POINTER(BYTE)*len(segments))(*[C.cast(s, C.POINTER(BYTE)) for s in segments])
            bitvector = BitVector(pointers)
            candidate = (BYTE*size)()
            native(C.byref(stream), stride, bits, C.byref(bitvector))
            got = rebuilt(source, len(encoded)//2, stride, bits, candidate, size)
            original = b''.join(bytes(s)[:PAGE] for s in segments)[:size]
            if got != count or stream.cursor != len(encoded)//2 or bytes(candidate) != original:
                raise AssertionError(f'{label}: native/reconstructed output mismatch, {got}/{count}')
            if expected is not None:
                independent = bytearray(size)
                for i, value in enumerate(expected):
                    for bit in range(bits):
                        if value & (1<<bit):
                            at = i*stride+bit
                            independent[at//8] |= 1<<(at&7)
                if original != independent:
                    raise AssertionError(f'{label}: independent samples disagree')
            comparisons += 1
            samples += count
            return hashlib.sha256(original[:(count*stride+7)//8]).hexdigest()

        for bits in (1, 4, 8, 9, 12, 15, 16):
            mask = (1<<bits)-1
            for stride in sorted({bits, bits+3, 32}):
                for values in ([], [0,mask,1,mask], [mask]*2+[0], [mask]*65538+[0]):
                    compare(encode(values),len(values),bits,stride,'boundary',values)
                values = []
                for _ in range(20):
                    values += [rng.randint(0,mask)]*rng.randint(1,1000)
                compare(encode(values),len(values),bits,stride,'random runs',values)
        for path in args.grle:
            data = path.read_bytes()
            width,height,word_bytes,size = struct.unpack_from('<4I', data,5)
            if data[:5] != b'GRLE\1' or word_bytes != 2 or size != len(data)-21 or size%2:
                raise AssertionError('Expected a strict GRLE16 file')
            digest=compare(data[21:],width*height,16,16,path.name)
            real.append(dict(source_sha256=hashlib.sha256(data).hexdigest(),width=width,height=height,decoded_sha256=digest))
        source=(WORD*3)(1,1,100)
        tiny=(BYTE*2)()
        if rebuilt(source,3,16,16,tiny,2) != C.c_size_t(-1).value:
            raise AssertionError('RLE16 output overflow was not rejected')
    result=dict(status='passed',original_function='0x14028f770',reference_image_sha256=EXPECTED_IMAGE,
                rebuilt_source_sha256={path:hashlib.sha256((ROOT/path).read_bytes()).hexdigest()
                                       for path in ('src/core/rle16.cpp','src/core/rle16.h','src/core/api.h','src/core/export.h')},
                differential_cases=comparisons,decoded_samples=samples,real_density_maps=real,
                scope='Original 16-bit RLE leaf, bit widths 1..16, strides through 32, extended runs and 64 KiB boundaries')
    (ROOT/'analysis/rle16-validation.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))


if __name__=='__main__':
    main()
