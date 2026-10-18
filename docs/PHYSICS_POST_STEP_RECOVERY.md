# Physics results connected to scene/view preparation

The portable CPU host now connects recovered dispatch and a real worker thread
to recovered contact collection, wheel readback, contact events, scene TRS,
Camera/view preparation, visibility and shader-block packing. This is an
integrated fixture path. It does not create the full native physics scene,
solve bodies/collisions, submit GPU drawing or start a playable world.

Six additional bounded native entries compile in C++17:

| Entry | Recovered behavior |
|---|---|
| `0x1400ca150` | Wheel speed, contact identity/point/normal/force, actor-frame position, angle and slip readback |
| `0x1400ca080` | In-order wheel registry traversal and per-set updates |
| `0x140144ce0` | Backend pair/group lookup, contact masks and scratch-list reset |
| `0x140145190` | Suppression, stay/indexed-part policy, listener order and handled-event short circuit |
| `0x140144ef0` | Pair hash lookup, enter/leave/stay masks, swap-last removal, previous-list growth/retention |
| `0x1400c7b20` | Completion wait, object finalizers, wheels, contact transitions, pending removal/callback/cancellation and busy clear |

Sources: [implementation](../src/core/physics_post.cpp),
[records/services](../src/core/physics_post.h),
[native comparison](../scripts/check_physics_post.py),
[portable fixtures](../tests/physics_post.cpp).

## Ordering and ownership

Asynchronous finalization waits for worker completion before reading results.
It finalizes registered objects in native tree order, updates wheels, delivers
contact transitions, then removes inactive pending items using the last element
to fill each gap. It clears busy last. The native synchronous branch clears
busy before callbacks. The thread transport now calls this recovered routine
instead of clearing busy itself. Failed finalization cannot count as a
completed frame or be silently retried against partial state.

The pending callback's third argument is a **float in XMM2**, confirmed in the
original instructions. Owner IDs are reloaded after callbacks, and pending
count/storage are reloaded after cancellation because it can compact or replace
the queue. The decompiler's untyped 32-bit value does not establish that ABI.

Wheel updates preserve source fields and opaque bytes. Point/normal copies,
contact flag checks and the translated frame-basis product retain native order.
Position's fourth lane is zero. Actor-frame lookup is an explicit service;
`atan` uses an optional original math boundary or platform libm. This does not
generate wheel load, slip or contact response.

The contact collector consumes backend groups. Indexed contacts select the
proper endpoint column and wrap byte selectors through their low five bits.
Nonindexed contacts test whether groups contain contacts. The reusable group
count resets while its pointer/capacity remain. Contact finalization matches
ordered endpoint IDs through native hash chains, emits entered/departed/retained
bits in order, and preserves suppression, part IDs and listener short circuiting.
History growth uses native capacity and 16-byte alignment. The host tracks its
allocations and releases them after joining; caller-supplied buffers retain
their ownership contract.

Bounds, finite-input checks, cycle limits and service/allocation failure handling
are authored guards. Errors can retain preceding callbacks and mutations; no
transaction across services is promised. All traversed pointer graphs, part
arrays and group row spans must be valid caller-owned storage.

## Connected CPU probe

`FS15PortableHost --probe` performs 16 real worker-thread steps with recovered
force queries and contact collection through an authored backend fixture. It
then runs native post-step order on a nonempty wheel registry/contact history,
delivers 16 enter/stay/leave events, binds wheel positions to recovered scene TRS,
and prepares Camera matrices, frustum, shader block and visibility queries.
Including the initial state, it checks **17 scene/view/visibility/upload frames**
before joining and releasing resources.

The binding and backend coordinates/groups are authored test inputs, not a
recovered vehicle specialization or a substitute solver. Wheel, scene, Camera,
packed shader positions and visibility must agree. JSON retains
`native_engine_started=false`, `physics_world_started=false`,
`renderer_started=false` and `playable=false`.

## Evidence

Unchanged original Windows code passes:

- 1,536 wheel readback states, split between original and platform `atan`;
- 128 varied nonempty wheel registry runs with one to three wheels;
- 512 contact collector states, including 455 nonempty pair lists and 3,577
  ordered backend lookup/group calls;
- 1,536 notification-policy states and 768 contact-history lifetimes;
- 256 full post-step manager states with contact history and pending callbacks,
  including cancellation against an empty native registry.

Native post-step fixtures use empty object/wheel registries; separate native
wheel-registry comparisons and connected portable fixtures cover the nonempty
wheel path. Successful native deletion and controller movement are not proved.
Portable tests cover cancellation-service routing/queue mutation, allocation
failure, history allocation cleanup, malformed graphs and failed finalization.
Original code, raw records and game resources remain local. See
[source-pinned evidence](../analysis/physics-post-validation.json).

The actual backend scene/dispatcher constructor, collision shapes, terrain
contact generation, body/constraint solver, controller finalizers and successful
native deletion remain required for gameplay. Full game Lua/scene loading and
the actual Linux graphics driver/material/draw consumers also remain unfinished.
Connecting recovered CPU paths cannot supply those missing implementations.

All **31 component/transport CTests** pass on Windows x64, Linux x64 and
ARM64 Linux under QEMU. Six installed-host checks verify the connected probe,
alongside six script-host and ten frontend/tester checks using authored
resources. [CI evidence](https://github.com/nat649/fs15-reconstruction/actions/runs/39148462680).
Original-resource Linux desktop/gameplay validation remains pending.
