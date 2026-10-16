# Native recovery and recovered vehicle animation

## Full catalogue export

The full-export workflow attempts every non-external, non-thunk function in the
existing Ghidra catalogue for the pinned FS15 1.4.2.0 runtime image. It keeps a
per-function CSV, individual pseudocode files and resumable progress locally.
Previously completed files are reused; failures remain explicit and can be
retried with a longer per-function timeout.

```powershell
.\scripts\analyze.ps1 -Target game-runtime -Existing -FullExport `
  -ExportName game-full -FunctionTimeoutSeconds 300 -JavaHome 'C:\path\to\jdk-21'
& 'C:\path\to\python.exe' scripts\index_native_exports.py
```

The [aggregate report](../analysis/native-export-summary.json) records the
catalogue, eligible exports, remaining failures and a digest of the verified
export set. The current pass verified **11,635 / 11,635 eligible functions**,
with no remaining export failures in this catalogue. The two large functions
at `0x1400f8420` and `0x1401f59a0` now have verified local exports. Increasing
the timeout exposed a response-buffer limit. The exporter now allows 128 MiB
responses and requests C text without the unused HighFunction syntax tree.
The successful retry used a 300-second per-function limit. Progress-file moves
retry brief Windows sharing conflicts, and the wrapper checks completion even
when Ghidra returns success after a script error.
An explicit binding audit added 30 previously uncovered executable functions
after the earlier 11,601-export pass, bringing the catalogue to 12,196 entries.
The subsequent audit of 16 explicit slots in two scene vtables discovered four
more targets, bringing the current catalogue to **12,200 entries**. All four
have local pseudocode exports; see [scene state recovery](SCENE_STATE_RECOVERY.md).
The remaining 565 catalogue entries are thunks or external functions.
Detailed recovered code stays under `recovered/decompiled/game-full`
and is excluded from Git. A complete pass through this catalogue does not prove
complete function discovery or recovered signatures. Decompiled pseudocode is
not directly compilable C++ and does not establish a rebuilt GE6 engine.

## Engine interface recovery

The export index now recognizes **498 explicit native scripting descriptors**
in three registration functions: `0x140011a30` (111), `0x1400f1580` (205), and
the newly exported `0x1400f8420` (182). The descriptor vtable, +0x60 name slot,
+0x68 callable slot and one of the two observed registration calls must all be
present. Reordered initializers are accepted; stale or unrelated stack slots
are rejected. Code labels remain distinguished from catalogued functions.
The earlier 205 associations remain included. These are additional
observations beside the earlier 112 Lua/native associations. Counts can
overlap and should not be added as a unique API total. The detailed index stays
local in `analysis/engine-bindings.json`. The 32 references originally emitted
as code labels now resolve to 30 distinct newly catalogued functions. General scripting utilities are part
of these descriptors; the count is not a unique total of engine subsystems.

The [native recompilation report](NATIVE_RECOMPILATION.md) distinguishes the
expanded pseudocode catalogue from 99 entries with compiled, bounded recovered
paths. It documents the newly recovered wheel-record layout/read adapters,
32-bit scripting operations and portable-core build. Full native engine
recompilation and Switch execution remain unverified.

The latest scene recovery adds six affine/quaternion math entries, cached world
transforms and four script-frame adapters with bit-exact native comparisons.
The [portability audit](PORTABILITY_STATUS.md) now records known static call
dependencies separately from computed calls, imports and missing concrete types.

Selected wheel paths traced from those descriptors:

| API | Adapter | Next native service | Evidence / remaining work |
|---|---|---|---|
| `createWheelShape` | `0x1400e72e0` | `0x1400c9390` | Packs wheel position/options; checks dynamic-body flags and queues creation |
| `setWheelShapeProps` | `0x1400e7400` | `0x1400c9cf0` | Passes wheel ID, torque, braking and steering to queued/immediate physics updates |
| `getWheelShapeAxleSpeed` | `0x1400e77e0` | `0x1400c9e50` | Looks up a wheel record and reads its speed field; layout remains inferred |
| `getWheelShapeSlip` | `0x1400e7b90` | `0x1400c9e50` | Reads two wheel-record fields; complete PhysX integration remains absent |
| `computeWheelShapeTireForces` | `0x1400e76c0` | `0x1400c24c0` | Calculation reconstructed and compared separately; complete wheel integration pending |
| `addDifferential` | `0x1400e7c40` | `0x1400c9630` | Marshals wheel/differential choices and ratios; not integrated into the host |

These traces guide further reconstruction. The playable host still uses the
documented temporary planar integrator and kinematic implement connections.
It does not execute these original physics functions or reproduce suspension,
traction, articulation and rigid-body contacts yet.

## Reconstructed tire-force component

`src/core/tire_forces.h` reconstructs the calculation at `0x1400c24c0`:
load smoothing, stiffness sensitivity, combined slip response, longitudinal and
lateral force, wheel torque and the aligning quantity. Parameter names/units
are inferred from wheel-record fields. The callable authored API rejects invalid
or non-finite inputs/results instead of exposing unchecked native memory.

`scripts/check_tire_forces.py` compares compiled C++ with the original routine
in **3,300 cases / 13,200 float outputs**. Cases cover zero/tiny slip, both signs,
pure/combined slip, load-ramp boundaries, the cosine transition and deterministic
random parameter combinations. Each output must satisfy
`abs(error) <= 2e-5 * max(1, abs(original))`. See [validation](../analysis/tire-forces-validation.json).

Only the inspected calculation and finite-input math helpers execute in an
isolated test process with synthetic wheel records. The oracle optionally maps
the original auxiliary `text` section for its cosine helper; no PE entry point,
Steam, `.bind`, scene or PhysX initialization is executed. Original code remains
local and is not a runtime dependency of the playable package.

This calculation is compiled into FS15Core but is **not yet used by gameplay**.
Original contact/load/slip data and the force-to-rigid-body bridge must be
recovered before replacing the temporary planar integrator reliably. The joint
and wheel interfaces remain separate outstanding work.

## Recovered animation in the playable build

The gameplay host now executes recovered `AnimatedVehicle.load/update` and
`SpecializationUtil` dispatch instead of its former interpolation shortcut.
The original loader interprets XML animation parts, including chained rotations,
translations, scales, visibility, direction, timing and reversal. Authored
native services expose node transforms and visibility. Initially hidden vehicle
geometry is retained so visibility changes can actually render it. Existing LOD,
non-renderable and trigger exclusions remain explicit.

The host also executes recovered `Trailer.load`, tipping state transitions,
animation callbacks and `Trailer.updateTick`. Doors and trailer bodies animate
before the original discharge start time. Grain transfers use the original
rate, and empty trailers automatically reverse the animation and close.
Typed I3D user attributes preserve numeric, boolean and string values across
the native/Lua boundary.

`tests/check_vehicle_animation.py` runs an authored native-service fixture twice:
once against normalized original bytecode, then against recovered Lua source.
The complete traces match in **777 samples across seven scenarios**: forward
chained parts, double-speed reverse, stop time, forward/reverse seeking,
joint-limit callbacks, looping and tipping/door/discharge/automatic closure.
See [hashes and scope](../analysis/vehicle-animation-validation.json). This
validates those exercised paths in three files that were syntax-only at that
milestone; it does not validate every branch. The later empty-table recovery
fix gives SpecializationUtil a complete bytecode match; AnimatedVehicle and
Trailer still require further validation. The current script-wide classification
is 308 exact deterministic bytecode matches / 162 syntax-only results.

The joint-limit fixture checks callback values against bytecode. It is not
evidence of real component joints or suspension in the playable host. Effects,
audio, procedural grain surfaces and the complete specialization lifecycle
remain unfinished.

`--scenario tip` is a disposable test layout using the original Brantner,
tractor and farm trigger. Hidden runs simulate four seconds, render the animated
trailer and verify grain conservation on both original maps. Scenario placement
is explicit; normal Career placement is unchanged.
