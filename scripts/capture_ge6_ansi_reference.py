"""Capture bounded OS text tables used by GE6's original MSVC CRT startup.

These are numeric Windows API observations, not original game code/resources.
The checked-in table is used by Linux's CP1252 transport. Other codepages,
characters and locales remain explicit unsupported boundaries.
"""
import argparse
import ctypes as C
import os
from pathlib import Path
import sys


def capture(path):
    if os.name != 'nt':
        raise RuntimeError('Capture requires the Windows reference OS')
    kernel = C.windll.kernel32
    if kernel.GetACP() != 1252:
        raise RuntimeError('This bounded capture requires ANSI codepage 1252')
    source = C.create_string_buffer(bytes(range(256)))
    wide = C.create_unicode_buffer(256)
    for flags in (0, 1, 9):
        if kernel.MultiByteToWideChar(1252, flags, source, 256, wide, 256) != 256:
            raise RuntimeError('Unexpected CP1252 decode behavior')
    types = (C.c_uint16 * 256)()
    if not kernel.GetStringTypeW(1, wide, 256, types):
        raise RuntimeError('GetStringTypeW failed')
    tables = {'unicode': [ord(x) for x in wide], 'ctype1': list(types)}
    for name, flags in (('lower', 0x100), ('upper', 0x200)):
        mapped = C.create_unicode_buffer(512)
        if kernel.LCMapStringW(0, flags, wide, 256, mapped, 512) != 256:
            raise RuntimeError('Unexpected CP1252 case expansion')
        tables[name] = [ord(mapped[i]) for i in range(256)]
    version = sys.getwindowsversion()
    lines = ['// Authored bounded OS observations; no game instructions or resources.',
             f'// Windows {version.major}.{version.minor} build {version.build}; ACP1252; LCMapStringW locale=0.',
             '// Reproduce with scripts/capture_ge6_ansi_reference.py.', '#pragma once',
             '#include <array>', '#include <cstdint>', 'namespace ge6_ansi {',
             f'constexpr uint32_t reference_lcid={kernel.GetUserDefaultLCID()};']
    for name, values in tables.items():
        lines.append(f'constexpr std::array<uint16_t,256> {name}{{{{')
        for start in range(0, 256, 16):
            lines.append('    ' + ','.join(f'0x{x:04x}' for x in values[start:start + 16]) + ',')
        lines.append('}};')
    lines.append('}')
    path.write_text('\n'.join(lines) + '\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    capture(parser.parse_args().output)
