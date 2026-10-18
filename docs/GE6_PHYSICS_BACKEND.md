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

The next native class is **`Bt2RaycastVehicle`**: ray contacts, wheel constraints,
motor/brake/steering inputs and solved wheel state. Original GE6 terrain shapes
and convex/heightfield algorithms, body/joint/scene loading, the remaining script
services, graphics shaders/materials/draw submission and the full startup loop
remain to be linked and tested. A working physics component is not a playable
game. No playable GE6 Linux session is claimed by this milestone.
