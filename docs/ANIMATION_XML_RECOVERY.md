# Recovered XML animation callbacks

Nine additional native entries recover the GE6 decimal/vector readers and the
seven callbacks that construct animation sets, clips, channels and keys from
decoded I3D XML attributes. They feed the existing recovered ownership and
playback services. The original XML tokenizer, complete scene/file integration,
update scheduling, skinning and renderer startup remain open.

| Native entry | Recovered behavior |
|---|---|
| `0x140026e60` | Parse a scalar float and return the consumed input cursor |
| `0x140026850` | Split ASCII-space-delimited float vectors, retaining native token behavior |
| `0x140199890` | Start/replace a pending CharacterSet and reset its node-ordinal bookkeeping |
| `0x140199970` | Resolve accumulated node IDs, bind the set and transfer ownership |
| `0x140199d70` | Construct a named clip with its converted duration |
| `0x140199e90` | Register a completed clip in shared set data |
| `0x140199ec0` | Start a channel and update its source node ID when supplied |
| `0x140199fa0` | Finalize a nonempty channel, assign its first-encounter node ordinal and register it; destroy empty channels |
| `0x14019a100` | Decode key time/TRS/visibility/interpolation attributes, pack the key and attempt ordered insertion |

## Native parsing rules retained

The custom float reader rounds to binary32 after each digit. Integer and
fractional accumulation use their observed double intermediates; exponent
scaling repeatedly rounds a binary32 power-of-ten factor. It skips ASCII
spaces, accepts a leading minus and recognizes lowercase `e`, including an
exponent sign. Leading plus, tabs and uppercase `E` are not silently accepted
as conventional numeric syntax. The scalar API returns the original prefix
cursor and preserves signed zero and native nonfinite results.

Vector parsing splits at every ASCII space rather than collapsing whitespace.
For example, a three-value read of `1  2 3` yields `1, 2, 2`. The recovered
callback retains this behavior. Excessive exponent work, missing terminators,
short vectors and nonfinite animation payloads are rejected by authored guards.
Short vectors otherwise reach uninitialized native stack fields; that undefined
behavior is not fabricated or called in the original-code fixture.
Missing clip duration is rejected and absent names use an empty host string;
those guards do not validate native null-string/invalid-parameter paths.

Attributes use exact case-sensitive names and their first matching occurrence.
A key without a nonnull time is a native no-op. XML rotations convert degrees
using `(degrees / 180.0f) * pi_float` before the recovered Euler/quaternion and
packed-key routines; binary `.i3d.anim` rotations are already radians.

The six interpolation attributes retain independent incoming/outgoing flags:
`iptin/iptout`, `iprin/iprout`, `ipsin/ipsout`. Only exact lowercase `linear`
suppresses the corresponding smooth flag; absent or different values keep it.
Visibility defaults to true, even when the key does not control visibility.
When present, only a case-insensitive `false` makes visibility false; `0` remains
true. All of these choices are checked against original callback output bytes.

## Ownership and host transport

`animation_xml.h` supplies an authored opaque event context and bounded decoded
attribute views. XML tokenization and the scene node-ID map are host services.
The native callbacks' static CRT `atof`/`strtol` seams remain explicit duration
and node-ID conversion callbacks; their CRT implementations are not counted as
recovered. Channel/clip storage is shared with the binary resource component.
Final native-layout records, key buffers, capacities, flags, node order and
object-registry effects are compared; the opaque event context is not presented
as the original complete I3D loader object.

Completed sets are owned by node references. Zero-node results require explicit
host ownership/cleanup. Native rejection of a non-increasing key time or changed
control bits leaves the newly allocated key orphaned. The reconstructed XML
context retains those allocations until explicit context destruction; the
reference fixture reclaims its original orphans separately. This preserves
the observed boundary without describing native rejected-key retention as
ordinary channel-owned cleanup.

Failed allocation or binding can retain preceding mutations. Pending resources
are cleaned by context destruction; a partially bound set is handed back through
the result callback for node/host cleanup. Calls must be serialized and correctly
nested, and host callbacks must not throw or reenter. Arbitrary callback effects,
concurrent scheduling and original exception/unwind behavior remain separate.

## Evidence

The [source-pinned native comparison](../analysis/animation-xml-validation.json)
records:

- 12,015 scalar results/cursors and 6,000 complete vector output frames match
  the original machine code bit for bit, including native parsing quirks.
- 7,189 successive XML callback states and 93 completed groups across 60
  synthetic event streams and three original I3D files.
- Complete channel/key records are checked after each relevant callback, not
  only sampled positions or accepted-key counts.
- 179 rejected XML key allocations retain the observed orphan boundary and
  are explicitly reclaimed by their respective fixture owners.
- All 63 complete lifetimes finish with empty object registries and no owned
  resource/orphan allocations beyond the explicitly retained registry sentinel.

The original local files remain excluded from Git:

| Resource | Groups / clips | Channels | Keys |
|---|---:|---:|---:|
| `kroneSwadro2000.i3d` | 1 / 1 | 96 | 582 |
| `kuhnGA8521.i3d` | 1 / 1 | 28 | 170 |
| `poettingerTop1252.i3d` | 1 / 1 | 56 | 326 |

Their hashes and aggregate counts are in the evidence. This establishes
callback construction/binding on decoded original XML, not a recovered XML
tokenizer, a live rendered animation or a complete engine session.

Original callbacks run in a private process mapping with unchanged code bytes.
The oracle initializes a private CRT FLS slot/thread record and the pinned
C-locale views. Four additional inspected kernel imports are bound alongside
the existing five private-heap/critical-section seams. This avoids borrowing
captured per-thread pointers and does not claim recovered CRT initialization.
The installed game and its thread state are not modified.

The asset-free `portable_animation_xml` CTest independently exercises XML events
through recovered playback and scene updates, numeric quirks, visibility/easing,
pending cleanup, rejected-key retention, invalid nesting, short vectors and
allocation failures through partial binding. See the tested revision and CPU
scope in [platform evidence](../analysis/portable-core-platform-validation.json).
All thirteen asset-free core tests pass on Windows x64, Linux x64 and ARM64 Linux
under QEMU. Original-code comparisons remain Windows-only; no complete engine
or Switch executable is established by these platform checks.

```powershell
python scripts/check_animation_xml.py --dll build/native-recovery/Release/FS15Core.dll
# A local inventory selects the three original base-game I3D files when present.
python scripts/check_animation_xml.py --dll build/native-recovery/Release/FS15Core.dll --i3d 'path/to/original.i3d'
ctest --test-dir build/portable-core -C Release --output-on-failure
```

Continue with complete I3D/XML dispatch, file/archive integration, update
scheduling, native skeleton/skin binding and renderer consumption. ReFarm's
existing authored host remains separate; full GE6 and Switch execution are not
validated by this component.
