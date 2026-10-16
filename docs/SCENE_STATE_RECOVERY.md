# Recovered scene state and virtual dispatch

The portable core now reconstructs **five additional native scene paths**.
They operate on bounded caller-owned records and do not constitute a complete
scene allocator, resource lifecycle, physics integration or game startup.

| Original entry | Reconstructed path |
|---|---|
| `0x1400b1b00` | Dirty-state suppression, visibility inheritance, inherited flags, clip-distance union, mask intersection, transform notifications and descendant propagation |
| `0x1400b6160` | Recompute observer-presence bit `0x10` and propagate changes upward |
| `0x1400b61f0` | Set inherited flag `0x08` through descendants |
| `0x1400b15d0` | Base matrix-node local matrix copy |
| `0x14010fce0` | TRS-node local matrix copy plus dirty-TRS bit `0x80` |

The two tables at `0x1404c0cd8` and `0x1404c5d98` are explicitly referenced
by the locally exported constructors. Their eight observed slots contain ten
distinct targets. Four were not previously catalogue entries. Discovery created
functions only in uncovered executable code and never split existing bodies.
All four now have local pseudocode exports. The wider catalogue contains
12,200 entries, including 11,635 eligible exports and 565 excluded thunks/external entries.

## Layout and bounded API

`src/core/scene_state.h` defines a partial `0x170`-byte native record with assertions
on field offsets. It includes the parent, child/observer vector views, local
and cached world matrices, resource-property pointer, local/inherited clip
distances and masks, and cached TRS fields. Unrecovered bytes remain opaque.
The names are inferred from observed behavior, not recovered original symbols.

The authored arena supplies bounds for nodes, child pointers, observer tokens,
resource-property records and the two known virtual-dispatch kinds. No raw
original vtable is executed by the reconstructed library. Updates are staged;
invalid views, cyclic/inconsistent links, nonfinite inputs, insufficient event
capacity and singular inverses reject without changing the records or events.
Descendant traversal has an authored limit of 256 levels.

Notifications are returned in the original traversal order for later caller
delivery. Original callbacks in the native oracle only record notifications;
they do not mutate scene objects. Callback reentrancy, callback side effects,
additional derived types and engine-managed object ownership remain unresolved.

The world-preserving child path uses the recovered world-cache getter, affine
inverse/multiply and the observed child matrix setter. On its resource-flag and
origin conditions, it updates the relative local matrix while retaining the
child's cached world pose, and removes transform-change propagation for that
child. This validates the matrix branch, not a real rigid-body solver.

## Validation

- 2,400 original-code comparisons for each of the five entries: **12,000 complete
  node arenas**, including flags, matrices, unknown bytes and unmodified fields.
- **7,200 notification-trace comparisons**, containing 1,173 observed events.
- **185 local child matrices changed** through the world-preserving branch.
- Cases cover visibility roots, dirty-cache suppression, clip/mask pruning,
  reflected bases, non-transform child filtering and signed-zero clip values.
- A third asset-free portable CTest checks propagation invariants, event-capacity
  atomicity, invalid pointers/cycles, matrix setters and singular parents.
- All thirteen portable-core tests pass on Windows x64, Linux x64 and ARM64 Linux
  under QEMU. This does not validate a Switch executable or console hardware.

Only pointer-bearing fields that differ because the two synthetic arenas have
different addresses are canonicalized for complete-arena comparison. Original
node/resource allocations, actual game instances and raw snapshots stay local.
See [native evidence](../analysis/scene-state-validation.json) and
[vtable discovery evidence](../analysis/scene-vtables-summary.json).

ReFarm continues using the previously recovered coordinate adapters. This new
state component is validated independently and is not yet connected to a full
native object lifecycle in ReFarm or a fully recovered GE6 executable.

```powershell
scripts/analyze.ps1 -Target game-runtime -Existing -Vtables '1404c0cd8:8','1404c5d98:8' -ExportName scene-vtables -JavaHome 'C:\path\to\jdk-21'
scripts/analyze.ps1 -Target game-runtime -Existing -FullExport -ExportName game-full -FunctionTimeoutSeconds 300 -JavaHome 'C:\path\to\jdk-21'
python scripts/index_native_exports.py
python scripts/index_scene_vtables.py
python scripts/check_scene_state.py --dll build/native-recovery/Release/FS15Core.dll
```
