# Native visibility predicates and spatial queries

Eleven additional native paths now compile in the portable C++17 core. They
extract six normalized clip planes from a supplied matrix, test fixed/dynamic
plane volumes, filter spatial entries and query the recovered grid. Original
functions run unchanged in the differential fixture. No render callback is
replaced by a fabricated visibility predicate in these comparisons.

## Native paths and layout

| Entry | Bounded recovered behavior |
|---|---|
| `0x14006d6c0` | Normalize all four plane coefficients using the native reciprocal-square-root seed/refinement algorithm |
| `0x140289cc0` | Extract and normalize six planes from a column-major 4x4 matrix |
| `0x140289e90`, `0x140289f00`, `0x140289f80` | Fixed six-plane point, sphere and box predicates |
| `0x1401af460`, `0x1401af4e0`, `0x1401af560` | Dynamic plane-array point, sphere and box predicates |
| `0x1400d6580` | LOD, clip-distance and sphere-volume entry filter |
| `0x1400d7250` | Volume query over depth-first grid cells |
| `0x1400d7460` | Point query over cells and their registered spheres |

The original fixed-volume identity table is `0x1404c7cc0`. Its inspected prefix
contains the table followed by six `(x,y,z,d)` planes, a total of `0x68` bytes.
The actual render-view object contains more fields; those are not claimed as
recovered by this prefix. The dynamic-volume table is `0x1404c7da8`, with plane
pointer at `+8` and 32-bit plane count at `+0x10`. Its `0x18`-byte prefix is a
borrowed view, not its complete owned class. Native identity pointers are never
executed by the reconstructed core.

The existing recovered 512-entry reciprocal seed initialization and two double
refinements are shared by animation, Camera and plane normalization through
`scene_rsqrt.h`. Each caller retains its own squared-length float grouping.
The initialization is reused rather than counted as another recovered path.
Plane normalization multiplies the fourth coefficient by the same inverse
length; finite zero-normal planes retain the original seed behavior.

## Plane and volume behavior

Extraction uses these row combinations in native order: row 4 minus row 1,
row 1 plus row 4, row 2 plus row 4, row 4 minus row 2, row 4 minus row 3, and
row 3 plus row 4. Each resulting plane is normalized. The record's identity
table is preserved. A staged output and finite/size rejection are authored
guards; no overlapping input/output contract is provided.

Point predicates reject a plane distance of zero or less. Sphere predicates
reject a distance at or below negative radius. Box predicates add the
half-extent support to the grouped position dot product, then reject zero or
less. Tangency therefore rejects at the outer boundary. Dynamic plane arrays
with zero planes accept. Arithmetic grouping follows the inspected instructions,
including differences between fixed/dynamic X/Y operand order. Negative finite
radius/extent inputs are not silently clamped.

## Query ordering and side effects

The volume query forms a box for each visited cell using its X/Z bounds and
the grid's common Y bounds. A rejected cell jumps to its depth-first skip
index, bypassing all descendants. Accepted cells visit live rows in storage
order. Each row tests its LOD token first, unless the token is `0xffffffff`.
That test can refresh the shared LOD cache even if a later distance or volume
test rejects the object.

For viewer distance `d`, sphere radius `r`, scale factor `f` and the two stored
clip values `c0/c1`, the native rejection tests are exactly:

```text
f * c0 <= d - r
r + d < f * c1
```

Only a surviving row runs the sphere-volume test. The distance uses platform
`sqrtf` after native-order float squared-distance accumulation. Original CRT
startup and exceptional math-library paths are not recovered by this backend.

The point query uses inclusive X/Z cell bounds, then LOD selection and inclusive
point-in-sphere squared distance. It does not apply the volume query's clip
distances. Both queries preserve native cell/row order, count every match and
write only the first `capacity` owner-token pointers. Capacity zero is a valid
count-only query. Neither query allocates, sorts or deduplicates results. The
outputs identify the owner token, not the address of a grid entry or a complete
scene object.

Caller-owned storage, finite input checks, a 65,535 dynamic-plane bound and
forward-skip validation are authored boundaries. A failure can leave written
output/cache prefixes; these APIs do not promise a manager transaction. Native
render masks and Shape/audio-specific acceptance occur in later consumers and
are not implemented as guessed extra filters here.

## Evidence

The [source-pinned report](../analysis/scene-visibility-validation.json) records:

- 4,000 bit-exact plane normalizations, including a finite zero normal;
- 1,200 bit-exact complete frustum extractions, including the identity/zero cases;
- 57,600 native point/sphere/box predicates across fixed and dynamic volumes;
- 48 explicit tangency checks at and around the single-precision boundary;
- 4,000 complete entry filters and 8,096 complete LOD cache allocation states,
  including cached stamps and low-16-bit stamp wrap cases;
- 2,696 volume queries and 2,696 point queries, totaling 89,564 matches;
- 3,363 queries with truncated output, while preserving total counts, stored
  prefixes, untouched output spare bytes and native result order;
- 32 complete synthetic grid lifetimes with unchanged grid/header/entry storage;
- 18 loaded Light/LOD forest lifetimes and 648 complete loaded forest/cache
  states, including six original Light subtrees from both maps and two tractors.

Synthetic matrices, plane arrays, allocator/world transport and guards are
authored. The original matrix extraction, volume predicates, row filters and
grid queries execute their real native bodies. Fixed/dynamic virtual targets
were checked against their original table slots. Full grid/header/entry storage
is checked for query immutability. Pointer identities are normalized for result
and cache comparisons. Plane coefficients and synthetic LOD cache bytes match
bit for bit on Windows; the existing loaded-forest check retains transform/libm
tolerance `1e-5`.

The portable test covers exact boundary rejection, an empty dynamic volume,
root sentinels, ordered output truncation/count-only queries, point-in-sphere
inclusion, cache refresh before rejection and malformed/nonfinite guards.
All 23 portable tests pass on Windows x64, Linux x64 and ARM64 Linux under
QEMU. All 28 native differential suites pass on Windows. Platform evidence
is recorded separately in
[CPU checks](../analysis/portable-core-platform-validation.json).

## Local original-resource inputs

The inventory remains the record of the installed game. When that installation
is temporarily unavailable, comparison scripts can reuse six original resources
from the existing ignored local package. `local_reference_assets.py` requires
the previously recorded SHA-256 for every fallback file; a changed/unpinned
cached file rejects. It does not alter the inventory, prepare/download resources
or publish game data. Four authored fixtures check selection, hash mismatch,
missing version pins and path containment.

```powershell
cmake --build build/native-recovery --config Release --target FS15Core
python scripts/check_scene_visibility.py --dll build/native-recovery/Release/FS15Core.dll
cmake --build build/portable-core --config Release
ctest --test-dir build/portable-core -C Release --output-on-failure
```

The later [projection component](SCENE_PROJECTION_RECOVERY.md) adds native
projection matrices, projective products and Camera basis conditioning.
Complete Camera view/GPU preparation, construction of dynamic shadow clip
volumes, Shape/audio consumers, GPU resources, full scene/render startup and
Switch execution remain open. ReFarm's renderer has not been changed by this
component milestone. These checks establish query behavior, not rendered-frame
parity or a complete rebuilt GE6 engine.
