# Recovered Camera view preparation and shader-block packing

Two additional GE6 entries now compile in the recovered C++17 core:

| Original entry | Recovered path |
|---|---|
| `0x1400b3e20` | Camera world/cache refresh, basis conditioning, perspective/orthographic projection, stereo displacement/off-axis shift, inverse, projective product, frustum and shader-block preparation |
| `0x14012cca0` | Position/normalized basis/self-volume updates and 49-float shader-block writes between buffer map/unmap calls |

The original bodies execute unchanged in the isolated x64 comparison. World
and buffer transport in the recovered implementation are explicit authored
callbacks. The native reference uses owned CPU buffers through synthetic
map/unmap virtual slots; no real GPU driver is executed. Exact labelled view
chains reuse the original tangent routine at the math boundary. Separate
chains exercise the platform tangent backend. Neither is a claim that the
native CRT or graphics backend has been reconstructed.

Source: [implementation](../src/core/scene_view.cpp),
[record/service interface](../src/core/scene_view.h),
[native comparison](../scripts/check_scene_view.py),
[portable fixtures](../tests/scene_view.cpp) and
[source-pinned evidence](../analysis/scene-view-validation.json).

## Native records and shader data

The inspected render-view record occupies `0x1e8` bytes. Its known fields are:

| Offset | Field |
|---|---|
| `+0x00` | Existing six-plane frustum prefix |
| `+0x68/+0xa8/+0xe8/+0x128` | Camera world, inverse, projection and projection-times-inverse matrices |
| `+0x168` | World position XYZ followed by zero |
| `+0x178` | Three independently normalized basis vectors |
| `+0x1a0/+0x1a8` | Camera identity and self-volume pointer |
| `+0x1d0/+0x1d8/+0x1e0` | Settings pointer, primary buffer token, secondary buffer token |

Other fields remain opaque. Allocation, constructor/destructor behavior and
the secondary buffer's full use are not recovered by these two paths.

The mapped block contains exactly **49 floats / 196 bytes**:

| Float indices | Contents |
|---|---|
| 0–11 | Three transposed world rows, each including translation |
| 12–27 | Transposed full projection matrix |
| 28–31 | Position XYZ and zero |
| 32–47 | Original settings values in native order; parameter meanings remain unresolved |
| 48 | Original double clock value multiplied by `0.001`, then rounded to float |

Mapped spare bytes remain untouched. The native routine refreshes position,
basis and the self-volume pointer **before map**. Camera identity changes only
**after unmap** in the preparation caller. The comparison checks complete
records at both callback boundaries, not just the final shader values.

Stereo preparation retains native half-offset multiplication, eye-dependent
sign inversion, displacement along the conditioned world X basis and the
off-axis shift using the larger of near distance and the Camera convergence
field. The same branch applies to perspective and orthographic cameras.
Singular affine inverses retain the previous inverse matrix and continue,
matching the original caller's ignored inverse-failure result.

## Evidence

The Windows reference comparisons pass:

- 3,000 exact complete upload records, including mapped and record spare bytes;
- 2,172 exact view chains, with the original tangent boundary where needed;
- 246 view chains using default platform tangent, with measured scaled error
  zero on this Windows build and an explicit `1e-5` comparison tolerance;
- 2,418 complete parent/Camera cache arenas and map/unmap trace pairs for view
  preparation, plus 3,000 upload trace pairs;
- 806 mono and 1,612 stereo chains, including four retained singular inverses;
- 2,418 ordered spatial queries consuming the prepared **self-volume**, with
  27,298 matches and 2,098 truncated output prefixes.

Six original Camera parameter sets come from both base maps and two tractors.
Each is included in the view/stereo fixtures. Camera transforms, parent
arenas, aspect ratios, stereo inputs, settings, clock values and query rows
are authored fixture inputs. Resource hashes remain in the aggregate report;
original files, instruction exports and pseudocode remain ignored locally.

The platform-math tolerance applies only to projection, combined and frustum
floats and their packed projection values. All other bytes and cache changes
still compare exactly. The recovered [world cache](../src/core/scene_graph.cpp),
[basis/projection math](SCENE_PROJECTION_RECOVERY.md),
[frustum and ordered queries](SCENE_VISIBILITY_RECOVERY.md) are consumed together
through this native preparation path. No drawing callback is substituted.

The portable fixtures cover shader layout/spares, callback/identity order,
opposite-eye offsets, singular inverse retention, and map failures. Authored
finite/length/capacity guards reject invalid inputs. A nonnull short map is
balanced by unmap without writing constants. Null map has no unmap. Unmap
failure retains already written values and the old Camera identity. Prefix
and world-cache mutations can remain after callback failure; no transaction
across external services is promised.

## Remaining work

Render-view allocation/initialization, other view variants, real GPU buffer
creation/map/unmap/upload and driver integration, Shape/audio/render consumers,
dynamic shadow-volume construction, native startup and complete resource
ownership remain open. This component does not draw a frame or establish a
complete rebuilt GE6 executable. Switch packaging/hardware validation remain
separate future work.

```powershell
cmake --build build/native-recovery --config Release --target FS15Core
python scripts/check_scene_view.py --dll build/native-recovery/Release/FS15Core.dll
cmake --build build/portable-core --config Release
ctest --test-dir build/portable-core -C Release --output-on-failure
```

Only native comparisons need the pinned local image and original reference
resources. The portable core and its fixtures require no original payloads.
