# Native recompilation work and Switch target

The requested future console target is **Nintendo Switch**. The current work
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

Those are **18 native entries with bounded reconstructed paths**. They are not
18 completely proven engine functions or a percentage of GE6 completion.
Unknown record bytes remain explicit in the recovered structures. Rendering,
suspension, contacts and a complete rigid-body solver are not supplied by this
library. The helpers are not yet connected to playable vehicle physics.

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
No Switch toolchain or packaging target is configured yet. Homebrew versus an
official-SDK port remains a project decision; no console SDK contents are included.

## Discovery and recovery ledger

Auditing the 32 scripting references previously emitted as code labels found
**30 distinct uncovered executable entries**. All 30 were created as functions
without splitting existing function bodies, and then decompiled. The enlarged
catalogue now contains **12,196 entries, 11,631 eligible pseudocode exports and
565 excluded thunks/external functions**. All 498 observed native scripting
descriptors now resolve to catalogue function entries. This audit is limited
to those explicit references; wider code discovery remains open.

```powershell
scripts/analyze.ps1 -Target game-runtime -Existing -RecoverBindings -ExportName binding-discovery -JavaHome 'C:\path\to\jdk-21'
scripts/analyze.ps1 -Target game-runtime -Existing -FullExport -ExportName game-full -FunctionTimeoutSeconds 300 -JavaHome 'C:\path\to\jdk-21'
python scripts/index_native_exports.py
python scripts/index_recompilation.py
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
