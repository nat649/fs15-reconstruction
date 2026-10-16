# Native spatial grid and active Light updates

The portable C++17 core now recovers the native grid's storage, cell selection,
registration, movement and removal, then connects these paths to Light bounds
and the original pending-update order. This replaces the earlier authored
Light-queue clearing fixture in a separate differential comparison. It does
not recover visibility queries, frustum traversal or GPU lighting/shadows.

## Native paths and records

| Entry | Bounded recovered path |
|---|---|
| `0x1400d6170` | Morton/depth-first cell index |
| `0x1400d62b0` | Cell entry allocation and append |
| `0x1400d6440` | Cell swap removal and moved owner's packed token |
| `0x1400d64e0` | In-place cell entry update |
| `0x1400d6660` | Grid initialization, equal-configuration no-op and entry migration |
| `0x1400d6d90` | Select the common ancestor cell of a sphere's X/Z extent |
| `0x1400d6f00` | Register an owner and write its packed cell/slot token |
| `0x1400d6fc0` | Locate and remove a registered owner |
| `0x1400d7060` | In-place movement or removal and registration in another cell |
| `0x1400da2e0` | Refresh a Light's world-space bound |
| `0x1400da450` | Consume Light dirty state and register/move/remove it |
| `0x140087f00` | Original manager order with Shape/audio pending lists empty |

The Light destructor's active-spatial branch extends the previously counted
`0x1400d9e10` path; it is not counted again. Existing world-cache, inherited LOD,
LOD update, scene lifetime and pointer-vector paths are reused.

| Record | Size | Observed layout |
|---|---|---|
| Grid | `0x38` | Cell pointer; dimension/level/cell counts; X/Z and Y limits; X/Z scales; opaque final word |
| Cell | `0x20` | Entry pointer; 16-bit count/capacity; X/Z center and half extent; depth-first subtree skip |
| Entry | `0x28` | Owner-token pointer; center/radius; two clip floats; LOD token; opaque final word |
| Sphere subobject | `0x20` | Identity table; opaque word; center at `+0xc`; radius at `+0x18`; opaque final word |

Inspection now identifies Light `+0x170` as its packed spatial token, written
on registration. The constructor leaves it untouched. The fields previously
labelled as spatial links/count at `+0x1e0..1f0` are the sphere's opaque word,
center and float radius. The sphere starts at `+0x1d8`; Light type remains at
`+0x174`. Unknown bytes are retained and the Light size remains `0x200`.

## Grid storage and movement

Initialization rounds the requested dimension up to a power of two in `1..128`.
The maximum grid has eight levels and 21,845 cells. X/Z coordinates are clamped
after native-order float-to-integer conversion. The selected cell is the common
ancestor of the clamped minimum/maximum coordinates. Morton interleaving and
the level offset identify its depth-first location. Each cell's skip value
points past its complete subtree, including the cell itself.

Cells grow by eight entries below capacity 64, then by 32. Reallocation copies
only live rows; spare bytes come from the allocator. A null sphere uses the
root cell and native unbounded sentinels: zero center, maximum finite radius
and near clip, negative maximum finite far clip. In-place update preserves the
owner pointer and opaque final word. Swap removal copies only the first
`0x24` bytes, preserves the destination's opaque word and updates the moved
owner's packed slot.

Packed tokens contain level in bits `0..2`, X in `3..9`, Z in `10..16` and
slot in `17..31`. Slot `0x7fff` is a search sentinel for indices at or above
32,767. Removal/movement searches those rows by owner-pointer identity. It does
not treat that token as an ordinary array index.

Reinitialization with changed dimensions/extents migrates live entries and
rewrites their owner tokens. The original frees the old cell-header allocation
but **retains the old per-cell entry buffers**. The recovered path preserves
that behavior, including retained bytes. The comparison explicitly verifies
and then reclaims these retained allocations through its authored allocator.
The new authored `spatial_release` helper releases only current cell buffers;
it is not an original destructor and cannot discover previously retained
buffers. A future runtime allocator must track/reclaim them explicitly when
reconfiguring a grid.

Finite/ownership/bounds validation and rejection of 16-bit capacity overflow
are authored guards. Warning/logger callbacks for oversized requested grids
are not recovered here. Allocation/callback failures may retain a completed
native-order prefix; these APIs do not promise a transaction over the manager.

## Light bounds, registration and pending consumption

Bounds refresh clears dirty bit `0x100`. Point/directional types copy world
translation and range. A spot light offsets its bound center along world Z by
`-((range * 0.5f) / innerConeCos)`, retaining native float operation order;
its radius remains the range. The world-matrix callback uses the recovered
world-cache path, supplied through authored owned-record transport.

Light update clears queued bit `0x800`; dirty bit `0x400` controls subsequent
processing. Visible and enabled lights resolve an invalid inherited LOD token
when needed, then register or move in the grid. Node flag `4` selects the null
bound/root sentinel. Hidden or disabled lights are removed. Active destruction
removes the pending entry first, then spatial ownership, then inherited TRS
members. Render-resource ownership still rejects before destruction.

The recovered manager consumer snapshots and processes pending LOD objects
first, allowing their token markers to enqueue Lights, then snapshots and
processes pending Lights. It clears Light and LOD vector ends afterward,
retaining capacities/spares. Original Shape and audio lists are explicitly
empty in this component. Their callbacks, queue consumption and spatial
queries remain open. The aggregate portable context layout is authored and
does not claim the complete original manager ABI or startup sequence.

## Evidence

The unchanged pinned image executes in a private Windows process. No game
entry point or live process is modified. The source-pinned
[differential report](../analysis/scene-spatial-validation.json) records:

- 72,816 native cell-index queries, all levels through a 128-by-128 leaf grid;
- 1,200 sphere-to-cell queries and 6,594 complete grid states;
- 1,167,562 cell/capacity snapshots, including empty cells and retained spare bytes;
- 4,236 dense cell appends, 432 cell moves and 432 swap removals;
- 5,760 grid add/move/remove operations across 24 complete mutable lifetimes;
- 48 nonempty reinitializations and 662 complete retained cell-buffer comparisons;
- five overflow-slot move/removal pairs around the `0x7fff` sentinel;
- 26 mixed Light/LOD forest lifetimes, 702 complete forest/LOD-manager states
  and 3,024 node records;
- 208 original manager flushes, 156 active transform updates, 468 active
  visibility/registration updates and 66 active spatial deleting destructions;
- six original Light subtrees from Map01, Map02, Deutz Agrostar and New Holland
  T8, with original attributes loaded before controlled update tests.

Pointer identities are normalized; grid/cell/header/token/retained-buffer bytes
match exactly on Windows. Existing forest comparisons use transform/libm
tolerance `1e-5`; the remaining Light fields compare byte for byte. The portable
test checks packed-slot swaps, root sentinels, allocation/extent guards, a
native spot-center result, hide/show registration and active deletion.
All 20 portable tests pass on Windows x64, Linux x64 and ARM64 Linux under
QEMU. All 25 native differential suites pass on Windows. Platform builds
are recorded separately in
[CPU evidence](../analysis/portable-core-platform-validation.json).

```powershell
cmake --build build/native-recovery --config Release --target FS15Core
python scripts/check_scene_spatial.py --dll build/native-recovery/Release/FS15Core.dll
cmake --build build/portable-core --config Release
ctest --test-dir build/portable-core -C Release --output-on-failure
```

Full scene/resource loading, spatial visibility/frustum queries, Shape/audio
dispatch, GPU resources/shadows, native subsystem startup and Switch execution
remain open. ReFarm's renderer is unchanged by this component milestone.
