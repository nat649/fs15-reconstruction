# Recovered scene attachment and removal

Three GE6 hierarchy entries now have bounded portable C++ paths:

| Native entry | Recovered behavior |
|---|---|
| `0x1401363d0` | Remove from the old parent's ordered child vector, set the new parent, append/insert and return the resulting index |
| `0x1400b17b0` | Matrix-node link, observer rescan, identity-root optimization and inherited visibility/clip/mask/transform updates |
| `0x1400b19c0` | Erase a child slot, clear its parent, update ancestor observer flags and optionally update the detached subtree |

Instruction inspection restores arguments missing from the pseudocode: unlink
propagates mask `0x3f` with **origin 0**, and the matrix link also uses origin 0.
The identity-root test inspects twelve matrix lanes with a strict **`1e-4f`**
threshold. It excludes affine padding. Reparenting retains local coordinates;
the child's world pose follows its new ancestor chain.

Reparenting an already attached node first performs ordinary unlink updates,
then link updates. Observers can therefore receive two ordered subtree traces.
Reordering within the same parent uses the child count **after removal**. Vector
erase retains capacity and the unused trailing slot, matching the native code.
The suppression argument used by the native destruction path skips detached
subtree updates but still rescans the former parent's observer flags.

## Portable boundary and provenance

`fs15_scene_link` and `fs15_scene_unlink` extend `scene_state.h` using its partial
native node layout and known base/TRS dispatch. Caller-owned writable child
storage must match the arena's child view. Each node reserves a disjoint span
through its observed begin/end/capacity pointers.

The native control flow, masks, ordering and identity test are recovered.
Temporary storage, bounds/forest checks, atomic commit and notification
transport are authored. Standard C++ rotation/copy operations implement the
observed pointer-vector effects; the original CRT/container helpers are not
claimed as recovered functions.

**Native vector allocation/growth is not implemented.** Exhausted child or event
capacity rejects without changing nodes, child storage, events or the returned
insertion index. The event-count output may report the required capacity.
Malformed pointers, overlapping reserved vectors, inconsistent links, duplicate
children, cycles and hierarchies deeper than 256 nodes are rejected by authored
guards. Outputs must not overlap the supplied input views.

These APIs do not allocate or destroy objects. They recover scene membership
changes relevant to lifecycle, not constructors, destructors, reference counting,
global registries, object naming or resource ownership. Arbitrary derived
vtables and observer callbacks that mutate the scene remain unresolved.
ReFarm still uses authored scene ownership; the new hierarchy component is
validated independently and is not substituted for a complete GE6 lifecycle.

## Evidence

- **19,200 successive complete node-arena and child-storage comparisons** on
  synthetic forests, including unused vector slots and parent references.
- 4,800 raw links, 5,137 matrix links, 4,800 normal removals and 4,463 suppressed
  removals. Cases include 7,335 reparentings, 2,602 root attachments and 1,217
  same-parent reorderings.
- **19,200 notification-trace comparisons**, containing 18,726 events, with exact
  order and origin. Reference callbacks only record events.
- A fifth asset-free CTest checks independent hierarchy/world-pose invariants,
  observer transfer, same-parent ordering, suppression, capacity atomicity and
  malformed/cyclic forest rejection.
- All fourteen portable-core tests pass on Windows x64, Linux x64 and ARM64 Linux
  under QEMU. Cross-platform checks use asset-free invariants; the original-code
  differential comparisons run on Windows only. No Switch build is validated.

All node bytes are compared exactly after canonicalizing addresses of the two
synthetic node/vector allocations. Property and observer storage is shared by
both fixtures. No game entry point, renderer, live game process or original
object allocator is initialized. Raw exports and original code remain local.

See [native evidence](../analysis/scene-hierarchy-validation.json),
[portable platform results](../analysis/portable-core-platform-validation.json)
and [remaining porting work](PORTABILITY_STATUS.md).

```powershell
python scripts/check_scene_hierarchy.py --dll build/native-recovery/Release/FS15Core.dll
cmake --build build/portable-core --config Release
ctest --test-dir build/portable-core -C Release --output-on-failure
```

The newer [owned scene component](SCENE_OWNED_RECOVERY.md) reuses these recovered
operations on separately allocated records and recovers child-vector growth.
The original fixed-arena API and the measurements above retain their stated scope.
