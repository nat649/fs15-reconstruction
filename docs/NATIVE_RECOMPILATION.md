# Native recompilation work and Switch target

The requested future console target is **Nintendo Switch homebrew (.nro)**.
Console work is deferred while native recovery and the Windows runtime advance.
The current work
recovers GE6 components from native analysis and prepares them as portable C++.
There is no complete recovered GE6 executable or verified Switch build yet.
The existing Windows native mode still executes the installed original engine.

## What is actually recompilable

`FS15PortableCore` builds these bounded recovered components without Windows,
Direct3D, LuaJIT, original executable bytes or original game resources:

| Component | Native coverage | Validation |
|---|---|---|
| RLE8 decoder | `0x14028f430` | 266 comparisons / 853,086 values |
| RLE16 decoder | `0x14028f770` | 107 comparisons / 35,133,356 values |
| Tire-force math | `0x1400c24c0` | 3,300 comparisons / 13,200 float results |
| Wheel registry lookup | `0x1400c9e50` | 1,440 successful native lookups, including signed IDs, duplicate IDs and unsigned tree keys |
| Seven wheel-read adapters | Axle speed, contact object/point/normal/force, position and slip | 11,760 complete script-frame comparisons |
| Seven 32-bit scripting operations | AND, OR, NOT, XOR, masked left/right shift and highest set bit | 21,763 complete script-frame comparisons |
| Affine point transform | `0x1400bfec0`, also used by wheel force-point updates | 12,000 bit-exact four-float results; integrated into host Lua point transforms |
| Affine multiplication/inversion, rigid inverse point, quaternion conversion, scale extraction | Six native math entries | 36,000 complete bit-exact output comparisons, including singular failures and partial writes |
| Cached hierarchical world transforms | `0x1400b1a50` | 1,200 complete node-arena comparisons across dirty/cached parent chains |
| Four Lua local/world point/direction adapters | `0x1400e5330`, `0x1400e5410`, `0x1400e54e0`, `0x1400e55f0` | 4,799 complete script frames and their 4,799 node arenas; integrated into ReFarm through an authored world-matrix bridge |
| Scene state, observer tracking, inherited flag and base/TRS matrix setters | Five native entries; two constructor-observed vtables | 12,000 complete node arenas and 7,200 side-effect-free notification traces; includes 185 world-preserving child matrix changes |
| TRS math/cache/rebuild and six Lua getters/setters | Eleven native entries | 18,000 math results, 7,200 complete node arenas and notification traces; all recorded Windows floats bit-identical, with explicit platform-libm boundary |

Those are **46 native entries with bounded reconstructed paths**. They are not
46 completely proven engine functions or a percentage of GE6 completion.
Unknown record bytes remain explicit in the recovered structures. Rendering,
suspension, contacts and a complete rigid-body solver are not supplied by this
library. The wheel/tire helpers are not yet connected to playable vehicle physics.
The point-transform leaf runs in the experimental host's Lua transform service
through an authored matrix-layout adapter. GRLE8/GRLE16 loading uses recovered
decoders with authored strict file-envelope/run validation. The transform oracle
supplies a 16-byte aligned buffer for the original MOVDQA store; the reconstructed
API also accepts unaligned caller buffers.

Wheel lookup comparisons use valid synthetic native registries. Missing-record
diagnostics reach CRT imports that the isolated reference does not link, and
the original position getter exposes uninitialized data on a failed lookup.
Those native cases are excluded. The authored API checks malformed/truncated
inputs and rejects undefined position results; separate component tests cover
these added guards. This does not prove safety for arbitrary native pointers.

## Portable build

```powershell
cmake -S . -B build/portable-core -DFS15_PORTABLE_CORE_ONLY=ON
cmake --build build/portable-core --config Release
ctest --test-dir build/portable-core -C Release --output-on-failure
```

The C API is in `src/core/api.h`. Windows export attributes are isolated in one
header and disappear for the static portable build. The wheel object views
currently require 64-bit pointers and IEEE binary32 floats. RLE16 writes its
little-endian output explicitly; bit shifts use unsigned wrapping and masked
counts instead of depending on x64 instruction behavior. Host compilation and
tests are distinct from cross-compilation and execution on the Switch.

The CI workflow builds/tests this target on Linux without original resources.
The Windows DLL remains available for original-code differential comparisons.
No Switch toolchain or packaging target is configured yet. Homebrew `.nro` is
the selected future route; no console SDK contents are included.

The workflow also defines an **ARM64 Linux cross-build with QEMU user-mode
execution**. All four asset-free core tests pass there, as well as on Windows x64
and Linux x64; see [platform evidence](../analysis/portable-core-platform-validation.json).
On a Linux host with `g++-aarch64-linux-gnu` and `qemu-user` installed:

```sh
cmake -S . -B build/arm64-core -DFS15_PORTABLE_CORE_ONLY=ON -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=cmake/aarch64-linux-gnu.cmake
cmake --build build/arm64-core --parallel 2
ctest --test-dir build/arm64-core --output-on-failure
```

This checks a different CPU architecture with authored asset-free invariants.
It is not a libnx build, `.nro`, console execution or a second architecture's
original-code differential comparison. See [portability status](PORTABILITY_STATUS.md).

## Recovered scene component boundaries

`scene_math.h` documents the original affine lane order and **WXYZ** quaternion
order. Quaternion-to-matrix preserves translation and padding; scale extraction
retains the native near-unit snapping interval. Matrix inversion retains its
`1e-36f` determinant threshold. Instruction inspection corrected a pseudocode
addition-order ambiguity in the quaternion trace before validation passed.

`scene_graph.h` exposes a caller-owned native-layout node arena: parent pointer
at `+0x28`, flags at `+0x50`, local matrix at `+0x98` and cached world at `+0xd8`.
Only traversed dirty nodes update, clearing bit `0x40`. Parent pointers must
resolve to records in that arena. Cycle, truncated buffer and nonfinite guards
are authored additions; no original object allocator, dirty propagation, scene
loader, vtable callbacks or ownership lifecycle is reconstructed by this API.

The four Lua adapters retain numeric tag/value locations and unrelated frame
bytes. The original inverse adapters ignore an inversion failure and can read
uninitialized stack data; one singular generated chain was excluded rather than
executing that undefined path. The bounded API rejects it and preserves the frame.

ReFarm supplies its existing world matrices through a cached synthetic-node
bridge. This exercises recovered coordinate conversions without claiming native
scene lifecycle integration. Both original-map gameplay fixtures still match
source/bytecode, frame partitions and pause/resume in 24 host runs. See
[component evidence](../analysis/scene-core-validation.json).

The additional `scene_state.h` component recovers the base/TRS state dispatcher,
observer-presence propagation and the two matrix-setter variants. It supplies a
partial `0x170`-byte node layout, bounded pointer views and ordered notification
output. It does not execute arbitrary original vtables/callbacks or allocate
engine objects. Its staged failure guards and maximum descendant depth are
authored. See [state recovery scope and remaining lifecycle work](SCENE_STATE_RECOVERY.md).

The [TRS update component](SCENE_TRS_RECOVERY.md) recovers lazy cache refresh,
matrix rebuilding, the original origin-zero propagation call, and six complete
Lua setter/getter frames. Its matrix/Euler flow is recovered; trig and square-root
functions use platform libm, with a recorded numeric gate. ReFarm now uses these
recovered composition/affine routines for I3D node transforms and vehicle poses.

## Discovery and recovery ledger

Auditing the 32 scripting references previously emitted as code labels found
**30 distinct uncovered executable entries**. All 30 were created as functions
without splitting existing function bodies, and then decompiled. The enlarged
catalogue now contains **12,200 entries, 11,635 eligible pseudocode exports and
565 excluded thunks/external functions**. All 498 observed native scripting
descriptors now resolve to catalogue function entries. This audit is limited
to those explicit references and a subsequent 16-slot scene-vtable audit that
added four functions. Wider code discovery remains open.

```powershell
scripts/analyze.ps1 -Target game-runtime -Existing -RecoverBindings -ExportName binding-discovery -JavaHome 'C:\path\to\jdk-21'
scripts/analyze.ps1 -Target game-runtime -Existing -FullExport -ExportName game-full -FunctionTimeoutSeconds 300 -JavaHome 'C:\path\to\jdk-21'
python scripts/index_native_exports.py
python scripts/index_recompilation.py
scripts/analyze.ps1 -Target game-runtime -Existing -Dependencies -ExportName native-dependencies -JavaHome 'C:\path\to\jdk-21'
python scripts/index_portability.py
```

The recompilation index checks source hashes against successful component
reports. Its per-function ledger stays local in `analysis/native-recompilation.json`.
The [public aggregate](../analysis/native-recompilation-summary.json) distinguishes
bounded validated components from pseudocode-only entries. It explicitly keeps
`full_native_rebuild_verified=false` and `switch_build_verified=false`.

## Remaining work for a complete port

- Continue function discovery, recover concrete types, ownership and object lifecycle.
- Reconstruct subsystem initialization and a complete linkable native call graph.
- Recover native rendering/material/resource services and introduce the chosen console platform interface.
- Recover physics world, shapes, joints, suspension and complete drivetrain behavior, respecting middleware boundaries.
- Rebuild complete scripting, input, audio, filesystem, threading, save and networking interfaces.
- Choose/configure the Switch toolchain, then validate compiler/ABI differences, console startup and full farming sessions.

Recovered executable code, pseudocode exports and game resources remain local.
The repository contains authored reconstruction/tooling and aggregate evidence.
