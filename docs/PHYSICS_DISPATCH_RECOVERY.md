# Recovered physics dispatch and worker loop

Three more bounded GE6 paths now compile as platform-independent C++17:

| Original entry | Recovered behavior |
|---|---|
| `0x1400c7a60` | Active/busy/positive-delta gate, elapsed and capped clocks, milliseconds-to-seconds conversion, synchronous call or worker wake |
| `0x1400c3c50` | Scene simulation call, backend/world lookup, observation collection boundary |
| `0x1400c3b90` | Request wait, stop check, simulation/collection, pending-time clear, completion signal and next wait |

Sources: [core implementation](../src/core/physics_dispatch.cpp),
[native record/service interface](../src/core/physics_dispatch.h),
[authored event/thread transport](../src/platform/physics_worker.cpp).
These are runtime scheduling paths, not a reconstructed physics solver.

## Native ordering and record fields

The manager's supported fields retain their original offsets: scene at `+0x38`,
active flag at `+0xa8`, fixed-step value at `+0xb0`, unsigned iteration count at
`+0xb4`, two clocks at `+0xc0/+0xc8`, busy flag at `+0x448`, request/completion
events at `+0x478/+0x480`, pending seconds at `+0x488` and asynchronous flag at
`+0x48c`. Opaque manager fields remain unchanged.

An eligible dispatch adds the full delta to the elapsed clock. It adds the
smaller of the delta and `float(iterations) * fixed_step * 1000` to the capped
clock, marks busy, and multiplies the delta by the original binary32 `0.001`
constant. Asynchronous dispatch stores the converted value before signaling
the request event. Synchronous dispatch invokes the backend immediately.
Nonzero busy, inactive state or nonpositive delta leaves the record unchanged.

The worker first waits, then checks the stop byte. Each accepted wake calls
the scene's simulation slot with seconds, unsigned iterations and fixed-step
value. It then resolves the backend world and runs the observation collection
boundary. Only after collection does it clear pending seconds and signal
completion. Stop wakes do not simulate or signal completion through the native
loop. Event waits use the observed infinite timeout and non-alertable mode.

## Real event/thread transport

The portable transport owns a `std::thread` and two mutex/condition-variable
events. Their retained signal bit supplies auto-reset semantics: a signal sent
before a wait is retained, one successful wait consumes it, and repeated signals
coalesce. Calls are serialized with one outstanding request. Stop is published
through the request event and the thread is joined before backend data dies.
The idle request wait remains unbounded. A submitting caller has an authored
10-second completion watchdog; no original watchdog behavior is claimed.

The transport accepts caller-provided simulation/world/collection callbacks.
It does not create a native physics scene. Completion now runs the recovered
[post-step/contact/wheel paths](PHYSICS_POST_STEP_RECOVERY.md), which replace
the earlier authored busy-flag acknowledgement. Full manager/scene creation,
controller movement, successful native deletion and the solver remain open.

`FS15PortableHost --probe` now runs **16 dispatched steps** on a distinct worker
thread. Its backend evaluates the recovered wheel-registry/tire-force adapter
using authored fixture records, verifies result tags, and collects completion.
The JSON reports this CPU path separately from `physics_world_started=false`,
`native_engine_started=false`, `renderer_started=false` and `playable=false`.
There is no gravity/contact/body simulation substituted behind those flags.

## Validation

[The native comparison](../scripts/check_physics_dispatch.py) executes the three
unchanged original bodies in the pinned local Windows image. Complete manager
and guard bytes plus ordered scene/world/event snapshots match:

- **3,072 dispatch states**: 2,660 ignored gate states, 287 asynchronous signals
  and 125 synchronous steps;
- **1,024 standalone synchronous states**, including unsigned iteration counts
  above `INT32_MAX` and the original floating-point conversion order;
- **512 complete worker lifetimes**, including stop before the first step and
  **2,044 completed steps** before stop.

The original observation collector runs only its empty-list path in these
fixtures. Its nonempty contact processing is covered separately by the
[post-step/contact comparison](PHYSICS_POST_STEP_RECOVERY.md). The backend solver and OS waits/signals are explicit seams;
original machine code, pseudocode and raw records stay local.

[Portable component fixtures](../tests/physics_dispatch.cpp) check native gate,
clock and callback order plus authored short-record, nonfinite-input and service
failure guards. Unlike the original code's ignored OS return values, authored
services stop on failure and retain preceding native mutations.
[Thread transport fixtures](../tests/physics_worker.cpp) check 24 real thread
lifetimes, completion counts, zero-step stops, repeated stops, rejected ticks
after shutdown, and backend failure without false completion success.
See the [source-pinned aggregate evidence](../analysis/physics-dispatch-validation.json).

Graphics-driver creation, full physics scene creation, collision detection,
constraint solving, rigid-body integration, controller/deletion services
and complete game startup remain unfinished.

All **30 component/transport CTests** pass on Windows x64, Linux x64 and
ARM64 Linux under QEMU. The installed Linux host passes six integration
checks including the 16-step worker probe, alongside six script-host and
ten frontend/tester checks. [CI evidence](https://github.com/nat649/fs15-reconstruction/actions/runs/39140990548).
These fixtures contain no original resources and do not validate a full
physics world, graphics driver or complete game startup.
