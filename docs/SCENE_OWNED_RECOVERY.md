# Owned child vectors and allocated scene hierarchies

Recovered scene operations can now work on individually allocated objects and
their independently owned property, child and observer buffers. Previously the
portable hierarchy component required a contiguous synthetic arena and fixed
child-vector capacity. This removes that transport constraint before recovering
the I3D node-construction callbacks.

## Recovered code and authored transport

Three additional native entries are reconstructed in portable C++:

| Entry | Behavior |
|---|---|
| `0x1400d5f30` | Reserve a larger pointer vector, copy used elements, free old storage and preserve its length |
| `0x1400d34e0` | Ensure one spare pointer slot, growing by `max(count+1, capacity+capacity/2)` |
| `0x14005a1b0` | Append an external or in-vector aliased pointer value, rereading an aliased element after relocation |

The native growth sequence starts `1,2,3,4,6,9,13`. A reserve copies used elements;
new unused bytes remain supplied by the allocator. Removal retains capacity and
its trailing pointer residue. Same-parent reordering removes the old membership
first, so a full vector does not need to grow for that operation.

`FS15SceneOwnedView` and its scatter/gather staging are **authored transport**.
They do not count as additional recovered GE6 functions. They adapt separately
allocated native-layout records into the existing recovered state/TRS/hierarchy
operations and commit changes back to their original owners. Parent/child
identities and property views are remapped; observer identities, opaque fields,
unchanged unused slots and unrelated ownership pointers are preserved.

Ordered link/reparent/unlink uses the already recovered `0x1401363d0`,
`0x1400b17b0` and `0x1400b19c0` behavior. Those entries are not counted again.
Child-vector growth now uses the recovered capacity/allocation rules. A service
allocator supplies the platform heap; its implementation remains authored.

## API scope and failure behavior

The view must describe a complete valid forest of supported matrix/TRS records,
with every object listed once. Default properties and detached body/joint
metadata are valid first-16-byte property views. The original interfaces retain this matrix/TRS contract. New
[LOD-aware variants](SCENE_LOD_RECOVERY.md) accept known LOD records with ordered
pending-request output; other derived dispatch remains excluded.

Bounds currently allow 65,535 nodes, 1,048,576 total child/observer slots and
256 levels. Pointee storage and complete vector allocations must be valid.
Staging copies the supplied forest and allocated child slots for each call; it
is a bounded validation transport, not a proven full-map loading strategy. The
complete original maps exceed its current node bound.

Output buffers must be disjoint. Bounds, cycle checks and transactional staging
are authored guards: insufficient event capacity, invalid views and allocation
failure leave the live forest unchanged. Native exception/prefix-write behavior
is not claimed for those rejected cases. Callbacks cannot throw or reenter.

Notifications retain original node indices, observer identities, order and
origin. They are returned for caller delivery. Real observer side effects and
reentrancy are not simulated. Recursive destruction still uses the recovered
object lifetime chain and known dispatch services; it is distinct from staging
a hierarchy operation.

## Evidence

[Source-pinned native comparison results](../analysis/scene-owned-validation.json):

- 9,600 complete pointer-vector allocation states across 1,066 reserves,
  1,066 grow calls and 7,468 appends, including 3,162 in-vector aliases;
- an external argument aliased to the vector's own begin field, verifying
  that the value is read after growth changes that field;
- 120 complete separately allocated forest lifetimes;
- 10,800 successive forest states and 179,100 complete node-record comparisons;
- 2,763 actual child-vector relocations, 3,600 raw and 1,800 matrix links,
  1,800 removals, 1,800 state updates and 1,800 TRS rebuilds;
- 2,989 ordered side-effect-free observer notifications;
- common XML attributes applied through recovered services to actual allocated
  records, including detached body/joint properties;
- recursive destruction with every owned buffer and object registration released.

The original constructor, allocator/container, hierarchy, state and destructor
code executes unchanged in a private-heap reference process. XML decoding,
platform initialization and portable registry/dispatch services remain authored.
No game entry point or live game is touched. All supported record bytes and
complete child allocation contents are compared after address normalization;
TRS/libm regions use scaled tolerance `1e-5` across successive operations.

The asset-free `portable_scene_owned` check exercises the growth sequence,
aliasing, multi-level ownership, insertion order, reparenting, inherited state,
observer identity/order, failure cleanup, inconsistent membership and cycle
rejection. All twenty-one portable checks pass on Windows x64, Linux x64 and ARM64 Linux
under QEMU. The exact tested revision and CI job IDs are tracked in
[platform evidence](../analysis/portable-core-platform-validation.json).

```powershell
python scripts/check_scene_owned.py --dll build/native-recovery/Release/FS15Core.dll
cmake --build build/portable-core --config Release
ctest --test-dir build/portable-core -C Release --output-on-failure
```

The [group callbacks](SCENE_GROUP_RECOVERY.md) now construct and attach known
TRS/LOD records through this owned transport. The I3D tokenizer, other derived
callbacks, original complete loader class, render/physics backend and engine
startup remain open. The Switch target remains
deferred; ARM64 Linux execution is separate from a console build.
