"""Exercise the native-code loader with authored PE instructions, never game code."""
import argparse
from pathlib import Path
import struct
import subprocess
import tempfile


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
    subprocess.run([str(exe.resolve()), '--check-singleton'], check=True, timeout=15)
    subprocess.run([str(exe.resolve()), '--check-version'], check=True, timeout=15)
    subprocess.run([str(exe.resolve()), '--check-network'], check=True, timeout=15)
    subprocess.run([str(exe.resolve()), '--check-startup'], check=True, timeout=15)
    subprocess.run([str(exe.resolve()), '--check-system'], check=True, timeout=15)
    subprocess.run([str(exe.resolve()), '--check-events'], check=True, timeout=15)
    subprocess.run([str(exe.resolve()), '--check-virtual-memory'], check=True, timeout=15)
    subprocess.run([str(exe.resolve()), '--check-threads'], check=True, timeout=15, stdout=subprocess.PIPE)
    rejected = subprocess.run([str(exe.resolve()), '--check-unknown-exception'], capture_output=True, text=True, timeout=15)
    assert rejected.returncode == 78 and 'KERNEL32.dll!RaiseException' in rejected.stderr and 'exception=0xdeadbeef' in rejected.stderr, rejected
    rejected = subprocess.run([str(exe.resolve()), '--check-alertable-wait'], capture_output=True, text=True, timeout=15)
    assert rejected.returncode == 78 and 'KERNEL32.dll!WaitForSingleObjectEx' in rejected.stderr and 'alertable=1' in rejected.stderr, rejected
    with tempfile.TemporaryDirectory(dir=workdir) as directory:
        subprocess.run([str(exe.resolve()), '--check-files', directory], check=True, timeout=15)
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
    threaded = fixture()
    thread_names = (b'CreateThread', b'WaitForSingleObject', b'CloseHandle', b'GetCurrentThreadId', b'TlsGetValue', b'TlsSetValue', b'TlsAlloc', b'TlsFree')
    for i, name in enumerate(thread_names):
        struct.pack_into('<Q', threaded, 0x680 + i * 8, 0x2200 + i * 64)
        struct.pack_into('<Q', threaded, 0x700 + i * 8, 0x2200 + i * 64)
        at = 0x802 + i * 64
        threaded[at:at + 62] = (name + b'\0').ljust(62, b'\0')
    def thread_code(rva):
        code = bytearray(b'\x53\x48\x83\xec' + (b'\x40' if rva == 0x1000 else b'\x20'))
        def call(iat):
            code.extend(b'\xff\x15' + struct.pack('<i', iat - (rva + len(code) + 6)))
        return code, call
    code, call = thread_code(0x1000)
    call(0x2130)  # TlsAlloc; parent and child use the same index.
    code += b'\x48\xbb' + struct.pack('<Q', 0x140003000) + b'\x89\x03\x89\xc1\xba\x11\x11\x00\x00'
    call(0x2128)
    code += b'\x83\xf8\x01\x74\x02\x0f\x0b'
    call(0x2118)
    code += b'\x89\x43\x04\x31\xc9\x31\xd2\x49\xb8' + struct.pack('<Q', 0x140001100)
    code += b'\x49\x89\xd9\x48\xc7\x44\x24\x20\x00\x00\x00\x00\x48\x8d\x43\x0c\x48\x89\x44\x24\x28'
    call(0x2100)  # Six-argument Microsoft ABI CreateThread.
    code += b'\x48\x85\xc0\x75\x02\x0f\x0b\x48\x89\xc3\x48\x89\xd9\xba\x88\x13\x00\x00'
    call(0x2108)
    code += b'\x85\xc0\x74\x02\x0f\x0b\x48\x89\xd9'
    call(0x2110)
    code += b'\x83\xf8\x01\x74\x02\x0f\x0b\x48\xbb' + struct.pack('<Q', 0x140003000) + b'\x8b\x0b'
    call(0x2120)
    code += b'\x48\x3d\x11\x11\x00\x00\x74\x02\x0f\x0b\x8b\x43\x04\x3b\x43\x08\x75\x02\x0f\x0b\x8b\x0b'
    call(0x2138)
    code += b'\x48\x83\xc4\x40\x5b\xc3'
    assert len(code) <= 0x100
    threaded[0x400:0x500] = code.ljust(0x100, b'\0')
    code, call = thread_code(0x1100)
    code += b'\x48\x89\xcb\x65\x48\x8b\x04\x25\x30\x00\x00\x00\x48\x3b\x40\x30\x74\x02\x0f\x0b'
    code += b'\x65\x48\x8b\x04\x25\x10\x00\x00\x00\x48\x39\xc4\x77\x02\x0f\x0b'
    call(0x2118)
    code += b'\x89\x43\x08\x8b\x0b'
    call(0x2120)
    code += b'\x48\x85\xc0\x74\x02\x0f\x0b\x8b\x0b\xba\x22\x22\x00\x00'
    call(0x2128)
    code += b'\xb8\x42\x00\x00\x00\x48\x83\xc4\x20\x5b\xc3'
    assert len(code) <= 0x100
    threaded[0x500:0x600] = code.ljust(0x100, b'\0')
    result = run(threaded)
    assert result.returncode == 0 and 'GE6_NATIVE_THREAD_FIRST_OS name=GetCurrentThreadId entry=0x140001100' in result.stdout, result
    assert 'GE6_NATIVE_THREAD_EXIT entry=0x140001100' in result.stdout and 'code=66' in result.stdout, result
    network = fixture()
    network[0x640:0x660] = b'WS2_32.dll\0'.ljust(32, b'\0')
    for i, ordinal in enumerate((115, 116, 111, 0, 0)):
        value = (1 << 63) | ordinal if ordinal else 0
        struct.pack_into('<Q', network, 0x680 + i * 8, value)
        struct.pack_into('<Q', network, 0x700 + i * 8, value)
    network[0xc00 + 408] = 0xa5
    code = bytearray(b'\x48\x83\xec\x28')
    def net_call(iat):
        code.extend(b'\xff\x15' + struct.pack('<i', iat - (0x1000 + len(code) + 6)))
    code += b'\xb9\x02\x02\x00\x00\x48\xba' + struct.pack('<Q', 0x140003000)
    net_call(0x2100)
    code += b'\x85\xc0\x74\x02\x0f\x0b\x48\xb8' + struct.pack('<Q', 0x140003000)
    code += b'\x81\x38\x02\x02\x02\x02\x74\x02\x0f\x0b'
    code += b'\x80\xb8' + struct.pack('<I', 408) + b'\xa5\x74\x02\x0f\x0b'
    net_call(0x2108)
    code += b'\x85\xc0\x74\x02\x0f\x0b'
    net_call(0x2108)
    code += b'\x83\xf8\xff\x74\x02\x0f\x0b'
    net_call(0x2110)
    code += b'\x3d' + struct.pack('<I', 10093) + b'\x74\x02\x0f\x0b\x48\x83\xc4\x28\xc3'
    network[0x400:0x600] = code.ljust(0x200, b'\x00')
    result = run(network)
    assert result.returncode == 0 and 'supported_imports=3 blocked_imports=0' in result.stdout and 'GE6_OS WSAStartup' in result.stdout, result
    local = fixture()
    local[0x640:0x640 + 15] = b'fs15_local.dll\0'
    names = (b'SteamAPI_Init', b'SteamApps', b'SteamUtils', b'SteamUser', b'SteamUserStats')
    for i, name in enumerate(names):
        start = 0x802 + i * 64
        local[start:start + 64] = bytes(64)
        local[start:start + len(name)] = name
    code = bytearray(b'\x48\x83\xec\x28')
    def call(iat):
        code.extend(b'\xff\x15' + struct.pack('<i', iat - (0x1000 + len(code) + 6)))
    for i in (0, 1, 3, 4):
        call(0x2100 + i * 8)
        code += b'\x48\x85\xc0\x74\x02\x0f\x0b'
    call(0x2110)
    code += b'\x48\x89\xc1\x48\x8b\x00\xff\x50\x48'
    code += b'\x3d' + struct.pack('<I', 313160) + b'\x74\x02\x0f\x0b\x48\x83\xc4\x28\xc3'
    local[0x400:0x600] = code.ljust(0x200, b'\x00')
    result = run(local)
    assert result.returncode == 0 and 'INIT_UNAVAILABLE' in result.stdout and 'UTIL_APP_ID' in result.stdout, result
    assert 'APPS_UNAVAILABLE' in result.stdout and 'USER_UNAVAILABLE' in result.stdout, result
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
