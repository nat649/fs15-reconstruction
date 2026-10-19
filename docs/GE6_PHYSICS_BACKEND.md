# GE6 physics world and solver recovery

This work targets GE6. It does not port or depend on ReFarm.

The pinned GE6 image contains explicit RTTI for `btCollisionDispatcher`,
`btDefaultCollisionConfiguration`, `btDiscreteDynamicsWorld`,
`btSequentialImpulseConstraintSolver`, `Bt2World`, `Bt2Solver` and
`Bt2RaycastVehicle`. Its world factory at `0x14027fb30` constructs the Bullet
configuration, dispatcher, broadphase, solver and GE6 world subclasses.
Earlier documents' references to a hypothetical PhysX backend are superseded
by this direct identification of Bullet in the inspected runtime.

## Reused middleware and recovered GIANTS code

The optional `FS15_BUILD_GE6_PHYSICS` target fetches upstream Bullet at commit
[`0e1a77047cfaa9bf94f7318686ba4179f40c8c1b`](https://github.com/bulletphysics/bullet3/tree/0e1a77047cfaa9bf94f7318686ba4179f40c8c1b),
with an archive SHA-256 check. It builds only LinearMath, BulletCollision and
BulletDynamics. Upstream code stays in the ignored build dependency directory;
its original license is installed with the package. It is reused middleware,
not newly decompiled GIANTS source. No original game binary is linked into the
portable target.

Neither released Bullet 2.82 nor released 2.83 matches all inspected structures.
The selected 2014 revision retains the user-pointer/user-index union, adds the
ignored-collision-object array and contains the row-solver function pointers.
Nine Windows x64 layout sizes match the inspected native paths:

| Object | Bytes |
|---|---:|
| Base discrete world | 464 |
| Base sequential impulse solver | 336 |
| DBVT broadphase | 288 |
| Collision dispatcher | 10,496 |
| Base collision configuration | 184 |
| Rigid body | 736 |
| Persistent manifold | 896 |
| Manifold point | 192 |
| Broadphase proxy | 64 |

This identifies a compatible tested upstream candidate, **not the exact full
GIANTS middleware fork**. Linux and ARM64 code use typed middleware APIs;
Windows object offsets must not be assumed portable.

[`src/ge6/physics_world.cpp`](../src/ge6/physics_world.cpp) recovers GE6's
collision dispatcher and near-pair rules, island merging, ordered vehicle
collision/solver hooks and setup/velocity/penetration/finish phases. In
particular, ordinary sleeping pairs are removed; stock Bullet's callback would
retain their manifolds. Trigger pairs follow a separate retention rule.
The [recovered vehicle sort](../src/core/physics_vehicle_sort.cpp) preserves
native equal-key swap order, rather than replacing it with `std::sort`.
RAII ownership and factory orchestration are authored. The stock broadphase
is currently serialized by its caller; the original broadphase's lock methods
and custom heightfield creation functions are not yet supplied.

## Native comparison

The [local differential test](../scripts/check_ge6_physics_world.py) executes
unchanged original GE6 factory, sphere, body setup, world/solver stepping and
destruction bodies in an isolated process. It supplies a private CRT heap,
aligned allocator and timing imports without calling the executable entry
point, Steam or an installed game process. The original factory's externally
supplied pair-cache allocation is explicitly reclaimed by the fixture owner.

The optional native math mapping now includes the auxiliary `data` section as
well as `text`: the original `powf` needs those masks/constants. A startup
assertion checks `powf(1, 1/60) == 1` before comparing trajectories. Mapping code
without its constant section had incorrectly erased damping velocities in an
early investigation; those results are excluded from the validation report.

The [source-pinned results](../analysis/ge6-physics-world-validation.json) record:

- 64 sphere-drop lifetimes and **15,360 trajectory frames**, including
  **6,963 frames with generated contacts**;
- maximum scaled position/velocity error **0.0000014157** against original GE6;
- 96 nonempty vehicle-hook fixtures and **26,984 ordered callback events**;
- 1,024 native vehicle-sort fixtures, including duplicate island keys and
  complete key-query order;
- complete native aligned-allocation cleanup after the explicit fixture-owned
  pair-cache teardown.

Vehicle callback fixtures verify the engine's routing and ordering. Their
objects do not implement the actual `Bt2RaycastVehicle` contact/wheel solver.
The portable CTest additionally requires accumulated gravity, collision response,
stable resting height and connected vehicle hook phases.

## Recovered wheel ray and finish paths

[`vehicle_wheel.cpp`](../src/ge6/vehicle_wheel.cpp) ports five further bounded
bodies: `0x140150a10` suspension frame, `0x140151930` wheel ray filter,
`0x1400ba820` closest-hit callback, `0x140153330` suspension ray and
`0x140155140` solved-wheel finish/readback. The public typed structures are
authored transport, not Windows Bullet object layouts.

The GE6 filter uses an OR of corresponding group/mask bits, rejects the owning
body, triggers and objects without the rigid-body internal type, and honors
the owner's ignored pairs. Replacing this with stock Bullet's cross-field AND
changes observed behavior. Suspension distances retain native compression and
droop clamps. Wheel finish preserves the native single angle wrap (not modulo),
sleep/inactive guards, solved normal-impulse force and slip/motor readback.

The [local comparison](../scripts/check_ge6_vehicle_wheel.py) and
[source-pinned result](../analysis/ge6-vehicle-wheel-validation.json) cover
768 frames, 768 miss/world-normal/local-normal ray fixtures, 4,096 filters and
1,024 finish states: **6,656 fixtures with zero observed numeric difference**.
The ray fixtures supply controlled results through an authored world callback;
unchanged original GE6 ray/callback bodies execute. A separate portable CTest
uses actual Bullet world ray queries to validate ground hits, owner/trigger/
non-rigid rejection, ignored pairs, compression limits and missing contacts.
These functions are not registered as a complete vehicle solver. The bounded
constraint helpers, penetration and velocity phases below extend them; full
setup orchestration and the drivetrain network remain open.

The world/solver checkpoint passes all 34 CTests on Windows x64, Linux x64 and
ARM64 Linux under QEMU ([CI](https://github.com/nat649/fs15-reconstruction/actions/runs/39155932339)).
The ray/finish extensions also pass Linux/ARM CI in
[run 39156379940](https://github.com/nat649/fs15-reconstruction/actions/runs/39156379940).

## Suspension constraint setup and penetration correction

[`vehicle_constraint.cpp`](../src/ge6/vehicle_constraint.cpp) recovers three
further bodies: the Jacobian helper `0x1401511a0`, spring setup `0x140150f50`
and complete wheel split-penetration loop `0x140154d70`. Its typed structures
are authored transport, not raw casts of the native Windows records. Query
and application directions remain distinct. Body inertia/angular factors,
solver velocities/external force impulses, asymmetric damping, compression
limits and solver ERP/split thresholds follow the original paths.

Instruction inspection corrected an ambiguous decompiler expression: the
native setup sums both body terms before adding spring softness. Preserving
that association matters near cancelling terms. Native wheel setup also uses
the geometric suspension length before its separate readback clamp; passing
the clamped length would suppress bottom-out correction.

The [local differential harness](../scripts/check_ge6_vehicle_constraint.py)
and [source-pinned report](../analysis/ge6-vehicle-constraint-validation.json)
cover 2,048 Jacobians, 2,048 spring states and 1,024 multi-wheel penetration
states over **6,640 successive iterations**. They compare complete records and
solver bodies, including inactive/sleeping guards, static solver bodies,
misses, accumulated-impulse clamps, shared bodies and aliased A/B bodies.
Maximum scaled numeric error is **0.0000005114** in this domain.

The additional `ge6_suspension_penetration_integration` CTest connects actual
Bullet world rays, recovered spring setup and the recovered penetration phase
through GE6's solver hooks to middleware transform writeback. With explicit
fixture ERP2 `0.2` and split threshold `0`, a chassis starts at height `0.5`,
reaches `0.56` after one step and `0.799629` after 30 steps, approaching the
`0.8` compression limit without introducing kinetic velocity or chassis
collision contacts. This is an authored one-wheel **phase isolation fixture**:
its motor and velocity phases deliberately do nothing. It is not registered
as a production vehicle and does not demonstrate drivable gameplay. All 35
component tests pass on Windows x64, Linux x64 and ARM64 Linux under QEMU;
[CI run 39220107155](https://github.com/nat649/fs15-reconstruction/actions/runs/39220107155)
validates source commit `5a880aa`.

## Wheel velocity, friction and braking

[`vehicle_velocity.cpp`](../src/ge6/vehicle_velocity.cpp) recovers the bounded
`0x1401540b0` phase over the previously recovered tire-force leaf. It applies
normal and correction velocity impulses, computes wheel load using the native
gravity queries, limits longitudinal/lateral tire impulses and couples brake
impulses to wheel spin and chassis velocity. Native impulse clamp order, up to
ten brake/friction inner iterations, body sharing, static-body guards and
sleep/inactive/contact guards are retained.

The [local comparison](../scripts/check_ge6_vehicle_velocity.py) and
[source-pinned report](../analysis/ge6-vehicle-velocity-validation.json) cover
**1,024 vehicle states / 6,640 successive iterations**, including **18,448
original gravity queries** and 584 states configured with brakes. Complete
wheel, constraint and solver-body records are compared. Maximum scaled numeric
error is **0.0000003242** in the tested finite, positive tire-parameter domain.

The original drivetrain helper `0x140155d50` executes with zero differential
nodes in these native fixtures. Recompiled code supplies an explicit no-op
driver callback and typed gravity transport. This does **not** recover the
motor/differential network or demonstrate power from a recovered engine.

The additional `ge6_wheel_velocity_integration` CTest connects actual world
rays, recovered Jacobian/spring setup, penetration, velocity/friction/brake
and wheel readback through the GE6 solver hooks. A one-wheel fixture starts
with wheel spin `20 rad/s`: the chassis reaches `8 m/s` while suspension keeps
it near height `1.49` under gravity. Applying the wheel brake brings chassis
speed and wheel spin to zero. Chassis-ground collision contacts do not supply
support or traction. Fixed slip inputs, simple geometry and setup orchestration
are authored fixture data. This is component integration, not an original
tractor, a recovered motor or a playable game. The new total is 36 component
tests; Windows passes locally and Linux/ARM validation follows in CI.

## Build

```sh
cmake -S . -B build/ge6-physics -DFS15_PORTABLE_CORE_ONLY=ON \
  -DFS15_BUILD_GE6_PHYSICS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build/ge6-physics --parallel 2
ctest --test-dir build/ge6-physics --output-on-failure
```

The first configure downloads the pinned public middleware. This target needs
no original game resources, GUI, Lua or ReFarm. On Windows, the local differential
script also needs the original pinned analysis image and the separately built
`FS15GE6PhysicsProbe` DLL. Never publish that image or recovered source exports.

## Remaining path to a playable GE6 Linux runtime

The next steps in **`Bt2RaycastVehicle`** are full setup/steering orchestration,
motor/differential recovery and integration of the recovered ray/setup/
penetration/velocity/finish methods into that complete class. Original GE6 terrain shapes
and convex/heightfield algorithms, body/joint/scene loading, the remaining script
services, graphics shaders/materials/draw submission and the full startup loop
remain to be linked and tested. A working physics component is not a playable
game. No playable GE6 Linux session is claimed by this milestone.
