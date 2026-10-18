# Recovered GE6 OpenGL buffers

Nine additional native paths now compile as portable C++17 in
[`gl_buffers.cpp`](../src/core/gl_buffers.cpp). These are recovered algorithms,
with explicit authored services for the original process-global GL dispatch,
allocator, counters and shared staging storage. They are separate from ReFarm's
authored D3D11 renderer and from the CPU buffers in the portable headless host.

| Native entry | Bounded recovered path |
|---|---|
| `0x140065c60` | Vertex allocation, GL storage, usage selection and cached binding invalidation |
| `0x140074850` | Index construction and unsigned-short/unsigned-int format selection |
| `0x140074920` | Index deletion, byte counter and base-vtable transition |
| `0x140074990` | Index mapping, optional orphaning and byte offset |
| `0x140074a10` | Index unmapping |
| `0x140074a40` | Vertex deletion, byte counter and base-vtable transition |
| `0x140074ab0` | Shared staging or direct vertex mapping |
| `0x140074b70` | Vertex unmapping or staged upload |
| `0x140074bd0` | Vertex subdata transfer, optionally orphaning storage first |

The native layouts retain opaque bytes, numeric vtable identities and unsigned
32-bit byte-counter wrapping. Original pointers are never executed by the port.
The allocator's CRT exception machinery is excluded; a failed authored allocator
returns null. The public API guards missing services and undersized caller
storage. It does not validate arbitrary native pointer graphs or GL ranges.

## Transfer semantics

Vertex storage uses `GL_ARRAY_BUFFER`; index storage uses
`GL_ELEMENT_ARRAY_BUFFER`. Descriptor flags select `GL_DYNAMIC_DRAW`,
`GL_STATIC_DRAW` or `GL_STREAM_DRAW` exactly as in the inspected instructions.
Mode `1` discards old storage, mode `2` maps for reading, other modes write.

For vertex writes of at most 16 MiB, an available global scratch buffer supplies
the returned address. This is its **start**, regardless of the destination
offset. Unmap clears its occupied flag, binds the original buffer, optionally
orphans storage and uploads scratch bytes at the recorded destination offset.
Another map while scratch is occupied instead maps GL storage directly. Read
maps always use GL. Direct mappings return the GL pointer plus the byte offset,
including the original null-pointer-plus-offset behavior on mapping failure.
Callers must inspect GL errors and obey object, context and mapping lifetimes.

## Evidence and Linux check

The [Windows differential test](../scripts/check_gl_buffers.py) runs all nine
unchanged original bodies against owned recording GL callbacks and an isolated
native CRT heap: **1,024 paired vertex/index lifetimes**, randomized flags,
read/write/discard modes, scratch contention, the 16 MiB boundary, failed GL
mapping and counter wrapping. Full map/unmap/destruction records and ordered GL
calls match. Constructor allocator spare bytes are excluded. The deleting free
flag and allocator exceptions are not native differential coverage.
See the [source-pinned report](../analysis/gl-buffers-validation.json).

`FS15GLBuffersCheck` is an asset-free transport test in the normal CTest suite.
`FS15GLDriverCheck` uses an authored SDL context and actual GL symbols, checking
initial data, staging/subdata, direct/discard maps, byte readback, resource
deletion, counters and GL errors. It does not draw a game scene. Linux CI runs
this executable on Mesa's software renderer under Xvfb, separately from the
asset-free core tests. This is real OpenGL API execution, not hardware GPU
performance or original-resource desktop validation.

[Linux run 39152321706](https://github.com/nat649/fs15-reconstruction/actions/runs/39152321706)
passes all **32** component/transport tests on Linux x64 and ARM64 Linux under
QEMU, plus the actual GL check on **llvmpipe (LLVM 20.1.2, 256 bits)**. The same
32 component tests pass locally on Windows x64. The source-pinned report records
the tested commit, runtime archive and four installed executable hashes.

```sh
cmake -S . -B build/linux-gl -DFS15_PORTABLE_CORE_ONLY=ON \
  -DFS15_BUILD_GL_CHECK=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build/linux-gl --parallel 2
ctest --test-dir build/linux-gl --output-on-failure
# On a Linux desktop:
./build/linux-gl/src/core/FS15GLDriverCheck
# For headless Mesa validation (requires Xvfb and xauth):
LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a ./build/linux-gl/src/core/FS15GLDriverCheck
```

Install SDL2 development headers, OpenGL headers and a working GL driver
(`libsdl2-dev libgl-dev libgl1-mesa-dri` on the CI distribution). The optional
driver-check executable is installed in `bin` and needs SDL2 and desktop GL.
The component-only checks have no SDL or GL dependency, including ARM64 QEMU.

## Remaining renderer and physics work

These buffer paths are not the complete graphics driver. Shader compilation,
material/texture state, vertex layouts, draw submission, render targets and
connection to the recovered scene loader still need recovery and integration.
The headless host continues to report `renderer_started=false` and
`playable=false`. Native physics factory/scene construction, collision shapes,
terrain contacts and the complete body/constraint solver are also unfinished.
