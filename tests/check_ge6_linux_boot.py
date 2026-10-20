"""Exercise the native-code loader with authored PE instructions, never game code."""
import argparse
import os
import select
import signal
from pathlib import Path
import struct
import subprocess
import tempfile
import time


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


def audio_fixture():
    """Authored Windows x64 instructions call real Linux OpenAL via the IAT."""
    data = fixture()
    data[0x640:0x650] = b'OpenAL32.dll\0'.ljust(16, b'\0')
    names = ('alcOpenDevice', 'alcCreateContext', 'alcMakeContextCurrent',
             'alListenerf', 'alGetListenerf', 'alGetError', 'alcDestroyContext', 'alcCloseDevice')
    data[0x680:0x700] = bytes(0x80)
    data[0x700:0x780] = bytes(0x80)
    for index, name in enumerate(names):
        for table in (0x680, 0x700):
            struct.pack_into('<Q', data, table + index * 8, 0x2200 + index * 64)
        at = 0x802 + index * 64
        encoded = name.encode() + b'\0'
        data[at:at + len(encoded)] = encoded
    code = bytearray(b'\x48\x83\xec\x28')
    def call(index):
        code.extend(b'\xff\x15' + struct.pack('<i', 0x2100 + index * 8 - (0x1000 + len(code) + 6)))
    def load_rcx(address):
        code.extend(b'\x48\xb9' + struct.pack('<Q', address) + b'\x48\x8b\x09')
    def save_rax(address):
        code.extend(b'\x48\xa3' + struct.pack('<Q', address))
    def nonnull():
        code.extend(b'\x48\x85\xc0\x75\x02\x0f\x0b')
    code += b'\x31\xc9'  # Linux default selected explicitly by test environment.
    call(0); nonnull(); save_rax(0x140003000)
    load_rcx(0x140003000); code += b'\x31\xd2'
    call(1); nonnull(); save_rax(0x140003008)
    load_rcx(0x140003008); call(2)
    code += b'\x3c\x01\x74\x02\x0f\x0b'
    # Microsoft ABI's second argument is XMM1 (not the System V XMM0).
    code += b'\xb9\x0a\x10\x00\x00\xb8' + struct.pack('<f', 0.375) + b'\x66\x0f\x6e\xc8'
    call(3)
    code += b'\xb9\x0a\x10\x00\x00\x48\xba' + struct.pack('<Q', 0x140003010)
    call(4)
    code += b'\x48\xb8' + struct.pack('<Q', 0x140003010) + b'\x81\x38' + struct.pack('<f', 0.375) + b'\x74\x02\x0f\x0b'
    call(5); code += b'\x85\xc0\x74\x02\x0f\x0b'
    code += b'\x31\xc9'; call(2)
    load_rcx(0x140003008); call(6)
    load_rcx(0x140003000); call(7)
    code += b'\x3c\x01\x74\x02\x0f\x0b\x48\x83\xc4\x28\xc3'
    assert len(code) <= 0x200
    data[0x400:0x600] = code.ljust(0x200, b'\0')
    return data


def console_fixture():
    """A PE callback receives a real SIGINT on another native ABI thread."""
    data = fixture()
    data[0x680:0x700] = bytes(0x80)
    data[0x700:0x780] = bytes(0x80)
    data[0xc00:0xc40] = bytes(0x40)
    for index, name in enumerate((b'SetConsoleCtrlHandler', b'Sleep', b'GetCurrentThreadId')):
        for table in (0x680, 0x700):
            struct.pack_into('<Q', data, table + index * 8, 0x2200 + index * 64)
        at = 0x802 + index * 64
        data[at:at + len(name) + 1] = name + b'\0'
    code = bytearray(b'\x48\x83\xec\x28')
    def call(index, start=0x1000):
        code.extend(b'\xff\x15' + struct.pack('<i', 0x2100 + index * 8 - (start + len(code) + 6)))
    call(2)
    code += b'\xa3' + struct.pack('<Q', 0x140003014)
    code += b'\x48\xb9' + struct.pack('<Q', 0x140001100) + b'\xba\x01\x00\x00\x00'
    call(0)
    code += b'\x85\xc0\x75\x02\x0f\x0b'
    loop = len(code)
    code += b'\x48\xb8' + struct.pack('<Q', 0x140003000) + b'\x83\x38\x01\x74\x0d\xb9\x05\x00\x00\x00'
    call(1)
    code += b'\xeb' + struct.pack('<b', loop - (len(code) + 2))
    # Compare the callback's Linux TID against the PE main thread's TID.
    code += b'\x48\xb8' + struct.pack('<Q', 0x140003010) + b'\x8b\x08\x3b\x48\x04\x75\x02\x0f\x0b'
    code += b'\x48\xb9' + struct.pack('<Q', 0x140001100) + b'\x31\xd2'
    call(0)
    code += b'\x48\x83\xc4\x28\xc3'
    assert len(code) <= 0x100
    data[0x400:0x500] = code.ljust(0x100, b'\0')
    code = bytearray(b'\x48\x83\xec\x28\x85\xc9\x74\x02\x0f\x0b')
    code += b'\x65\x48\x8b\x04\x25\x30\x00\x00\x00\x48\x3b\x40\x30\x74\x02\x0f\x0b'
    call(2, 0x1100)
    code += b'\xa3' + struct.pack('<Q', 0x140003010)
    code += b'\x48\xb8' + struct.pack('<Q', 0x140003000) + b'\xc7\x00\x01\x00\x00\x00'
    code += b'\xb8\x01\x00\x00\x00\x48\x83\xc4\x28\xc3'
    data[0x500:0x600] = code.ljust(0x100, b'\0')
    return data


def window_resource_fixture():
    """An authored PE loads a real icon and registers its native class layout."""
    data = fixture()
    data[0x640:0x650] = b'USER32.dll\0'.ljust(16, b'\0')
    data[0x680:0x700] = bytes(0x80)
    data[0x700:0x780] = bytes(0x80)
    data[0xc00:] = bytes(0x400)
    for index, name in enumerate((b'LoadImageW', b'RegisterClassW', b'DestroyIcon', b'UnregisterClassW')):
        for table in (0x680, 0x700):
            struct.pack_into('<Q', data, table + index * 8, 0x2200 + index * 64)
        at = 0x802 + index * 64
        data[at:at + len(name) + 1] = name + b'\0'
    struct.pack_into('<II', data, 0x98 + 128, 0x3200, 0x200)
    # WNDCLASSW, copied UTF-16 class name, and a harmless native procedure.
    struct.pack_into('<I4xQiiQQQQQQ', data, 0xc00, 0x1020, 0x140001100, 0, 0, 0x140000000, 0, 0, 0, 0, 0x140003080)
    label = 'AuthoredWindow\0'.encode('utf-16le')
    data[0xc80:0xc80 + len(label)] = label
    def put(at, fmt, *values):
        struct.pack_into('<' + fmt, data, 0xe00 + at, *values)
    put(14, 'H', 2)
    put(16, 'II', 3, 0x800000c0); put(24, 'II', 14, 0x80000020)
    put(0x2e, 'H', 1); put(0x30, 'II', 101, 0x80000050)
    put(0x5e, 'H', 1); put(0x60, 'II', 1033, 0x70)
    put(0x70, 'II', 0x32a0, 20)
    put(0xa0, 'HHHBBBBHHIH', 0, 1, 1, 2, 2, 0, 0, 1, 32, 64, 7)
    put(0xce, 'H', 1); put(0xd0, 'II', 7, 0x800000e0)
    put(0xee, 'H', 1); put(0xf0, 'II', 1033, 0x100)
    put(0x100, 'II', 0x3340, 64)
    put(0x140, 'IiiHH', 40, 2, 4, 1, 32)
    data[0xf68:0xf78] = bytes((255, 0, 0, 255)) * 4
    code = bytearray(b'\x48\x83\xec\x38')
    def call(index):
        code.extend(b'\xff\x15' + struct.pack('<i', 0x2100 + index * 8 - (0x1000 + len(code) + 6)))
    def pointer(value):
        code.extend(b'\x48\xb9' + struct.pack('<Q', value))
    def success():
        code.extend(b'\x48\x85\xc0\x75\x02\x0f\x0b')
    pointer(0x140000000)
    code += b'\xba\x65\x00\x00\x00\x41\xb8\x01\x00\x00\x00\x45\x31\xc9'
    code += b'\xc7\x44\x24\x20\x00\x00\x00\x00\xc7\x44\x24\x28\x00\x00\x00\x00'
    call(0); success()
    code += b'\x48\xa3' + struct.pack('<Q', 0x140003020)
    pointer(0x140003000); call(1); success()
    pointer(0x140003020); code += b'\x48\x8b\x09'; call(2); success()
    pointer(0x140003080); code += b'\x48\xba' + struct.pack('<Q', 0x140000000); call(3); success()
    code += b'\x48\x83\xc4\x38\xc3'
    assert len(code) < 0x100
    data[0x400:0x500] = code.ljust(0x100, b'\0')
    data[0x500:0x506] = b'\x31\xc0\xc3\x90\x90\x90'
    return data


def message_fixture():
    """Seven Microsoft-ABI arguments, UTF-16 result and untouched buffer guard."""
    data = fixture()
    data[0x680:0x700] = bytes(0x80)
    data[0x700:0x780] = bytes(0x80)
    for table in (0x680, 0x700):
        struct.pack_into('<Q', data, table, 0x2200)
    data[0x802:0x811] = b'FormatMessageW\0'
    data[0xc00:0xc80] = bytes((0xa5,)) * 0x80
    code = bytearray(b'\x48\x83\xec\x38\xb9\x00\x10\x00\x00\x31\xd2\x41\xb8\x7f\x00\x00\x00\x45\x31\xc9')
    code += b'\x48\xb8' + struct.pack('<Q', 0x140003000) + b'\x48\x89\x44\x24\x20'
    code += b'\x48\xc7\x44\x24\x28\x40\x00\x00\x00\x48\xc7\x44\x24\x30\x00\x00\x00\x00'
    code += b'\xff\x15' + struct.pack('<i', 0x2100 - (0x1000 + len(code) + 6))
    code += b'\x83\xf8\x2d\x74\x02\x0f\x0b\x48\xb8' + struct.pack('<Q', 0x140003000)
    code += b'\x81\x38' + struct.pack('<I', 0x00680054) + b'\x74\x02\x0f\x0b'
    code += b'\x48\xb8' + struct.pack('<Q', 0x14000305a)
    code += b'\x81\x38' + struct.pack('<I', 0xa5a50000) + b'\x74\x02\x0f\x0b\x48\x83\xc4\x38\xc3'
    data[0x400:0x600] = code.ljust(0x200, b'\0')
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
    subprocess.run([str(exe.resolve()), '--check-environment'], check=True, timeout=15)
    subprocess.run([str(exe.resolve()), '--check-window-resources'], check=True, timeout=15)
    subprocess.run([str(exe.resolve()), '--check-modules'], check=True, timeout=15)
    subprocess.run([str(exe.resolve()), '--check-user-module'], check=True, timeout=15)
    subprocess.run([str(exe.resolve()), '--check-format-message'], check=True, timeout=15)
    message_path = workdir / 'authored-system-message.exe'
    message_path.write_bytes(message_fixture())
    message = subprocess.run([str(exe.resolve()), '--fixture', str(message_path)], capture_output=True, text=True, timeout=15)
    assert message.returncode == 0 and 'GE6_AUTHORED_FIXTURE_RETURN' in message.stdout, message.stdout + message.stderr
    window_path = workdir / 'authored-window-resources.exe'
    window_path.write_bytes(window_resource_fixture())
    window = subprocess.run([str(exe.resolve()), '--fixture', str(window_path)], capture_output=True, text=True, timeout=15)
    assert window.returncode == 0 and 'GE6_IMAGE_RESOURCE_LOADED first=2 second=2' in window.stdout and 'GE6_WINDOW_CLASS_REGISTER' in window.stdout and 'GE6_AUTHORED_FIXTURE_RETURN' in window.stdout, window.stdout + window.stderr
    subprocess.run([str(exe.resolve()), '--check-console'], check=True, timeout=15, stdout=subprocess.PIPE)
    unhandled = subprocess.run([str(exe.resolve()), '--check-console-default'], capture_output=True, text=True, timeout=15)
    assert unhandled.returncode == 130 and 'GE6_CONSOLE_CALL' in unhandled.stdout, unhandled
    console_path = workdir / 'authored-console-abi.exe'
    console_path.write_bytes(console_fixture())
    process = subprocess.Popen([str(exe.resolve()), '--fixture', str(console_path)], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    prefix = b''
    try:
        deadline = time.monotonic() + 5
        while b'GE6_CONSOLE_REGISTER entry=0x140001100' not in prefix:
            remaining = deadline - time.monotonic()
            assert remaining > 0 and select.select([process.stdout], [], [], remaining)[0], prefix
            chunk = os.read(process.stdout.fileno(), 65536)
            assert chunk, prefix
            prefix += chunk
        process.send_signal(signal.SIGINT)
        output, error = process.communicate(timeout=10)
        output = prefix + output
        assert process.returncode == 0 and b'GE6_CONSOLE_CALL entry=0x140001100' in output and b'GE6_AUTHORED_FIXTURE_RETURN' in output, output + error
    finally:
        if process.poll() is None:
            process.kill()
            process.communicate()
    audio_env = dict(os.environ, ALSOFT_DRIVERS='null')
    subprocess.run([str(exe.resolve()), '--check-audio'], check=True, timeout=15, env=audio_env)
    audio_path = workdir / 'authored-openal-abi.exe'
    audio_path.write_bytes(audio_fixture())
    audio = subprocess.run([str(exe.resolve()), '--fixture', str(audio_path)], capture_output=True, text=True, timeout=15, env=audio_env)
    assert audio.returncode == 0 and 'GE6_AUTHORED_FIXTURE_RETURN' in audio.stdout, audio.stdout + audio.stderr
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
