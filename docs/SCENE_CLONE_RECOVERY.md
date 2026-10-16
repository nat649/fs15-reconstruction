# Scene attribute, property and node copy recovery

Seven additional bounded native paths now compile in `FS15PortableCore`.
They recover the shared copying behavior used below the original scene clone
dispatcher, including Camera and LOD wrappers. This is not yet the complete
recursive scene clone/remapping pipeline or a complete vehicle loader.

| Entry | Recovered behavior |
|---|---|
| `0x14016eea0` | Allocate and clone one typed attribute value |
| `0x1401366d0` | Clear destination attributes and copy source entries in key order |
| `0x1401279d0` | Clone rigid-body property metadata into a new detached record |
| `0x140149a60` | Clone joint property metadata into a new detached record |
| `0x14010fd80` | Copy common TRS-node state, properties and attributes |
| `0x1400b5950` | Construct a fresh Camera and apply the common copy |
| `0x1400d7ca0` | Construct a fresh LOD group and apply the common copy |

The default-property clone, constructors, attribute RB insertion/cleanup,
TRS refresh/rebuild, state propagation and supported owned/LOD services were
already recovered and are reused without counting their entries again.
Plain TRS allocation plus common copy is exposed through authored known-type
dispatch; this does not mark the larger original `0x1401b9d70` dispatcher as
recovered. See [source-pinned native evidence](../analysis/scene-clone-validation.json).

## Preserved copy semantics

Attribute values start with a zero 64-bit payload and the source type.
Boolean copies one byte, integer/float copy four bytes, and string kinds `3`
and `4` allocate independent terminated text. Unknown kinds retain their type
and a zero payload. Opaque `+0xc` bytes remain untouched, rather than being
copied from the source.

Map assignment destroys existing destination values/keys and reinserts source
entries in sorted order using the native RB tree behavior. Tree topology,
colors, key capacities/spare bytes and value allocation contents match the
reference. It is not a dictionary conversion or a raw pointer copy. Native
self-assignment clears the map. Kind `4` text is duplicated, but the original
map cleanup still does not release it; the fixtures track those retained
allocations separately and reclaim them explicitly. Ownership quirks are not
silently replaced with different semantics.

Rigid-body cloning constructs default metadata with the new owner, merges
source flags, clears the native low state bits, copies collision/material and
damping fields, clamps density to the native minimum and clears its override
flag. Optional center-of-mass fields copy only when source flag `0x8000` is set.
Solver iterations copy, the final change mask is zero, and backend/attachment
fields remain freshly initialized. Unknown fields that the original clone
does not copy retain their constructor/default/opaque state.

Joint cloning copies projection/drive values, per-axis limits, break values
and the observed byte at `+0x80`. Source flags merge with constructor defaults,
then low state bits clear. Manager/constraint pointers and observer bindings
are not copied. Neither property path duplicates a live PhysX actor or joint.
Native trigger flags can remain in copied metadata; active-trigger teardown
still requires unrecovered services and remains a separate boundary.

The common node copy preserves this order:

1. Refresh the source's lazy TRS cache if dirty; copy nine cached floats.
2. Clear the target cache-dirty bit and rebuild/propagate its transform.
3. Copy flag `0x20`, clone the source property and destroy/replace the target property.
4. Apply each changed clip value separately with propagation mask `8`, then
   each changed local mask with `0x10` / `0x20`, all with origin zero.
5. Apply visibility and propagate mask `2` if changed; copy flag `4`.
6. Replace the attribute map.

Destination identity, class/table, name, child ownership and shared-animation
binding remain its existing state. A clone wrapper obtains a new registered
object/ID through its constructor and copies the source name. It does not
copy the source's child vector or its animation binding. Existing target
children remain attached when copying into an existing node.

The LOD wrapper infers its distance count by scanning from the second slot
with a previous value initialized to zero. A fully distinct four-slot array
therefore produces count `3`: `(0,10,20,30)` clones as `(0,10,20,20)`. A second
slot equal to zero produces count `1` and constructor defaults. This behavior
is retained from the instructions instead of replacing it with a four-float
memcpy. Camera cloning copies only the constructor's projection parameters
before the common TRS copy; opaque/default tail fields are freshly initialized.

## Ownership and validation

The C API uses authored allocator, registry, owned-forest and propagation
callbacks; stable native vtable addresses are identity tokens, never callable
tables in the portable binary. `constructed` registers a fresh record with
the caller's owned view before common copy callbacks run. After successful
construction, the result remains caller-owned even if a later step fails.
Completed prefix mutations and registry effects are not rolled back.

Known default, detached body and detached joint target properties can be
replaced. Trees/pointees must be valid owned storage; distinct owners must not
share member allocations. Bounds/finite/allocation guards and failure cleanup
are authored. Native exception unwinding and temporary CRT string allocations
are not reproduced. This component copies supported single records; original
top-level clone finalization, unsupported derived resources, backend observers,
shared-animation rebinding and complete loading remain open.

The private Windows native reference calls unchanged code from the pinned image
without starting its entry point or altering a running game. Comparisons cover:

- 1,800 complete attribute-value clones, including strings and unknown kinds;
- 900 complete default/body/joint property clones;
- 270 map assignments, 90 self-assignments and 90 complete map lifetimes;
- 1,344 observed retained kind-4 text allocations in the map fixtures;
- 270 copies into existing scene nodes, 90 self-copies, 30 Camera and 30 LOD clone wrappers;
- 600 complete forest states, 2,580 node records and 2,940 complete tree comparisons;
- 90 complete cloned LOD-manager allocations and 90 complete scene lifetimes.

Complete record/property/tree bytes are checked after pointer normalization;
transform/libm tolerance is `1e-5`. Child-vector contents/capacities and names
are compared separately. The portable test loads its source through recovered
I3D group/Camera callbacks, validates independent attribute storage and LOD
distance behavior, and exercises allocation failures with owned-prefix cleanup.
All twenty portable tests pass on Windows x64, Linux x64 and ARM64 Linux
under QEMU. All 25 native differential suites pass on Windows. Platform
validation is tracked in [the separate report](../analysis/portable-core-platform-validation.json).

```powershell
cmake --build build/native-recovery --config Release --target FS15Core
python scripts/check_scene_clone.py --dll build/native-recovery/Release/FS15Core.dll
cmake --build build/portable-core --config Release
ctest --test-dir build/portable-core -C Release --output-on-failure
```

Original executables, raw pseudocode and allocation snapshots stay local.
This milestone does not recover projection/rendering, full startup or a Switch
executable; the [remaining port work](PORTABILITY_STATUS.md) still applies.

The later [recursive clone component](SCENE_CLONE_TREE_RECOVERY.md) adds native
pointer remap storage, clone-list allocation and known TRS/Camera/LOD/Light subtree
assembly. Top-level shape/skin/resource finalization remains open.
