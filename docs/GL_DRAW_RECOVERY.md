# GE6 OpenGL draw dispatch

`src/core/gl_draw.cpp` recovers native `0x140065540`, including the bound-layout
gate, five primitive modes, cached point size, non-indexed arrays, indexed
draws and range-index draws. Native instruction inspection establishes that
the index byte offset is multiplied in a 32-bit register and then zero-extended;
negative/large first indices retain that wrapping. Range end calculation also
wraps at 32 bits. Vertex layout/program/context/index binding are caller services.
Null service and invalid primitive guards are authored transport boundaries.

The [local comparison](../scripts/check_gl_draw.py) and
[source-pinned report](../analysis/gl-draw-validation.json) compare complete
driver records and ordered GL calls for **4,096 states / 3,853 calls**. This
includes missing layouts, both index sizes, cache hits/misses/NaN point-size
state, and integer wrapping. Original GL imports are rebound only in the local
isolated reference image; no running game or executable entry point is invoked.

The optional `FS15GLDriverCheck` now creates a triangle through recovered buffer
allocation and sends array/index/range draws through this recovered dispatch.
It reads the center pixel from an actual OpenGL framebuffer and requires blue
for all three paths, then black when the native layout gate suppresses drawing.
The context, fixed-function matrices/attributes and constant color are authored
test transport. They do not implement or replace GE6's scene shaders/materials.
The CPU-only `portable_gl_draw` CTest checks dispatch and wrapping without a GPU.

[CI run 39226238887](https://github.com/nat649/fs15-reconstruction/actions/runs/39226238887)
passes all 38 component tests on Linux x64 and ARM64 Linux under QEMU for source
commit `1d92558`. Its actual-driver step also passes array/index/range primitive
pixel readback on Mesa llvmpipe (LLVM 20.1.2). This is software OpenGL validation;
hardware GPU and original scene rendering remain separate.

On Linux with SDL2/OpenGL installed:

```sh
cmake -S . -B build/gl-check -DFS15_PORTABLE_CORE_ONLY=ON \
  -DFS15_BUILD_GL_CHECK=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build/gl-check --parallel 2
LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a build/gl-check/src/core/FS15GLDriverCheck
```

The [object integration target](GE6_OBJECT_RENDERING.md) now connects this draw
path to original map geometry, hierarchy, diffuse textures and DEM geometry,
with a Windows/NVIDIA framebuffer capture. Original GE6 shader/material state,
terrain layer rendering, desktop gameplay and native engine startup remain
open. Neither target establishes a playable game milestone.
