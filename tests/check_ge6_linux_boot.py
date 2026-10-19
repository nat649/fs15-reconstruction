"""Exercise the native-code loader with authored PE instructions, never game code."""
import argparse
from pathlib import Path
import struct
import subprocess


def fixture(blocked=False):
    data = bytearray(0x1000)
    def put(offset, fmt, *values):
        struct.pack_into('<' + fmt, data, offset, *values)
    data[:2] = b'MZ'
    put(0x3c, 'I', 0x80)
    data[0x80:0x84] = b'PE\0\0'
    put(0x84, 'HHIIIHH', 0x8664, 3, 0, 0, 0, 240, 0x22)
    opt = 0x98
    put(opt, 'H', 0x20b)
    put(opt + 16, 'I', 0x1000)
    put(opt + 24, 'Q', 0x140000000)
    put(opt + 32, 'II', 0x1000, 0x200)
    put(opt + 56, 'II', 0x4000, 0x400)
    put(opt + 108, 'I', 16)
    put(opt + 120, 'II', 0x2000, 40)
    for index, (name, rva, raw, size, flags) in enumerate((
            (b'.text', 0x1000, 0x400, 0x200, 0x60000020),
            (b'.rdata', 0x2000, 0x600, 0x600, 0x40000040),
            (b'.data', 0x3000, 0xc00, 0x400, 0xc0000040))):
        at = opt + 240 + index * 40
        data[at:at + 8] = name.ljust(8, b'\0')
        put(at + 8, 'IIII', 0x1000 if name == b'.data' else size, rva, size, raw)
        put(at + 36, 'I', flags)
    put(0x600, '5I', 0x2080, 0, 0, 0x2040, 0x2100)
    data[0x640:0x64d] = b'KERNEL32.dll\0'
    names = [b'SetHandleCount', b'QueryPerformanceFrequency', b'MultiByteToWideChar', b'WideCharToMultiByte', b'SetUnhandledExceptionFilter']
    if blocked:
        names.append(b'UnsupportedFixtureAPI')
    for index, name in enumerate(names):
        put(0x680 + index * 8, 'Q', 0x2200 + index * 64)
        put(0x700 + index * 8, 'Q', 0x2200 + index * 64)
        at = 0x802 + index * 64
        data[at:at + len(name) + 1] = name + b'\0'
    data[0xc00:0xc03] = bytes((0xe9, 0x80, 0))
    code = bytearray(b'\x48\x83\xec\x48')  # Shadow space + four stack arguments + alignment.
    # Original MSVC stack probes use GS, independently of Linux libc's FS TLS.
    code += b'\x65\x48\x8b\x04\x25\x30\x00\x00\x00\x48\x3b\x40\x30\x74\x02\x0f\x0b'
    code += b'\x65\x48\x8b\x0c\x25\x10\x00\x00\x00\x48\x39\xcc\x77\x02\x0f\x0b'
    def call(iat):
        code.extend(b'\xff\x15' + struct.pack('<i', iat - (0x1000 + len(code) + 6)))
    code += b'\xb9' + struct.pack('<I', 2048)
    call(0x2100)
    code += b'\x3d' + struct.pack('<I', 2048) + b'\x74\x02\x0f\x0b'
    code += b'\x48\xb8' + struct.pack('<Q', 0x140003800) + b'\x48\x83\x38\x00\x74\x02\x0f\x0b'
    code += b'\x48\xb9' + struct.pack('<Q', 0x140003800)
    call(0x2108)
    code += b'\x48\xb8' + struct.pack('<Q', 0x140003800) + b'\x48\x8b\x00'
    code += b'\x3d' + struct.pack('<I', 1000000000) + b'\x74\x02\x0f\x0b'
    # Round-trip accented/euro characters through six- and eight-argument imports.
    code += b'\xb9' + struct.pack('<I', 1252) + b'\xba\x01\x00\x00\x00'
    code += b'\x49\xb8' + struct.pack('<Q', 0x140003000) + b'\x41\xb9\x03\x00\x00\x00'
    code += b'\x48\xb8' + struct.pack('<Q', 0x140003820) + b'\x48\x89\x44\x24\x20\xc7\x44\x24\x28\x03\x00\x00\x00'
    call(0x2110)
    code += b'\x83\xf8\x03\x74\x02\x0f\x0b'
    code += b'\x48\xb8' + struct.pack('<Q', 0x140003820) + b'\x81\x38' + struct.pack('<I', 0x20ac00e9) + b'\x74\x02\x0f\x0b'
    code += b'\xb9' + struct.pack('<I', 1252) + b'\x31\xd2'
    code += b'\x49\xb8' + struct.pack('<Q', 0x140003820) + b'\x41\xb9\x03\x00\x00\x00'
    code += b'\x48\xb8' + struct.pack('<Q', 0x140003840) + b'\x48\x89\x44\x24\x20\xc7\x44\x24\x28\x03\x00\x00\x00'
    code += b'\x48\xc7\x44\x24\x30\x00\x00\x00\x00\x48\xc7\x44\x24\x38\x00\x00\x00\x00'
    call(0x2118)
    code += b'\x83\xf8\x03\x74\x02\x0f\x0b'
    code += b'\x48\xb8' + struct.pack('<Q', 0x140003840) + b'\x81\x38' + struct.pack('<I', 0x80e9) + b'\x74\x02\x0f\x0b'
    code += b'\xb9\x34\x12\x00\x00'
    call(0x2120)
    code += b'\x48\x85\xc0\x74\x02\x0f\x0b\x31\xc9'
    call(0x2120)
    code += b'\x48\x3d\x34\x12\x00\x00\x74\x02\x0f\x0b'
    if blocked:
        call(0x2128)
    code += b'\x48\x83\xc4\x48\xc3'
    assert len(code) <= 0x200
    data[0x400:0x400 + len(code)] = code
    return data


def check(exe, workdir):
    workdir.mkdir(parents=True, exist_ok=True)
    path = workdir / 'authored.exe'
    def run(data, mode='--fixture'):
        path.write_bytes(data)
        return subprocess.run([str(exe.resolve()), mode, str(path)], capture_output=True, text=True, timeout=15)
    result = run(fixture())
    assert result.returncode == 0 and 'GE6_AUTHORED_FIXTURE_RETURN os_calls=6' in result.stdout, result
    assert 'supported_imports=5 blocked_imports=0' in result.stdout, result
    assert 'GE6_ORIGINAL_MAIN_REACHED' not in result.stdout, result
    result = run(fixture(True))
    assert result.returncode == 78 and 'KERNEL32.dll!UnsupportedFixtureAPI' in result.stderr and 'caller=0x140001' in result.stderr, result
    result = run(fixture(), '--image')
    assert result.returncode == 1 and 'hash differs' in result.stderr, result
    broken = fixture()
    struct.pack_into('<I', broken, 0x98 + 240 + 12, 0x2000)
    result = run(broken)
    assert result.returncode == 1 and 'Overlapping PE sections' in result.stderr, result
    print('Native GE6 boot transport: authored PE mapping, Microsoft ABI import call/writeback, named missing-import stop, pinned-image rejection and overlap rejection passed. No original engine is booted by these fixtures.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--exe', required=True, type=Path)
    parser.add_argument('--workdir', required=True, type=Path)
    args = parser.parse_args()
    check(args.exe, args.workdir)
