# Common I3D node attributes and pending physics metadata

The recovered portable core now applies the common GE6 I3D attributes to a
constructed TRS node, using the recovered rename, TRS rebuild, state propagation
and property lifetime components. This is a further part of native scene loading;
it does not construct a complete map, renderer or physics world.

## Native paths and provenance

| Native entry | Reconstructed bounded behavior |
|---|---|
| `0x140194460` | Common node attributes, ordered body/joint metadata creation and source-ID registration request |
| `0x14018c100` | Common-attribute wrapper with an initially empty scratch name |
| `0x140299930` | Exact, first-match XML attribute lookup, including a null first value |
| `0x140127910` | Rigid-body property construction, preserving opaque bytes |
| `0x140127d50`, `0x1401279a0` | Detached rigid-body destructor and deleting wrapper |
| `0x1401497e0` | Joint property construction, preserving opaque bytes |
| `0x1401498b0`, `0x140149880` | Detached joint destructor and deleting wrapper |
| `0x14009e5b0`, `0x14009e5f0` | Property flags and dirty-mask updates, with explicit queue dispatch service |
| `0x140129720` | Minimum-clamped density and actor-dependent dirty marking |

The C++ implementation is reconstructed from the pinned native image and local
instruction/pseudocode analysis. Stable native vtable addresses are identity
tokens; the portable code never calls them. Allocation, object registration,
decoded XML attribute transport and CRT integer conversion are authored service
boundaries. The native XML tokenizer, source-ID tree implementation and platform
startup are not replaced with claims of recovered code.

The `0x88`-byte rigid-body and `0x90`-byte joint records retain constructor defaults
and untouched opaque bytes. They hold pending scene metadata with no live PhysX
manager, actor or constraint. Known detached records can now be replaced and
released through the recovered node lifetime chain. Active backend/registered
observer teardown rejects before mutation.

## Preserved loading behavior

- Names use the first exact matching attribute; the wrapper's absent name is
  empty. Rename allocates even for an empty string.
- Translation, rotation and scale use three native scalar reads, each advancing
  the cursor by its consumed length plus one. This differs from the animation
  vector splitter: `1  2 3` becomes `(1,2,3)` here. Rotation converts degrees by
  dividing by float `180` and multiplying by float pi before rebuilding the cache.
- Missing TRS attributes restore `(0,0,0)`, `(0,0,0)` and `(1,1,1)`. Visibility
  defaults to true; only case-insensitive `false` is false. Occlusion defaults false.
- Static precedes dynamic, which precedes kinematic. Compound precedes
  compoundChild. Collision/trigger masks, material/damping values, density and
  solver iterations retain their original write/dirty-bit ordering. Density
  clamps to `0.0001f`; an absent actor prevents its density dirty bit being set.
- A joint replaces a body created earlier in the same call. Projection values
  require projection to be enabled. The registration switch also enables drive
  values without an axis-drive attribute, matching the native control flow.
  Joint limit angles are stored without the node-rotation degree conversion.
- Clip distances clamp at zero; masks use original CRT decimal-prefix conversion.
  A nonnull nodeId precedes refId, even when registration is disabled and only
  conversion occurs. Registration transport retains first-ID-wins semantics.

The API requires valid supported node/attribute/pointee storage. Added bounds,
finite-number and exponent-work guards reject undefined native inputs. Prefix
mutations can remain after a failed callback or allocation; services cannot
throw or reenter. These guards are authored and are tested separately from
native behavior.

## Validation

[Source-pinned native evidence](../analysis/scene-xml-validation.json) records:

- 800 complete property constructors, 1,600 flag changes and 400 density updates;
- 268 detached nondeleting destructors and 532 deleting wrappers;
- 8,172 attribute lookups and 2,043 common node attribute/lifetime comparisons;
- 1,403 common records sampled from original `kroneSwadro2000.i3d`, `map01.i3d`
  and `map02.i3d`, alongside 640 synthetic attribute sets with varied switches;
- 214 final rigid-body and 130 final joint records, plus 1,699 default properties;
- native source-ID tree contents checked after every load, including duplicates;
- complete owned-storage cleanup on both sides.

The original code executes unchanged in a separate process with a private heap,
FLS and C-locale fixture. No game entry point or running game is touched. Record
bytes are compared after pointer normalization; TRS/libm float regions use the
existing scaled tolerance `8e-6`. The maps' geometry, materials and assets are not
copied into the public repository. Sampling attributes of a Shape/Light/Camera
tests their common base metadata on a TRS fixture, not those derived render types.

Trigger metadata is compared before cleanup. The fixture then clears the
unactivated trigger bit on both sides, explicitly excluding the unrecovered
native trigger-release branch. No collision simulation is validated by this work.

The asset-free `portable_scene_xml` check covers defaults and priorities,
TRS grammar, flags, joint limits, registration-controlled drives, allocation
failure cleanup and unsupported backend/trigger boundaries. All eighteen portable
checks pass on Windows x64, Linux x64 and ARM64 Linux under QEMU. Exact tested
source revision, CI job IDs and scope are recorded in [platform evidence](../analysis/portable-core-platform-validation.json).

```powershell
cmake --build build/native-recovery --config Release --target FS15Core
python scripts/check_scene_xml.py --dll build/native-recovery/Release/FS15Core.dll
cmake --build build/portable-core --config Release
ctest --test-dir build/portable-core -C Release --output-on-failure
```

## Next loading boundaries

Native child-vector growth and separately allocated hierarchy operations are now
covered by the [owned scene component](SCENE_OWNED_RECOVERY.md).
Other derived-node callbacks and complete loader ownership remain open. The
[group callbacks](SCENE_GROUP_RECOVERY.md) now connect TransformGroup/LOD
construction, attachment and source-ID-resolved animation/resource paths.
Shape decoding, native materials/render resources,
active physics/observer services, triggers, complete startup and a linked portable
engine remain open. The future Switch `.nro` target remains deferred.
