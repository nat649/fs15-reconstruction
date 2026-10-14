"""Capture ONLY the game's loaded code section for static analysis.

This uses the normally installed game, leaves its files and process unchanged,
and does not capture other modules or writable game data. The output is an
analysis input, not a rebuilt or distributable executable.
"""
import argparse
import ctypes as C
from ctypes import wintypes as W
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import time
from inventory import PE, ROOT, DEFAULT_GAME, entropy, sha256, write_json

K32 = C.WinDLL("kernel32", use_last_error=True)
INVALID_HANDLE = C.c_void_p(-1).value


class PROCESSENTRY32W(C.Structure):
    _fields_ = [("dwSize", W.DWORD), ("cntUsage", W.DWORD), ("th32ProcessID", W.DWORD),
                ("th32DefaultHeapID", C.c_size_t), ("th32ModuleID", W.DWORD),
                ("cntThreads", W.DWORD), ("th32ParentProcessID", W.DWORD),
                ("pcPriClassBase", W.LONG), ("dwFlags", W.DWORD), ("szExeFile", W.WCHAR * 260)]


class MODULEENTRY32W(C.Structure):
    _fields_ = [("dwSize", W.DWORD), ("th32ModuleID", W.DWORD), ("th32ProcessID", W.DWORD),
                ("GlblcntUsage", W.DWORD), ("ProccntUsage", W.DWORD),
                ("modBaseAddr", C.c_void_p), ("modBaseSize", W.DWORD), ("hModule", W.HMODULE),
                ("szModule", W.WCHAR * 256), ("szExePath", W.WCHAR * 260)]


K32.CreateToolhelp32Snapshot.argtypes = [W.DWORD, W.DWORD]
K32.CreateToolhelp32Snapshot.restype = W.HANDLE
K32.Process32FirstW.argtypes = [W.HANDLE, C.POINTER(PROCESSENTRY32W)]
K32.Process32NextW.argtypes = [W.HANDLE, C.POINTER(PROCESSENTRY32W)]
K32.Module32FirstW.argtypes = [W.HANDLE, C.POINTER(MODULEENTRY32W)]
K32.Module32NextW.argtypes = [W.HANDLE, C.POINTER(MODULEENTRY32W)]
K32.CloseHandle.argtypes = [W.HANDLE]
K32.OpenProcess.argtypes = [W.DWORD, W.BOOL, W.DWORD]
K32.OpenProcess.restype = W.HANDLE
K32.ReadProcessMemory.argtypes = [W.HANDLE, C.c_void_p, C.c_void_p, C.c_size_t, C.POINTER(C.c_size_t)]
K32.ReadProcessMemory.restype = W.BOOL


def processes():
    snapshot = K32.CreateToolhelp32Snapshot(2, 0)
    if snapshot == INVALID_HANDLE:
        raise C.WinError(C.get_last_error())
    try:
        item = PROCESSENTRY32W()
        item.dwSize = C.sizeof(item)
        ok = K32.Process32FirstW(snapshot, C.byref(item))
        while ok:
            if item.szExeFile.lower() == "farmingsimulator2015game.exe":
                yield item.th32ProcessID
            ok = K32.Process32NextW(snapshot, C.byref(item))
    finally:
        K32.CloseHandle(snapshot)


def module(pid):
    snapshot = K32.CreateToolhelp32Snapshot(8 | 16, pid)
    if snapshot == INVALID_HANDLE:
        raise C.WinError(C.get_last_error())
    try:
        item = MODULEENTRY32W()
        item.dwSize = C.sizeof(item)
        ok = K32.Module32FirstW(snapshot, C.byref(item))
        while ok:
            if item.szModule.lower() == "farmingsimulator2015game.exe":
                return item.modBaseAddr, Path(item.szExePath)
            ok = K32.Module32NextW(snapshot, C.byref(item))
    finally:
        K32.CloseHandle(snapshot)
    raise RuntimeError(f"Game module not found in PID {pid}")


def read_memory(handle, address, size):
    buf = C.create_string_buffer(size)
    read = C.c_size_t()
    if not K32.ReadProcessMemory(handle, address, buf, size, C.byref(read)):
        raise C.WinError(C.get_last_error())
    if read.value != size:
        raise RuntimeError(f"Partial memory read at {address:x}: {read.value}/{size}")
    return buf.raw


def restore_relocations(image, pe, section, delta):
    patched = 0
    if not delta or len(pe.directories) <= 5 or not pe.directories[5][0]:
        return patched
    relocation_rva, relocation_size = pe.directories[5]
    pos = pe.offset(relocation_rva)
    end = pos + relocation_size
    while pos + 8 <= end:
        page, block_size = struct.unpack_from("<II", pe.data, pos)
        if block_size < 8 or pos + block_size > end:
            raise ValueError("Invalid PE relocation block")
        for offset in range(pos + 8, pos + block_size, 2):
            entry = struct.unpack_from("<H", pe.data, offset)[0]
            typ, rva = entry >> 12, page + (entry & 0xfff)
            if section["rva"] <= rva < section["rva"] + section["raw_size"]:
                if typ in (3, 10):
                    fmt = "<I" if typ == 3 else "<Q"
                    mask = (1 << (32 if typ == 3 else 64)) - 1
                    file_offset = pe.offset(rva)
                    value = struct.unpack_from(fmt, image, file_offset)[0]
                    struct.pack_into(fmt, image, file_offset, (value - delta) & mask)
                    patched += 1
                elif typ != 0:
                    raise ValueError(f"Unsupported code relocation {typ}")
        pos += block_size
    return patched


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--launch-game", action="store_true")
    parser.add_argument("--via-steam", action="store_true", help="Use the installed Steam client to start app 313160 normally")
    parser.add_argument("--steam", type=Path, default=Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) / "Steam/steam.exe")
    parser.add_argument("--game", type=Path, default=DEFAULT_GAME)
    parser.add_argument("--wait", type=int, default=55)
    args = parser.parse_args()
    game = args.game.resolve(strict=True)
    existing = list(processes())
    if not existing and args.launch_game:
        launcher = args.steam if args.via_steam else game / "FarmingSimulator2015.exe"
        launch_args = [str(launcher), "-applaunch", "313160"] if args.via_steam else [str(launcher)]
        child = subprocess.Popen(launch_args, cwd=game)
        print(f"Started normally: {launcher.name}, PID {child.pid}", flush=True)
    deadline = time.monotonic() + args.wait
    last_error = "No game process detected"
    while time.monotonic() < deadline:
        for pid in processes():
            handle = None
            try:
                base, source = module(pid)
                if source.parent != game / "x64":
                    last_error = f"Expected x64 game, found {source}"
                    continue
                disk = source.read_bytes()
                pe = PE(disk)
                section = next(s for s in pe.sections if s["name"] == ".text")
                handle = K32.OpenProcess(0x410, False, pid)
                if not handle:
                    raise C.WinError(C.get_last_error())
                sample = read_memory(handle, base + section["rva"], min(65536, section["virtual_size"]))
                sample_entropy = entropy(sample)
                if sample_entropy > 7.7 or sample_entropy < 1.0:
                    last_error = f"Code not ready: entropy {sample_entropy}"
                    continue
                # Raw section padding can extend beyond VirtualSize: use disk padding there.
                length = min(section["raw_size"], section["virtual_size"])
                code = read_memory(handle, base + section["rva"], length)
                image = bytearray(disk)
                image[section["raw_offset"]:section["raw_offset"] + length] = code
                fixed = restore_relocations(image, pe, section, base - pe.base)
                out = ROOT / "inputs" / "runtime" / "FarmingSimulator2015Game.runtime.bin"
                out.parent.mkdir(parents=True, exist_ok=True)
                out.write_bytes(image)
                provenance = dict(process_id=pid, module_path=str(source),
                                  captured_utc=__import__('datetime').datetime.now(__import__('datetime').timezone.utc).isoformat(),
                                  disk_sha256=hashlib.sha256(disk).hexdigest(),
                                  analysis_image_sha256=sha256(out), runtime_base=f"0x{base:x}",
                                  preferred_base=f"0x{pe.base:x}", captured_section=".text", captured_bytes=length,
                                  original_code_entropy=section["entropy"], loaded_code_entropy=entropy(code),
                                  relocations_restored=fixed,
                                  note="Only .text replaced with normally loaded code, ASLR normalized. Analysis image only; not a rebuild.")
                write_json(ROOT / "analysis" / "runtime-provenance.json", provenance)
                print(json.dumps(provenance, indent=2), flush=True)
                return
            except (OSError, ValueError, RuntimeError) as error:
                last_error = str(error)
            finally:
                if handle:
                    K32.CloseHandle(handle)
        time.sleep(1)
    raise RuntimeError(f"No usable runtime code captured: {last_error}")


if __name__ == "__main__":
    main()
