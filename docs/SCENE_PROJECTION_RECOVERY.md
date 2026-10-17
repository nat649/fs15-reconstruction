# Recovered Camera projection math

Four additional GE6 native entries now compile in `FS15Core` and the asset-free
C++17 `FS15PortableCore`. These are the original projection/basis algorithms
reconstructed from the pinned x64 image and inspected instructions. They are
not the complete Camera-to-renderer implementation.

| Original entry | Recovered behavior |
|---|---|
| `0x140042800` | Off-axis perspective matrix from left/right, bottom/top and near/far bounds |
| `0x140042930` | Off-axis orthographic matrix from the same six bounds |
| `0x14001b060` | Full column-major projective 4x4 multiplication |
| `0x1400b2130` | In-place Camera basis conditioning with the native reciprocal-square-root seed/refinement and threshold comparisons |

Source: [implementation](../src/core/scene_projection.cpp),
[bounded interface](../src/core/scene_projection.h),
[portable fixtures](../tests/scene_projection.cpp) and
[native comparison](../scripts/check_scene_projection.py).

## Recovered behavior and limits

Projection matrices retain native float operation grouping, sign-bit negation
and clip depth **-1 to +1**. Reversed extents and negative finite distances
remain valid inputs to the recovered formulas. No Direct3D depth conversion
or replacement projection convention is added.

The full projective product includes all fourth-row terms. The previously
recovered affine product deliberately omits those terms, so it cannot replace
this operation for projection matrices. Native addition order is retained for
each output row. The authored interface stages output and supports aliased
input/output; native alias behavior is outside the compared contract.

Camera basis conditioning retains seven non-basis lanes byte for byte. It
normalizes the first basis vector, conditionally removes its projection from
the second, normalizes the second, conditionally removes both projections from
the third, then normalizes the third. The original tests use the interval
`[0.99999f, 1.00001f]` for both inverse lengths **and dot products**. This unusual
condition is retained; replacing it with a tolerance around zero changes
native results. A dot product equal to one can leave a nonorthogonal basis.
Degenerate finite vectors also retain the recovered seed behavior.

Null/length/finite checks, rejection of zero projection spans, staged output
and finite-result rejection are authored guards. Rejection leaves output
unchanged. They do not establish behavior for every original input or pointer.

## Differential evidence

The isolated reference executes the four original bodies from the pinned local
image, with the original reciprocal-seed initializer. It never executes the
game entry point. Complete outputs, padding/spare lanes and signed-zero cases
are compared byte for byte:

- 4,000 perspective matrices and 4,000 orthographic matrices;
- 5,000 full projective products;
- 5,000 complete conditioned Camera records;
- 18 projections using six original I3D Camera parameter sets, at three
  aspect ratios;
- 18 complete projection/view/inverse/frustum chains and 144 point predicates.

The original resources are both base maps, Deutz Agrostar 6.61 and New Holland
T8.320. Their hashes and result digest appear in the
[source-pinned report](../analysis/scene-projection-validation.json). Original
I3D resources and raw pseudocode/instruction exports remain ignored locally.
Hash-verified cached copies supply vehicle inputs if the installation drive is
unavailable; the installation inventory is not rewritten.

The FOV-to-extents calculation, aspect ratios and world transforms in these
chains are authored fixture inputs. The comparison establishes the recovered
matrix/basis functions together with existing affine inversion and frustum
extraction. It does not claim recovery of the entire `0x1400b3e20` view path.
The portable fixtures additionally cover projected world-point visibility,
native near/far depth, reversed bounds, aliased products, native dot-condition
behavior and atomic rejection of malformed/nonfinite/overflow outputs.

## Remaining work

The later [view component](SCENE_VIEW_RECOVERY.md) recovers the supported
Camera view/stereo preparation and `0x14012cca0` shader-block packing through
explicit owned map/unmap transport. Render-view allocation, other view
variants, real GPU buffer/backend operations and derived render consumers,
dynamic shadow-volume construction, native startup and renderer/resource
ownership remain open. ReFarm's renderer has not been substituted for these
native components. No complete GE6 or Switch `.nro` build is established.

All 22 portable tests pass on Windows x64, Linux x64 and ARM64 Linux under
QEMU. All 27 native differential suites and the separate original mesh-cipher
check pass on Windows. CPU platform evidence is tracked separately in the
[portability status](PORTABILITY_STATUS.md).

## Reproduce locally

```powershell
cmake --build build/native-recovery --config Release --target FS15Core
python scripts/check_scene_projection.py --dll build/native-recovery/Release/FS15Core.dll
cmake -S . -B build/portable-core -DFS15_PORTABLE_CORE_ONLY=ON
cmake --build build/portable-core --config Release
ctest --test-dir build/portable-core -C Release --output-on-failure
```

Native comparisons require the user's pinned original image and local
reference resources. The portable build and tests require no original inputs.
