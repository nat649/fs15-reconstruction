# Native Light recovery and derived scene dispatch

Nine additional bounded native paths now compile in the portable C++17 core.
They recover Light records, cone/dirty state, decoded I3D loading, pending-only
lifetime and clone dispatch. The scene walker now distinguishes Light virtual
behavior from ordinary TRS behavior. This does not implement spatial light
registration, render/GPU resources or the original deferred-update consumer.
The later [spatial component](SCENE_SPATIAL_RECOVERY.md) now recovers grid
registration/removal, active destruction and LOD/Light pending consumption;
the narrower fixture described here remains independently reproducible.

| Entry | Recovered path |
|---|---|
| `0x1400d9bc0` | Construct the `0x200`-byte Light record |
| `0x1400d9de0` | Deleting/nondeleting destructor wrapper |
| `0x1400d9e10` | Pending removal and inherited lifetime, without active spatial/GPU ownership |
| `0x1400d9f30` | Light clone wrapper followed by common node copy |
| `0x1400da110` | Base scene traversal followed by Light-specific dirty/queue effects |
| `0x1400da1b0` | Cached inherited LOD token and recursive marker override |
| `0x1400da210` | Cone factors, drop-off clamp, dirty flags and queue request |
| `0x140087cf0` | Append the Light to its manager's pending pointer vector |
| `0x14018c770` | Decoded I3D Light begin callback |

Constructors, common XML attributes, TRS/state propagation, supported property
copying, pointer-vector allocation, LOD services and recursive remap/clone
storage are reused. They are not counted twice. See
[source-pinned evidence](../analysis/scene-light-validation.json).

## Recovered record and metadata

The Light uses typed TRS class `0x17`, type flags `0x2072` and identity table
`0x1404c1d98`. The eight inspected slots inherit ordinary link/unlink, observer
rescan, inherited LOD lookup and TRS matrix assignment. State and marker slots
are Light-specific, as is the deleting wrapper. Across five observed scene
tables, 40 slots resolve to 17 distinct targets; the five earlier discovery
records remain retained and this audit creates no new functions.

| Offset | Observed field |
|---|---|
| `+0x170` | Spatial packed token, untouched by construction; identified by later grid recovery |
| `+0x174` | Type: directional `0`, point `1`, spot `2` |
| `+0x178/+0x184` | Diffuse/specular colors, three floats each |
| `+0x190/+0x194/+0x198` | Range, drop-off and cone angle in degrees |
| `+0x19c/+0x1a0` | Outer/inner cone cosine factors |
| `+0x1a4..1b0` | Depth-map bias, slope bias/clamp and resolution |
| `+0x1b4..1c4` | Shadow far/extrusion distances, perspective parameter, splits and split parameter |
| `+0x1c8/+0x1cc` | Decay and affected-light masks |
| `+0x1d0/+0x1d4` | Light option flags and cached LOD token |
| `+0x1d8..1f0` | Sphere subobject table, opaque word, center and float radius; named by later spatial recovery |
| `+0x1f4/+0x1f8` | Opaque word and render-resource pointer |

The constructor leaves opaque bytes untouched, initializes spatial links and
render resources to null, replaces decay zero with one and initializes masks
and LOD token to `0xffffffff`. Shadow splits clamp to `1..4` for directional
lights and become one for other types. Option bits preserve diffuse/specular,
shadow-map and perspective arguments in their native positions.

Cone calculation retains float operation order:
`angle = ((coneAngle * 0.5f) * piFloat) / 180.0f`.
It stores `cos(angle)`, clamps drop-off to `0..5`, then stores
`cos(angle - (angle / 5) * dropOff)`. Negative zero is retained by the clamp.
The math backend is the platform float cosine, not a recovery of the original
CRT trigonometric implementation. Finite-input guards are authored additions.

## Virtual updates, queues and ownership

After base traversal, the Light state override uses the **original input mask**,
including bits suppressed by base propagation. Mask `0xc` sets dirty bit
`0x400`; bit `4` also invalidates the cached LOD token. Mask `3` sets `0x500`.
A visible or already registered Light requests a pending update only when its
`0x800` queued bit is clear. The marker override updates a changed cached token,
sets `0x400` and requests the update before invoking the base recursive marker.

The new authored `scene_derived.h` transport supports matrix/TRS/LOD/Light
dispatch. It stages Light token fields separately from base records and emits
separate LOD and Light queue requests in each manager's original order. Queue
capacity and node/token changes are checked before commit. The previous APIs
retain their original supported-kind boundary; callers opt into derived
transport explicitly. World/matrix transport accepts the inherited Light paths.
Marker traversal for LOD registration and teardown does not inspect already
dying ancestors or property objects.

The Light context represents only the observed pending vector, not the complete
native manager. Original pending storage is at `manager + 0xe8`; the oracle maps
that subview explicitly. The constructor starts hidden and normally does not
append. Loading/attachment, inherited visibility and later transforms trigger
the recovered queue behavior.

Pending-only destruction removes the first matching pending entry using a
swap with the last entry. Native queued flag/spare-slot bytes are retained.
It changes the spatial subobject's table, then runs inherited TRS cleanup.
Active spatial registration or render resources reject before that path;
those resource lifetimes remain open. Callback/allocation failures can retain
native-order prefixes, and completed allocations remain caller-owned.

## XML and clone details

Attribute searches retain the first exact-name match from attribute zero.
Missing/null first `name` ignores the begin callback. Light type values and
boolean `true` checks are case-insensitive; a nonnull boolean value other than
`true` is false. Diffuse/specular default to `(1,1,1)`, with emission enabled.
Shadow-map casting defaults false. Range defaults one, decay two, cone angle
60 and drop-off four. Cone/drop attributes apply only to spot lights.

Shadow settings parse only when shadow-map casting is enabled. Defaults are
depth bias/slope bias `0.005`, slope clamp `0.02`, resolution `512`, far distance
`80`, extrusion distance `200`, perspective parameter `5`, one split and split
parameter `0.4`. Color triples use native scalar-cursor-plus-one parsing.
Decay/split counts use unsigned digit-prefix accumulation with wrapping.
Depth resolution uses the original signed-decimal CRT seam; affected masks use
the original unsigned CRT seam with base zero, including its overflow behavior.
These CRT services are not newly recovered parsers.

The callback constructs and retains the Light, reads affected masks, applies
common attributes with physics `0`, source IDs `1`, joint `0`, then appends it
to the current parent. Other physics/joint attributes therefore do not create
those property types on a Light. End uses the existing non-shape callback.

Light cloning copies constructor parameters, then copies the source option
flags and runs the recovered common node copy. Affected masks reset to the
constructor default rather than copying the source mask. Recursive clone
dispatch now accepts Light at its original priority between Shape and reference
resources. The copied subtree uses real Light state/queue behavior; it is not
reinterpreted as a TRS-only placeholder.

## Evidence and remaining work

The unchanged pinned original image executes in a private Windows fixture, with
explicit platform/locale seams. No game entry point or live process is altered.
Comparisons pass for:

- 300 complete constructors, 300 direct virtual/post states and 300 nondeleting destructors;
- 46 mixed forest lifetimes, 1,104 complete forest/LOD-manager states and 6,460 node records;
- 166 Light begin callbacks, including 80 ignored missing/null names, plus 80 group and 40 Camera begins;
- 138 each of cone dirty, virtual state, inherited marker, TRS, unlink/relink and LOD-flush operations;
- 46 staged marker operations, alongside direct native-order subtree markers;
- 206 recursively cloned nodes, including nested Lights and LOD/Camera siblings;
- six complete original Light subtrees from Map01, Map02, Deutz Agrostar and
  New Holland T8, loaded with original attributes before controlled update tests.

Node/property/attribute bytes, names, child vectors and both pending-vector
capacities/spares/order are compared. Destruction also compares retained
pending spare bytes after objects are released. Pointer identities are
normalized; direct cone tolerance is `1e-6` and forest transform/libm tolerance
is `1e-5`. The oracle uses an **authored consumer fixture** that clears queued
flags/list entries between selected tests. It does not execute or validate the
original renderer update consumer.

The asset-free test covers conditional XML parsing, nested-Light LOD tokens,
pending deduplication, recursive copy, queue-capacity atomicity and cleanup
across 24 allocation-failure attempts. These platform/container guards and
callback transport are authored. All 20 portable tests pass on Windows x64, Linux x64 and ARM64 Linux
under QEMU. All 25 native differential suites pass on Windows. CPU/platform
results are recorded separately in [platform evidence](../analysis/portable-core-platform-validation.json).

```powershell
cmake --build build/native-recovery --config Release --target FS15Core
python scripts/check_scene_light.py --dll build/native-recovery/Release/FS15Core.dll
cmake --build build/portable-core --config Release
ctest --test-dir build/portable-core -C Release --output-on-failure
```

Spatial registration/removal and LOD/Light deferred consumption are now
covered by the separate [spatial component](SCENE_SPATIAL_RECOVERY.md). Spatial
visibility/frustum queries, Shape/audio dispatch, GPU lighting/shadows,
Shape/reference cloning, skin/animation finalization, full engine startup and
Switch execution remain open. ReFarm's authored renderer is unchanged by this
milestone; metadata/state parity does not establish original-GE6 lighting output.
