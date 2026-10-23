"""Inspect the desktop ELF ABI; no original game inputs are read."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

ALLOWED = {'libX11.so.6', 'libXrandr.so.2', 'libGL.so.1', 'libOpenGL.so.0',
           'libGLX.so.0', 'libm.so.6', 'libc.so.6', 'libpthread.so.0',
           'libdl.so.2', 'librt.so.1', 'ld-linux-x86-64.so.2'}


def inspect(path):
    raw = path.read_bytes()
    if raw[:5] != b'\x7fELF\x02' or raw[5] != 1 or int.from_bytes(raw[18:20], 'little') != 62:
        raise ValueError('Expected an x86_64 little-endian Linux ELF')
    dynamic = subprocess.check_output(['readelf', '-d', str(path)], text=True)
    versions = subprocess.check_output(['readelf', '--version-info', str(path)], text=True)
    needed = sorted(set(re.findall(r'\(NEEDED\).*?\[(.+?)\]', dynamic)))
    if set(needed) - ALLOWED:
        raise ValueError('Unexpected non-driver shared dependencies: ' + str(set(needed) - ALLOWED))
    glibc = sorted({tuple(map(int, value.split('.'))) for value in re.findall(r'\bGLIBC_([0-9.]+)', versions)})
    if not glibc or glibc[-1] > (2, 31):
        raise ValueError('The portable desktop build must require at most glibc 2.31')
    return dict(file=path.name, sha256=hashlib.sha256(raw).hexdigest(), direct_shared_libraries=needed,
                maximum_glibc='.'.join(map(str, glibc[-1])), architecture='x86_64',
                host_glibc_and_graphics_used=True, game_inputs_included=False)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('files', type=Path, nargs='+')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    result = dict(status='passed', binaries=[inspect(path) for path in args.files])
    text = json.dumps(result, indent=2) + '\n'
    if args.output:
        args.output.write_text(text, encoding='utf-8')
    print(text, end='')
