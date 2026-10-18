"""Version-pinned native reference for investigating the FS15 GAR format.

This oracle requires the user's locally captured 1.4.2.0 x64 image. It calls
the observed cipher initializer and buffer decoder in this process. Other
comparison scripts use its pinned mapping for inspected routines; the optional
auxiliary math section is needed by the tire-force comparison. It never
executes the PE entry point or performs game/license initialization.
It is an original-code dependency, not a reconstructed cipher implementation.
"""
import ctypes as C
import hashlib
from pathlib import Path
import struct
from inventory import PE, ROOT

EXPECTED_IMAGE = "6d1beaf9d7e3c57dd51cec03385a53bda82b4ac946e622d8d2fa592a56d71f9f"
DEFAULT_IMAGE = ROOT / "inputs/runtime/FarmingSimulator2015Game.runtime.bin"


class NativeGarCipher:
    def __init__(self, image=DEFAULT_IMAGE, *, math_helpers=False):
        if C.sizeof(C.c_void_p) != 8:
            raise RuntimeError("The native reference requires 64-bit Windows Python")
        data = Path(image).read_bytes()
        if hashlib.sha256(data).hexdigest() != EXPECTED_IMAGE:
            raise ValueError("Unsupported reference image; regenerate the verified FS15 1.4.2.0 snapshot")
        pe = PE(data)
        # Observed GAR constructor arguments in 0x14027d5f0, read from local code.
        self.key = struct.unpack_from("<Q", data, pe.offset(0x27d702))[0]
        self.iv = struct.unpack_from("<Q", data, pe.offset(0x27d6f8))[0]
        self.lua_key = data[pe.offset(0x4a8278):pe.offset(0x4a8278) + 8]
        self.kernel = C.WinDLL("kernel32", use_last_error=True)
        self.kernel.VirtualAlloc.argtypes = [C.c_void_p, C.c_size_t, C.c_uint32, C.c_uint32]
        self.kernel.VirtualAlloc.restype = C.c_void_p
        self.kernel.VirtualProtect.argtypes = [C.c_void_p, C.c_size_t, C.c_uint32, C.POINTER(C.c_uint32)]
        self.kernel.VirtualFree.argtypes = [C.c_void_p, C.c_size_t, C.c_uint32]
        self.kernel.FlushInstructionCache.argtypes = [C.c_void_p, C.c_void_p, C.c_size_t]
        self.kernel.GetCurrentProcess.restype = C.c_void_p
        self.base = 0
        try:
            end = max(s["rva"] + max(s["virtual_size"], s["raw_size"]) for s in pe.sections)
            self.base = self.kernel.VirtualAlloc(pe.base, end, 0x3000, 0x04)
            if self.base != pe.base:
                raise RuntimeError("The native reference's preferred address range is unavailable")
            C.memmove(self.base, data[:pe.size_headers], pe.size_headers)
            for section in pe.sections:
                # Optional inspected math helpers AND their separate constant
                # section. In particular powf reads masks/1.0 from `data`;
                # mapping only `text` silently makes even powf(1, dt) wrong.
                # Never map/execute .bind or the entry point.
                if section["name"] not in ((".text", ".rdata", ".data", ".pdata", "text", "data") if math_helpers else (".text", ".rdata", ".data", ".pdata")):
                    continue
                block = data[section["raw_offset"]:section["raw_offset"] + section["raw_size"]]
                C.memmove(self.base + section["rva"], block, len(block))
                old = C.c_uint32()
                protection = 0x20 if section["name"] in (".text", "text") else 0x04 if section["name"] == ".data" else 0x02
                if not self.kernel.VirtualProtect(self.base + section["rva"], max(section["virtual_size"], len(block)), protection, C.byref(old)):
                    raise C.WinError(C.get_last_error())
            self.kernel.FlushInstructionCache(self.kernel.GetCurrentProcess(), self.base, end)
            self.state = C.create_string_buffer(0x3b0)
            initialize = C.WINFUNCTYPE(None, C.c_void_p, C.c_uint64, C.c_uint64)(self.base + 0x2e6e60)
            initialize(self.state, self.key, self.iv)
            self.initial_state = self.state.raw
            self.decode = C.WINFUNCTYPE(None, C.c_void_p, C.c_void_p, C.c_uint32, C.c_void_p)(self.base + 0x2e7500)
        except BaseException:
            self.close()
            raise

    def decrypt(self, encrypted):
        if not self.base:
            raise RuntimeError("Native reference is closed")
        if len(encrypted) < 8 or len(encrypted) % 8 or len(encrypted) > 0xffffffff:
            raise ValueError("Expected complete 8-byte blocks fitting a uint32 length")
        source = C.create_string_buffer(encrypted)
        output = C.create_string_buffer(len(encrypted))
        C.memmove(self.state, self.initial_state, len(self.initial_state))
        self.decode(self.state, source, len(encrypted), output)
        return output.raw

    def close(self):
        if self.base:
            self.kernel.VirtualFree(self.base, 0, 0x8000)
            self.base = 0

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()
