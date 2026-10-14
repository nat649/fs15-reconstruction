"""Compare the mesh bridge cipher with an isolated original GE6 native routine.

Uses the already pinned local analysis image; never injects into a game process.
Original code bytes, key tables and per-case vectors remain local.
"""
import argparse
import ctypes as C
import hashlib
import json
from pathlib import Path
import struct
from native_gar import NativeGarCipher, DEFAULT_IMAGE
from inventory import PE, ROOT


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('fixtures', type=Path)
    args = parser.parse_args()
    vectors = json.loads(args.fixtures.read_text())
    image = DEFAULT_IMAGE.read_bytes()
    pe = PE(image)
    tested_bytes = 0
    with NativeGarCipher() as reference:
        # Shape stream reader 0x14029f490 calls this buffer routine. Its state
        # is copied from the 64-byte seed record by constructor 0x14029f350.
        decode = C.WINFUNCTYPE(None, C.c_void_p, C.c_void_p, C.c_uint32, C.c_void_p)(reference.base + 0x2b3fa0)
        for case in vectors:
            key_offset = pe.offset(0x571050 + case['seed'] * 64)
            state_bytes = bytearray(image[key_offset:key_offset+64])
            struct.pack_into('<Q', state_bytes, 32, case['block'])
            state = C.create_string_buffer(bytes(state_bytes))
            source = bytes.fromhex(case['input'])
            src = C.create_string_buffer(source)
            dst = C.create_string_buffer(len(source))
            decode(state, src, len(source), dst)
            assert dst.raw == bytes.fromhex(case['output']), (case['seed'], case['block'], case['length'])
            assert struct.unpack_from('<Q', state.raw, 32)[0] == case['next']
            tested_bytes += len(source)
    summary = dict(status='passed', originalFunction='0x1402b3fa0',
                   inputImageSha256=hashlib.sha256(image).hexdigest(),
                   differentialCases=len(vectors), comparedBytes=tested_bytes,
                   scope='Mesh cipher only: four seeds, partial/full blocks and 32-bit counter carry; not the full engine')
    (ROOT/'analysis/shapes-native-validation.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(json.dumps(summary,indent=2))


if __name__ == '__main__':
    main()
