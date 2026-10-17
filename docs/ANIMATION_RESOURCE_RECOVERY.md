# Recovered binary animation resources

Seven additional native entries now connect decoded `.i3d.anim` resources to
the recovered CharacterSet ownership and playback paths. The portable core
loads a real local game resource with the same resulting keys, clips, node
binding and sampled poses as unchanged GE6 code. Filesystem/archive access,
complete scene/render startup remain open. The
[XML callbacks](ANIMATION_XML_RECOVERY.md) are now recovered separately.

| Native entry | Recovered behavior |
|---|---|
| `0x1402805b0` | Build a WXYZ quaternion from radians using original Z/Y/X matrix rotations |
| `0x1401cb5c0` | Allocate and pack variable-size translation/quaternion/scale/visibility keys |
| `0x1401bd190` | Append strictly increasing, consistently controlled keys; retain rejected-key ownership |
| `0x1401bd2f0` | Generate the 64-entry acceleration lookup and time scale |
| `0x1401768b0` | Append a channel with native vector growth behavior |
| `0x1401751e0` | Combine channel control flags and register a clip in shared data |
| `0x1401ca840` | Parse the binary animation payload, build resources, resolve node IDs and bind CharacterSets |

The version/file wrapper is authored around the payload entry. It accepts the
observed big-endian version 2/3 headers; original file-opening routine
`0x1401995a0` is not counted as recovered. Node-ID resolution, allocation,
object registry/IDs, synchronization and result callbacks are authored transport
around the recovered algorithms. Portable vtable values are identity tokens.

## Format and construction behavior

The payload starts with a big-endian group count. Each group contains a counted
name padded to four bytes and its clips. Each clip contains a padded name,
float duration and channel count. Channels contain node ID, key count, then
time/control flags and optional three-float translation, rotation and scale.
Binary rotations are radians; the native inline XML path separately converts
degrees and remains unrecovered. The visibility state is carried in bit 15.
Counted temporary names pass through a C-string constructor, retaining the
original first-NUL behavior rather than silently storing embedded NUL tails.

Keys retain their native eight-byte header and variable-size payload. Packed
rotation stores signed 16-bit XYZW components with sign-dependent rounding of
`component * 32767`; it does not add a new clamp. Channel insertion rejects
equal/decreasing times and changes to the low four control bits. Those native
rejections leave the supplied key owned by the caller and report diagnostics.
The portable interface reports the same conditions without invoking a logger.

Lookup construction uses the key span, `span / 64` bucket spacing and
`64 / span` index scale. Empty/single-time channels only reset the first lookup
slot and scale; the remaining opaque slots are preserved. Empty serialized
channels are discarded by the payload reader. Each nonempty channel records
its first-encounter node ordinal in the newly named `node_index` field. Repeated
node IDs retain their ordinal; serialized channel ordering is preserved.

Clips and keys enter the recovered shared ownership chain. A later group that
binds the same nodes releases their old CharacterSet references and can delete
the earlier set/data/clip/key graph. This lifetime behavior is tested alongside
the final loaded records. Zero-node groups have no node references; their
explicit cleanup belongs to the host. Missing-node groups are skipped and
destroyed through the known lifetime paths.

## Original-code evidence

The [source-pinned report](../analysis/animation-resources-validation.json)
records:

- 4,000 Euler/quaternion comparisons and 4,000 complete packed-key allocations.
- 5,440 successive complete channel insertion/finalization records, including
  allocation capacity and preserved opaque lookup fields.
- 440 non-monotonic/mismatched-control diagnostics, with rejected-key cleanup
  performed separately by each fixture owner.
- 90 synthetic binary files plus one installed original resource, comparing
  final registries, complete surviving records, vector capacity, key payloads,
  default poses, bound node order and nested destruction.
- 637 resource-to-pose updates after loading, including negative/out-of-range
  looping seeks. All 10,437 compared node float regions match bit for bit in
  the recorded Windows run.

The real resource is `poettingerTop1252.i3d.anim`, SHA-256
`08aaf88f93f49775083624b141ca53e8c56464d6595dcf8adef13de120aa1b75`:
7,028 bytes, one group, one clip, 56 channels and 326 keys. All bytes are
consumed by both the independent fixture inventory and recovered reader. The
original payload reader and evaluator run in an isolated pinned-image process;
the installed game is not patched or launched. The original resource remains
local. Only aggregate evidence and reconstruction/tooling source are published.

Native temporary lookup trees and platform allocation internals are not claimed
as reconstructed by this reader. Their final graph effects are checked through
authored resolver/registry interfaces. Original-code fixtures use successfully
resolved TRS nodes; missing-node handling and allocation-failure guards are
checked separately by the asset-free test. Original CRT math remains a platform
library boundary; the numeric gate is `8e-6`, with recorded maximum error zero.

## Portable integration and guards

The `portable_animation_resources` test loads an authored binary fixture,
resolves its clip, seeks it, evaluates it and applies translation/visibility
through recovered scene services. It also checks later-group rebinding,
missing-node cleanup, every truncation of the fixture, unsupported versions,
allocation failure cleanup and preserved empty-channel lookup slots. See the
[platform evidence](../analysis/portable-core-platform-validation.json) for the
tested revision and platform results.
All twenty-two asset-free core tests pass on Windows x64, Linux x64 and ARM64 Linux
under QEMU. Original-code comparisons remain Windows-only; no Switch build or
complete engine execution is established by these checks.

Preflight count, string, finite-value, monotonicity and node-ordinal guards are
authored additions. Invalid/truncated inputs fail before engine allocation.
Allocation or host-service failures can retain earlier completed groups or a
partially bound group; the callback reports that boundary for caller cleanup.
Callbacks must be serialized, non-reentrant and must not throw. A borrowed
result can be invalidated by later rebinding unless the host retains a reference.

Continue with original XML tokenization/complete dispatch, I3D/file/archive integration,
update scheduling, skeleton/skin binding and renderer consumption. ReFarm's
existing resource/animation host remains separate. This component does not
establish a complete portable GE6 or Switch executable.

```powershell
python scripts/check_animation_resources.py --dll build/native-recovery/Release/FS15Core.dll
# Optional local input; an existing local inventory selects the base-game file.
python scripts/check_animation_resources.py --dll build/native-recovery/Release/FS15Core.dll --anim 'path/to/original.i3d.anim'
ctest --test-dir build/portable-core -C Release --output-on-failure
```
