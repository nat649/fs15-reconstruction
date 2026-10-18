# Native startup: render views and wheel-force calls

Three further GE6 entries now compile in the portable C++17 core:

| Original entry | Recovered behavior |
|---|---|
| `0x14012ca00` | Render-view initialization and two ordered driver buffer requests |
| `0x14012cc40` | Ordered buffer destruction and base-vtable transition |
| `0x1400e76c0` | Wheel registry lookup, load gate, angle tangent, tire-force calculation and two script results |

The authored [headless host](../src/portable/main.cpp) now executes these paths
alongside recovered Camera creation, world/cache updates, projection, shader
packing, visibility and cleanup. Its JSON reports view creation/destruction,
two allocated/released CPU buffers and the registry-to-force adapter. It still
reports `native_engine_started=false`, `renderer_started=false`,
`physics_world_started=false` and `playable=false`.

## Render-view initialization

The [lifetime implementation](../src/core/scene_view_lifecycle.cpp) recovers
the inspected native layout and initialization boundaries. Three matrices
start as identity; the combined matrix and opaque spare bytes remain untouched.
Six initial planes are `(0, 1, 0, 0)`. They become a usable camera volume only
after the existing recovered view preparation runs. Context dimensions at
`+0x78/+0x7c` are copied into the view, and the settings pointer retains that
context's identity and lifetime.

Native driver slot `+0x220` receives two requests: **196 bytes**, then
**176 bytes**, both with null initial data and flag `8`. The descriptor's
uninitialized padding is excluded from the authored service interface.
Even a null first result does not suppress the second request. Destruction
releases each nonnull buffer in order, using deleting flag `1`, then changes
the view's vtable. Native stale token fields remain untouched; callers must
destroy a constructed view exactly once.

The portable host owns CPU storage for both requests. This exercises native
resource lifetime and shader packing, but does not implement a graphics driver
or submit triangles. Other view variants and the secondary block's producers
and consumers remain open.

## Wheel-force integration

The [force adapter](../src/core/wheel_forces.cpp) connects the recovered
wheel-ID lookup with the recovered tire-force calculation. Inputs come from
the native script frame: shape ID, longitudinal input, lateral angle and load.
For a resolved wheel and load above `1e-5`, the native adapter takes the
angle's tangent, evaluates the wheel parameters, and writes longitudinal and
lateral force into two tag-`3` script return slots. All spare frame bytes
remain unchanged. Parameter names and units are still inferred.

The singleton accessor and missing-wheel diagnostic stream are replaced by an
explicit caller-supplied registry boundary. This is a force-query path. It
does not generate contact/load/slip values or apply impulses to bodies.
Suspension, collision shapes, terrain collision, rigid-body integration,
thread synchronization and full physics-world creation remain unfinished.

## Validation and next boundaries

The [local native comparison](../scripts/check_startup_view_wheel.py) executes
the three unchanged original Windows bodies against caller-owned fixtures:

- **1,024 complete view lifetimes**, all four combinations of buffer-creation
  failure, complete record/guard comparisons, and snapshots at ordered create
  and destroy callbacks;
- **4,096 complete wheel script frames**, including duplicate IDs, first-match
  selection, threshold loads, near-zero inputs and native result tags;
- 2,048 original-tangent-boundary cases and 2,048 platform-math cases, with
  zero measured scaled force error on the tested Windows build (`2e-5` limit).

Missing-wheel diagnostics reach unlinked CRT formatting in the native fixture;
that original failure branch is excluded. Missing/disabled wheels, invalid
inputs, short records and partial buffer creation are also checked through
[portable fixtures](../tests/startup_view_wheel.cpp). Authored size/finite
guards are separate from recovered behavior. Evidence and source hashes are in
[the aggregate report](../analysis/startup-view-wheel-validation.json).

The dependency audit now exposes distinct view-lifetime, wheel-adapter and
physics-manager-startup roots. The observed physics-manager constructor
`0x1400c29d0` still reaches **76 eligible entries without validated component
coverage**, with **124 computed call sites** in that static closure. Its
Win32 events/thread creation and backend scene/dispatcher setup are concrete
next boundaries. Computed targets are not closed by these counts. A functioning
full native startup and real rendering/physics services remain unvalidated.

All **31 component/transport CTests** pass on Windows x64, Linux x64 and
ARM64 Linux under QEMU. The Linux runs also pass six installed headless-host
checks, six script-host checks and ten SDL frontend/tester checks, using
authored fixtures. Five nonzero/stationary force reference pairs are included
in the portable fixtures. [CI evidence](https://github.com/nat649/fs15-reconstruction/actions/runs/39148462680).
Original-resource Linux desktop validation remains separate and pending.
