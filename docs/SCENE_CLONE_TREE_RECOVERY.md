# Recursive scene cloning and pointer remapping

Seven further native entries now have bounded reconstructed paths in the
portable C++17 core. This milestone connects the recovered single-record copy
to native-order subtree traversal, owned attachment, pointer remapping and
clone-list storage. It does not recover the complete top-level scene-clone
finalizer, skin/animation rebinding or unsupported derived resource classes.

| Entry | Recovered behavior |
|---|---|
| `0x1401ba050` | Allocate a `0x30`-byte original-to-clone map candidate |
| `0x14016ea00` | Unique insertion, consuming duplicate candidates without replacing values |
| `0x14016eb20` | Pointer-keyed RB tree attachment, rotations and recoloring |
| `0x140096190` | Recursive map-node cleanup in descending unsigned-key order |
| `0x140088360` | Clone-list pointer-vector reservation |
| `0x140320740` | Clone-list 1.5x growth when an append needs space |
| `0x1401b9d70` | Recursive clone dispatcher on known TRS, Camera, LOD and Light branches |

Existing constructors, single-record copy, typed attribute/property cloning,
hierarchy dispatch and pointer-vector algorithms are reused. Distinct native
vector template instantiations are compared directly; they share portable
allocation logic with the already recovered vector implementation. No existing
entry is counted twice. See [source-pinned evidence](../analysis/scene-clone-tree-validation.json)
and [single-record copy](SCENE_CLONE_RECOVERY.md).

## Remap layout and ownership

The map header has an opaque word at `+0`, sentinel pointer at `+8` and count
at `+0x10`. Each node holds left/parent/right pointers, source at `+0x18`, target
at `+0x20`, color/nil bytes at `+0x28/+0x29` and untouched padding. Source keys
compare as unsigned 64-bit pointer values. Insertion preserves native topology,
extrema, colors and duplicate behavior. The candidate factory initializes only
observed fields; it does not zero opaque bytes or the sentinel payload.

Initialization and whole-map release are authored C entry points around the
observed setup/free sequence. Releasing the map frees map nodes and optionally
its sentinel, never the mapped scene objects. Clone-list storage also owns
only its pointer allocation. Scene allocations remain owned by the constructor
callback and recovered lifetime services.

The comparison suite checks standalone maps with identical numeric keys on
both sides, including zero, maximum unsigned and high-bit keys. Exact topology
comparison is meaningful there. Scene source addresses differ across the two
heaps, so their numeric ordering and resulting tree topology may differ. Those
scene remaps compare normalized original-to-clone associations. Original GE6
lookup code additionally consumes the reconstructed map storage directly.
Cleanup ordering is checked against descending unsigned keys in each heap.

## Native traversal and attachment

The recovered dispatcher first searches the original-to-clone map. An already
mapped source returns null and performs no clone or attachment; it does not
return the previous target. A fresh supported source is handled in this order:

1. Dispatch by native type flags and run the known constructor/copy wrapper.
2. Insert the original address and new clone into the remap map.
3. Append the new clone to the pointer vector, retaining native capacity/spares.
4. Traverse source child slots in order. Null children and children without
   type flag `0x40` are skipped, including their descendants.
5. Recursively clone eligible children and attach each nonnull result using
   the recovered ordered link path with position `UINT32_MAX`.

Camera flag `0x8000` takes precedence, followed by the original derived
dispatch order. Known LOD flag `0x80` precedes plain TRS flag `0x40`. A plain
root without `0x40` returns null. Light flag `0x2000` now selects the
[recovered Light wrapper](SCENE_LIGHT_RECOVERY.md), before reference/LOD/TRS
dispatch. The Shape and reference/resource clone branches remain unsupported and reject where encountered; they are not
reinterpreted as generic transforms. A completed prefix remains available.

The cloned hierarchy retains local transforms, source names, supported
properties, independent attributes and the Camera/LOD copy behavior already
recovered. The clone's root remains detached, while children attach beneath
the copied parents. The original source tree is not reparented. LOD requests
from copy and attachment reach the recovered registry/world-position update.

The `constructed` callback must retain every completed allocation and add it
to supported owned transport before copy services run. Failures can leave
registered objects, remap entries, list entries and partial links. `result`
retains a root allocation when one exists; `unfinished` identifies a record at
failure and may alias it or an existing map/list entry. These are references
to one allocation, not independent ownership. Callers release all retained
prefixes using known lifecycle dispatch, without double destruction.

## Evidence and limits

The private Windows oracle maps the pinned original image and executes its
unchanged code, with explicit platform/private-heap/locale seams. It does not
run the game entry point or modify a live game. Comparisons pass for:

- 5,490 complete candidate records and successive remap states, including
  5,250 insertions, 240 duplicate insertions and 120 complete lifetimes;
- 3,000 complete clone-vector states: 1,000 reserves and 2,000 grow/appends;
- 628 source XML begin callbacks followed by 564 recursively cloned nodes;
- 99 complete scene lifetimes, 297 successive forest states and 3,873 node records;
- 99 duplicate-root no-ops and 564 original lookups on reconstructed remaps;
- 72 post-copy LOD updates and 99 complete LOD-manager allocation comparisons;
- 63 complete original subtrees: 20 from Krone Swadro, 19 from Map01 and 24
  from Map02. Original attributes, transforms and hierarchy are retained.

No unsupported child is removed or reparented to fabricate an original map
fixture. Synthetic mixed TRS/Camera/LOD trees separately test the native
excluded-child filter. All compared node/property/attribute bytes, names,
child-vector allocations and clone-list allocations are checked. Pointer
identities are normalized; transform/platform-libm tolerance is `1e-5`.
Pending trigger metadata, if encountered, is deactivated only for bounded
teardown after copy comparisons; active trigger/backend teardown is not proven.

The asset-free portable test covers remap reuse/duplicates, copied hierarchy
and world positions, excluded subtrees, stopping at unsupported derived
dispatch and cleanup across 55 allocation-failure attempts. Bounds, validation,
failure cleanup and owned callback transport are authored. Current limits are
65,535 traversal calls, depth 256 and 1,048,576 remap/vector slots. Repeated
validation/staging is a bounded correctness transport, not a validated strategy
for loading the entire original maps in one allocation/view.

```powershell
cmake --build build/native-recovery --config Release --target FS15Core
python scripts/check_scene_clone_tree.py --dll build/native-recovery/Release/FS15Core.dll
cmake --build build/portable-core --config Release
ctest --test-dir build/portable-core -C Release --output-on-failure
```

All twenty-three portable tests pass on Windows x64, Linux x64 and ARM64 Linux
under QEMU; all 28 native differential suites pass on Windows.
[Platform evidence](../analysis/portable-core-platform-validation.json) is
recorded separately from original-code comparisons. The larger original
`0x14016de50` path still needs shape/skin remapping, reference resolution,
animation rebinding, backend integration and finalization. Full GE6 linking,
engine startup and Switch execution remain unverified.
