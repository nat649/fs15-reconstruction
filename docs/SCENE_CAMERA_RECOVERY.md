# Native Camera recovery

Nine additional bounded GE6 paths are compiled in the portable C++17 core.
The reference is the pinned local x64 analysis image, SHA-256
`6d1beaf9d7e3c57dd51cec03385a53bda82b4ac946e622d8d2fa592a56d71f9f`.
The native comparison process maps this image privately and calls original code
without starting its entry point or modifying the live game. Allocator, registry,
owned-forest and callback transport around the recovered algorithms is authored.

| Native entry | Recovered path |
|---|---|
| `0x140052540` | Three-vector normalization using the native reciprocal square-root table |
| `0x1400b5870` | Camera constructor |
| `0x1400b5910` | Deleting/nondeleting destructor wrapper |
| `0x1400b59f0` | World pose to parent-relative Euler rotation and translation |
| `0x1400b5be0` | Follow a target along the normalized camera world Z axis |
| `0x1400b5d00` | Smooth the target point before applying the follow offset |
| `0x1400e8840` | Script-frame camera creation |
| `0x1400e8940` | Script-frame smooth-follow adapter |
| `0x14018c430` | Decoded I3D Camera begin callback |

The existing constructor, TRS, common XML, hierarchy, state and non-shape end
paths are reused; they are not counted again. See
[source-pinned evidence](../analysis/scene-camera-validation.json).

## Object and inherited dispatch

The Camera occupies `0x190` bytes. Its first `0x170` bytes are the recovered TRS
node. The constructor calls the typed TRS constructor with class `0x16`, adds
type flag `0x8000` and installs identity token `0x1404c1078`. These identity
addresses are never invoked as C++ vtables in the portable core.

| Offset | Field / native initialization |
|---|---|
| `+0x170` | FOV, stored directly in degrees |
| `+0x174` / `+0x178` | Near/far clip parameters |
| `+0x17c` | Orthographic height |
| `+0x180` / `+0x184` | Unknown fields, initialized to `0.1` / `3.0` |
| `+0x188` | Orthographic byte, preserved without canonicalizing direct constructor inputs |
| `+0x189..18f` | Untouched opaque bytes |

The eight inspected Camera slots inherit the same link/unlink, base state,
observer, LOD-marker/token and TRS matrix-setter bodies already recovered.
The owned view therefore dispatches Camera as TRS kind `1`, preserving its
derived tail. Known-table guards now accept Camera when traversing a branch
or inherited LOD token. The fixture checks a Camera with an attached LOD child:
camera motion reaches the descendant's world position in the LOD manager.
Recursive destruction dispatches the Camera wrapper and releases its ordinary
TRS members; no additional camera-owned allocation was observed here.

At the Camera milestone, four constructor-observed tables exposed 32 slots
and 14 distinct targets. The later [Light audit](SCENE_LIGHT_RECOVERY.md)
extends the total to five tables, 40 slots and 17 targets. The five earlier discovery records remain retained; the
Camera audit created no new functions. This is not exhaustive class discovery.

## XML and script inputs

The I3D callback ignores missing/null first `name` matches. All attribute
searches start at attribute zero and retain the first exact-case match. This
was checked against instructions because decompiler loop restructuring
misleadingly suggested that `fov` might skip the first attribute.

XML defaults are FOV `60`, near `0.1`, far `1000`, orthographic false and height
`1`. Scalar parsing uses the recovered scene numeric grammar. Orthographic is
true only for case-insensitive `true`. Common attributes use physics `0`, source
ID registration `1` and joint `0`; `dynamic`/`joint` attributes do not create body
or joint metadata on these nodes. Construction precedes common attributes,
ordered append and current-parent update. Camera end uses the existing
non-shape callback.

The callback API shares the group ownership contract: after successful
construction the caller owns the returned node even when registration,
attribute processing or attachment later fails. A completed prefix is not
rolled back. Constructor allocation failures clean acquired members before the
raw record is released. Derived lifecycle dispatch must remain supported.

The creation script frame holds name at `+0`, FOV at `+0x10`, near at `+0x20`
and far at `+0x30`. It creates a perspective camera with orthographic height
`1`. The follow frame holds target XYZ at `+0/+0x10/+0x20`, distance at `+0x30`,
alpha at `+0x40` and factor at `+0x50`. A null camera is a successful no-op.
The APIs add explicit frame/string bounds and leave input frame bytes intact.

## Pose and follow ordering

With a parent, pose refreshes its world matrix, initializes an inverse to
identity and attempts the recovered affine inverse. GE6 ignores inverse
failure; a singular parent therefore retains identity. It multiplies this
inverse by the requested world pose and extracts Euler angles without a scale
argument. A parentless camera uses the requested pose directly.

The camera's existing scale is retained. After a lazy TRS refresh if needed,
the function stores rotation and rebuilds/propagates. It then refreshes again
if necessary, stores translation and performs a second rebuild/propagation.
Collapsing these two updates would change callback and descendant ordering.

Follow copies the current world matrix, normalizes lanes `8..10`, replaces its
translation with `target + axis * distance`, then calls pose. Normalization
reuses the recovered seed-table initialization and performs the same two double
precision refinement steps. Zero vectors retain native zero results.

Smoothing first evaluates the camera origin through the world matrix, preserving
the original multiply/add order. For squared distance `d2 > 0.001`, it computes
`alpha = ((d2 * factor + (1 / d2) * 0.00065) * (1 / d2)) * alpha`, subtracts the
scaled displacement from that origin, and calls follow. These constants are
checked directly against the pinned image. No replacement camera-control model
is substituted for this behavior.

## Validation and remaining boundaries

The Windows native differential suite passes:

- 2,000 bit-exact vector normalizations;
- 200 direct constructors, 200 script creations and 400 complete lifetimes;
- 127 Camera XML creations, two ignored begins and 129 complete forest lifetimes;
- 381 pose, 381 follow, 381 smoothing and 381 script-follow updates;
- 1,653 complete forest states / 6,608 node records, including 18 parentless
  cameras, singular parents, retained non-unit scale, gimbal cases and 127
  exact/below-threshold smoothing inputs;
- six original camera attribute sets from both original maps, Deutz Agrostar
  and New Holland T8 resources, exercised in isolated synthetic parent fixtures.

Original camera attributes are retained; these isolated fixtures are not
complete original scene loads. Pointer fields are normalized for comparison,
opaque bytes are checked and transform/libm tolerance is `2e-5`. The asset-free
portable test also covers Camera-to-LOD updates, script frames, nonfinite guards
and allocation-failure cleanup. All twenty-one portable tests pass on Windows x64,
Linux x64 and ARM64 Linux under QEMU; all 26 native differential suites pass on
Windows. Platform checks are recorded separately in
[the portable-core evidence](../analysis/portable-core-platform-validation.json).

```powershell
cmake --build build/native-recovery --config Release --target FS15Core
python scripts/check_scene_camera.py --dll build/native-recovery/Release/FS15Core.dll
cmake --build build/portable-core --config Release
ctest --test-dir build/portable-core -C Release --output-on-failure
```

The later [projection component](SCENE_PROJECTION_RECOVERY.md) recovers
matrix construction, full projective products and Camera basis conditioning.
Known frustum/spatial queries are covered by the
[visibility component](SCENE_VISIBILITY_RECOVERY.md). Complete view/GPU
preparation, renderer consumption,
recursive derived cloning, the original XML tokenizer/complete loader, other
derived nodes, full engine startup and Switch execution remain open. Storing
the projection parameters does not establish recovery of projection rendering.
ReFarm continues to use its existing authored free-camera controls; this
milestone validates native Camera services in the recovered core.

The later [scene-copy component](SCENE_CLONE_RECOVERY.md) recovers the shared
TRS/attribute/property copy and Camera single-record clone wrapper. Full
recursive object/resource remapping remains open.
