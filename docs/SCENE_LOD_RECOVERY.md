# LOD registration, distance selection and scene dispatch

The portable core now reconstructs the known GE6 LOD node's metadata,
registration and removal, camera-distance cache, child branch tokens, dirty
updates and supported destructor chain. These paths run in mixed owned LOD/TRS
hierarchies. They do not constitute the complete spatial culler or renderer.

## Native paths

| Entry | Reconstructed bounded behavior |
|---|---|
| `0x1400d7a70` | Construct the `0x188`-byte LOD node over the recovered TRS constructor |
| `0x1400d7be0`, `0x1400d7bb0` | Remove pending/registered state and execute the known destructor/deleting chain |
| `0x1400d7620` | Append a `0x28`-byte registry entry, with allocation blocks of 1024 entries |
| `0x1400d7840` | Swap-remove a registry entry and update the moved owner's index |
| `0x1400d78f0` | Update world position/distances and invalidate the cached selection |
| `0x1400d60b0` | Stamp-cached squared-distance level selection |
| `0x1400d7d50` | Base scene propagation followed by LOD dirty marking and pending scheduling |
| `0x1400d7e20` | Set the group index and dispatch packed branch tokens to children |
| `0x1400d7ea0` | Dirty world-position refresh, visible-group registration/move and hidden-group removal |
| `0x1400d7db0` | Direct-child LOD token lookup, or inherited lookup for a null child argument |
| `0x1400b6260` | Inherited lookup through known matrix/TRS/LOD parents |
| `0x140087e50` | Append the LOD pending-update request through recovered pointer-vector allocation |

The LOD constructor explicitly references table `0x1404c1c48`. Auditing its eight
slots found `0x1400d7db0` as executable code without a catalogue function.
Discovery created that function without splitting an existing body. Across the
five inspected scene tables including [Camera](SCENE_CAMERA_RECOVERY.md)
and [Light](SCENE_LIGHT_RECOVERY.md), 40 slots now resolve to 17 distinct targets; five
functions were discovered through those explicit slots. All have local
pseudocode exports. The catalogue now contains 12,201 entries and 11,636 eligible
exports. This bounded audit is not exhaustive engine class discovery.

The discovery script now retains earlier verified creation records when an
audit is extended, rather than losing their provenance when targets already
exist. See [aggregate vtable evidence](../analysis/scene-vtables-summary.json).

## Preserved details

- Fewer than two constructor distances produce `(0,100,100,100)`. Otherwise
  up to four supplied values are copied and the last value fills remaining lanes.
- The initial group index is `0x3fffffff`. A valid child token combines the group
  index with the child's ordinal in the upper two bits. An unregistered group or
  an absent direct child returns `0xffffffff`. Known inherited lookup stops at
  the first LOD parent, retaining that invalid-index behavior.
- Registry rows hold position, four squared distances, a level count, selected
  byte and 16-bit cache stamp. Unused distance lanes stay opaque. Growth copies
  used rows; removal retains the unused trailing row and updates the moved owner.
- The camera epoch is a 32-bit field, but row invalidation compares only its low
  16 bits. Changing a viewer position without changing those bits retains the
  cached selection. Exact distance equality selects the next level.
- The first distance lane is not a transition threshold. Selection starts with
  lane one and compares squared distance against the squared camera factor times
  the stored squared threshold. Floating-point addition order follows inspected
  instructions: `z*z + (x*x + y*y)`.
- LOD dirty scheduling uses the original input mask, even if base propagation
  suppresses every bit. Derived post-processing runs after base traversal, so
  descendant LOD requests precede their ancestor's request. A queued bit prevents
  duplicate scheduling until the update clears it.
- Hiding unregisters the group and directly restores its invalid index. Deleting
  a pending group removes its first pending occurrence by swapping the last
  pointer into that slot, matching the native lifetime path.

## Authored boundaries

The reconstructed C++ retains native record offsets and opaque bytes. Native
vtable addresses are identity tokens and are never executed by portable code.
The context combines observed manager/pending views; its aggregate layout and
initialization are authored rather than a recovered complete global singleton.

`scene_state_lod` and the corresponding TRS/link/unlink variants accept LOD kind
two. The existing APIs retain their matrix/TRS dispatch contract. Their staged
queue output and the owned scatter transport are authored interfaces that retain
native request order; callers append successful requests to the LOD context.
Observer notifications remain side-effect-free outputs. Reentrant observer or
allocator side effects are outside the comparison scope.

World position and inherited child marking use the recovered world-transform
and inherited-state paths through explicit services. A known-subtree marker can
also run while the rest of a forest is being destroyed, without inspecting freed
parent properties. Allocator, registry and supported destructor dispatch services
remain authored platform seams.

Finite/bounds/cycle checks exclude malformed storage and undefined numeric
inputs. The registry limit is 64,512 entries, before native uint16 block-capacity
overflow. Staged node/queue capacity failures preserve live nodes; callback or
allocation failures in the direct native-order functions can retain a mutation
prefix. Their guards do not claim native exception semantics.

## Evidence

[Source-pinned native comparisons](../analysis/scene-lod-validation.json) include:

- 160 complete LOD constructors and 160 destructor/deleting lifetimes;
- 640 index changes with exact child callback arguments and 3,520 token queries;
- 960 standalone derived state changes with complete pending-vector comparison;
- 2,100 registrations, crossing two registry growth boundaries; 1,715 moves and
  2,117 removal calls, including out-of-range native no-ops;
- 12,000 stamp-cache queries, including camera epochs beyond 65,535;
- 36 mixed owned LOD/TRS forest lifetimes and 4,896 successive forest states;
- 520 queued world/registration updates, 78,336 inherited-token queries and
  8,644 distance queries against entries produced by those owned hierarchies;
- 5,073 complete manager allocations, including untouched/spare row bytes;
- recursive destruction with no remaining object registrations or owned buffers.

Original code executes unchanged in an isolated private heap/FLS fixture. No
game entry point or live game is touched. Record and container bytes are compared
after address normalization; accumulated hierarchy matrix regions use the scaled
platform-libm tolerance `1e-5`. The standalone registry/distance results are exact.
Original executable bytes, raw pseudocode and scene resources remain local.

The asset-free `portable_scene_lod` check covers constructor defaults, branch
tokens, exact thresholds, cache reuse/low-bit wrap, visible/hidden registration,
swap-removal, world-position moves, queue capacity and recursive ownership.
All twenty-two portable checks pass on Windows x64, Linux x64 and ARM64 Linux
under QEMU. The exact tested revision and CI job IDs are tracked in
[platform evidence](../analysis/portable-core-platform-validation.json).

```powershell
python scripts/check_scene_lod.py --dll build/native-recovery/Release/FS15Core.dll
cmake --build build/portable-core --config Release
ctest --test-dir build/portable-core -C Release --output-on-failure
```

The [group XML callbacks](SCENE_GROUP_RECOVERY.md) now construct and attach
TRS/LOD records and connect animations through their loaded source IDs. Complete
loader ownership, spatial tree traversal/frustum culling, derived shape/light/
camera consumption, full renderer scheduling and engine startup remain open.
The future Switch build is still deferred and unverified.
