# Recovered translation, rotation and scale updates

Eleven additional native GE6 entries are reconstructed in portable C++17:

| Entry | Recovered behavior |
|---|---|
| `0x14009de00` | Rotate a matrix by one axis using the original column-pair updates |
| `0x14009d8c0` | Compose local TRS: Z, Y, X rotations, then per-column scale and original translation |
| `0x140057980` | Extract Euler angles, including zero-scale reciprocals and the `1e-6` gimbal branch |
| `0x14009e520` | Refresh translation/rotation/scale caches from the local matrix and clear dirty bit `0x80` |
| `0x14010fd00` | Rebuild the local matrix, dispatch its setter, propagate mask 1 with origin 0, then clear dirty TRS state |
| `0x1400e51f0`, `0x1400e60c0`, `0x1400e6610` | Complete translation/rotation/scale setter frames, lazy refresh and rebuild |
| `0x1400e5260`, `0x1400e6130`, `0x1400e6750` | Complete getter frames with numeric tags, lazy cache refresh and unrelated-byte preservation |

Instruction inspection confirms the explicit `R8D=0` before the rebuild's
virtual propagation call. The pseudocode omitted that argument. Rotation-axis
column pairs are `(4,8)`, `(8,0)` and `(0,4)`. These details come from the pinned
original image; reconstructed code does not execute that image at runtime.

## Original algorithm and platform math boundary

`scene_trs.cpp` recovers GE6's matrix/update flow. Its sine, cosine, atan2 and
square-root operations use the platform C++ math library. Original CRT function
bodies are not reconstructed or embedded. This boundary is important for other
CPU architectures and C libraries: it is not a claim of universal bit identity.

The native comparison gate allows scaled float error of `8e-6`. In the recorded
Windows run, **all compared float vectors/matrices were bit-identical**, with
zero maximum error. That observation is stronger than the gate for these cases,
but does not extend to every input or other platforms' math implementations.

The native scale setter reports an error for non-unit dynamic-object scaling
and still stores/rebuilds the scale. The reconstructed API preserves that update
and returns a diagnostic flag instead of invoking the unrecovered engine logger.
The isolated reference logger has zero listeners; logger callback behavior is
not validated by this component.

## Evidence and integration

- 6,000 complete matrices for composition, 6,000 for axis rotation and 6,000 Euler
  vectors, including zero scale, reflected bases and gimbal cases.
- 7,200 complete scene arenas across refresh/rebuild and six Lua adapters.
  Flags, unknown bytes, pointer-normalized record regions and frame metadata
  remain exact; float regions are compared with the documented gate.
- 7,200 notification traces, including 5,607 events with exact order and origin.
- 2,700 getter vectors and 351 exercised dynamic-scale diagnostic conditions.
- A fourth asset-free portable CTest checks rotation conventions, decomposition,
  gimbal behavior, lazy caches, getters/setters and invalid-input atomicity.
- All twenty portable-core tests pass on Windows x64, Linux x64 and ARM64 Linux
  under QEMU. Only the Windows differential run executes original native code;
  cross-platform invariant checks do not prove universal bit identity.

ReFarm's I3D node loading, animated hierarchy updates and vehicle-root poses
now call recovered TRS composition and affine parent multiplication. The XML
loader, ownership containers, visibility policy, renderer and physics remain
authored or otherwise incomplete. Original I3D translation, scale, Euler units,
hierarchy and resources are retained; this does not introduce new map placement.
After integration, both maps pass the 24-run source/bytecode, frame-partition and
pause reliability suite. Workspace/package renders match for both static scenes
and starting-fleet/discharge scenarios. Bjornholm's 272 sampled road vertices
retain approximately `0.04965 m` median DEM alignment error.

See [native comparison results](../analysis/scene-trs-validation.json),
[component ledger](../analysis/native-recompilation-summary.json) and
[remaining porting work](PORTABILITY_STATUS.md). There is still no complete
recovered GE6 executable or validated Switch build.

```powershell
python scripts/check_scene_trs.py --dll build/native-recovery/Release/FS15Core.dll
cmake -S . -B build/portable-core -DFS15_PORTABLE_CORE_ONLY=ON
cmake --build build/portable-core --config Release
ctest --test-dir build/portable-core -C Release --output-on-failure
```
