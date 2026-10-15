"""Compare reconstructed RLE against the original validated 823-byte native leaf.

The original leaf executes only in this disposable test process, not in the
game. Ghidra has checked that it has no calls or external memory references.
Only the pinned local reference with the known SHA-256 is accepted.
"""
import argparse
import ctypes as C
import hashlib
import json
from pathlib import Path
import random
from inventory import ROOT

REFERENCE_SHA256 = "b0a14bc2a8764b6cb979ff372730c6fab11339645fc284fa0fb09e5d75c7c83c"
PAGE = 65536
BYTE = C.c_uint8


class Input(C.Structure):
    _fields_ = [("data", C.POINTER(BYTE)), ("size", C.c_uint32), ("cursor", C.c_uint32)]


class BitVector(C.Structure):
    _fields_ = [("segments", C.POINTER(C.POINTER(BYTE)))]


def encode(values):
    result = bytearray()
    pos = 0
    while pos < len(values):
        end = pos + 1
        while end < len(values) and values[end] == values[pos]:
            end += 1
        length = end - pos
        result.append(values[pos])
        if length > 1:
            result.append(values[pos])
            extra = length - 2
            while extra >= 255:
                result.append(255)
                extra -= 255
            result.append(extra)
        pos = end
    return bytes(result)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--dll", type=Path, required=True)
    parser.add_argument("--reference", type=Path, default=ROOT / "inputs/reference/rle8-reference.bin")
    args = parser.parse_args()
    code = args.reference.read_bytes()
    if hashlib.sha256(code).hexdigest() != REFERENCE_SHA256 or len(code) != 823:
        raise ValueError("Native reference differs from the inspected original leaf")
    metadata = json.loads(args.reference.with_suffix(".json").read_text())
    if metadata["external_calls"] or metadata["external_memory_references"]:
        raise ValueError("Native reference is not standalone")
    kernel = C.WinDLL("kernel32", use_last_error=True)
    kernel.VirtualAlloc.argtypes = [C.c_void_p, C.c_size_t, C.c_uint32, C.c_uint32]
    kernel.VirtualAlloc.restype = C.c_void_p
    kernel.VirtualProtect.argtypes = [C.c_void_p, C.c_size_t, C.c_uint32, C.POINTER(C.c_uint32)]
    kernel.VirtualFree.argtypes = [C.c_void_p, C.c_size_t, C.c_uint32]
    kernel.FlushInstructionCache.argtypes = [C.c_void_p, C.c_void_p, C.c_size_t]
    kernel.GetCurrentProcess.restype = C.c_void_p
    memory = kernel.VirtualAlloc(None, len(code), 0x3000, 0x04)
    if not memory:
        raise C.WinError(C.get_last_error())
    C.memmove(memory, code, len(code))
    old = C.c_uint32()
    if not kernel.VirtualProtect(memory, len(code), 0x20, C.byref(old)):
        raise C.WinError(C.get_last_error())
    kernel.FlushInstructionCache(kernel.GetCurrentProcess(), memory, len(code))
    native = C.WINFUNCTYPE(None, C.POINTER(Input), C.c_int, BYTE, C.POINTER(BitVector))(memory)
    rebuilt = C.CDLL(str(args.dll.resolve())).fs15_decode_rle8
    rebuilt.argtypes = [C.POINTER(BYTE), C.c_size_t, C.c_uint, C.c_uint, C.POINTER(BYTE), C.c_size_t]
    rebuilt.restype = C.c_size_t
    randomizer = random.Random(0xF515)
    count = 0
    total_samples = 0

    def compare(values, width, stride, label, initial=0):
        nonlocal count, total_samples
        encoded = encode(values)
        source = (BYTE * max(1, len(encoded))).from_buffer_copy(encoded or b"\0")
        state = Input(source, len(encoded), 0)
        size = max(1, (len(values) * stride + 7) // 8 + 4)
        segment_count = (size + PAGE - 1) // PAGE
        segments = [(BYTE * (PAGE + 4))(*([initial] * (PAGE + 4))) for _ in range(segment_count)]
        pointers = (C.POINTER(BYTE) * segment_count)(*[C.cast(s, C.POINTER(BYTE)) for s in segments])
        vector = BitVector(pointers)
        candidate = (BYTE * size)(*([initial] * size))
        native(C.byref(state), stride, width, C.byref(vector))
        actual_count = rebuilt(source, len(encoded), stride, width, candidate, size)
        original = b"".join(bytes(segment)[:PAGE] for segment in segments)[:size]
        if actual_count != len(values) or state.cursor != len(encoded) or bytes(candidate) != original:
            mismatch = next((i for i, (a, b) in enumerate(zip(bytes(candidate), original)) if a != b), None)
            raise AssertionError(f"{label}: samples {actual_count}/{len(values)}, cursor {state.cursor}/{len(encoded)}, first mismatch {mismatch}")
        # The seed values are independently packed to catch a faulty stream generator.
        expected = bytearray([initial] * size)
        for i, value in enumerate(values):
            for bit in range(width):
                offset = i * stride + bit
                mask = 1 << (offset & 7)
                if value & (1 << bit):
                    expected[offset >> 3] |= mask
                else:
                    expected[offset >> 3] &= ~mask
        if original != expected:
            raise AssertionError(f"{label}: original native output disagrees with the independently packed samples")
        count += 1
        total_samples += len(values)

    try:
        for width in range(1, 9):
            mask = (1 << width) - 1
            for stride in sorted({width, width + 3, 16}):
                compare([], width, stride, "empty")
                compare([0, mask, 0, mask], width, stride, "literals", 0xa5)
                for length in (2, 3, 256, 257, 258, 512, 513, 1024):
                    compare([mask] * length + [0], width, stride, f"run-{length}", 0xa5)
                values = []
                for _ in range(25):
                    value = randomizer.randint(0, mask)
                    values.extend([value] * randomizer.randint(1, 600))
                compare(values, width, stride, "seeded-runs")
        # Force reads and writes across the original 64 KiB BitVector segments.
        compare([255] * 70000 + [0, 2, 3], 8, 8, "segment-boundary", 0xa5)
        compare([1] * 530000 + [0, 1, 0], 1, 1, "bit-segment-boundary", 0xa5)
        # Deliberate bounded-output behavior added by the reconstruction.
        source = (BYTE * 3)(1, 1, 100)
        output = (BYTE * 1)()
        if rebuilt(source, 3, 8, 8, output, 1) != C.c_size_t(-1).value:
            raise AssertionError("Output overflow was not rejected")
        result = dict(status="passed", native_reference_sha256=REFERENCE_SHA256,
                      rebuilt_source_sha256={path:hashlib.sha256((ROOT/path).read_bytes()).hexdigest()
                                             for path in ('src/core/rle8.cpp','src/core/api.h','src/core/export.h')},
                      original_function="0x14028f430", differential_cases=count,
                      decoded_samples=total_samples, added_capacity_check="passed",
                      scope="Valid RLE8, 1..8-bit values, strides up to 16 in these tests; native 64 KiB segment boundaries included")
        (ROOT / "analysis/rle8-validation.json").write_text(json.dumps(result, indent=2) + "\n")
        print(json.dumps(result, indent=2))
    finally:
        kernel.VirtualFree(memory, 0, 0x8000)


if __name__ == "__main__":
    main()
