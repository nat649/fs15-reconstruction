# Recovered GE6 animation playback

Ten additional native entries are reconstructed as portable C++17. They extend
the recovered [CharacterSet ownership and binding](SHARED_SCENE_RECOVERY.md)
with key selection, compressed quaternion decoding, four-track playback and
pose application through known scene services. They do not establish a complete
animation loader, skeletal renderer or recovered GE6 executable.

| Native entry | Recovered behavior |
|---|---|
| `0x1400524b0` | Generate 512 reciprocal-square-root seeds and the special middle entry |
| `0x140174af0` | Four-component normalization using the generated seed and two double-precision refinement steps |
| `0x1402806f0` | Shortest-path normalized linear quaternion interpolation |
| `0x1401766d0` | Decode signed 16-bit XYZW keys, convert to WXYZ and interpolate the selected pair |
| `0x140176950` | Clamp/wrap clip time, use 64 lookup buckets, select adjacent/seam keys and apply per-component easing |
| `0x1401762a0` | Store a track time and detect pose changes using clamped old/new values |
| `0x140176330` | Store a blend weight and update the dirty byte |
| `0x140176370` | Assign a clip index, retain other track flags and mark changes |
| `0x1401763b0` | Case-sensitive name lookup with native inline/heap strings, returning the first match |
| `0x140175820` | Advance four tracks, sample active clips, blend translation/rotation/scale/visibility and apply node updates |

## Layout and retained behavior

CharacterSet contains four parallel arrays: flags at `+0x54`, clip indices at
`+0x64`, speeds at `+0x74`, times at `+0x84` and weights at `+0x94`. The update
entry receives the embedded `+0x38` observer in the original executable; the
portable API receives the containing record explicitly. Its enabled and dirty
bytes occupy `+0x50` and `+0x51`.

Clips have duration at `+0`, a native string at `+8` and channel pointers at
`+0x30`. A channel retains its flags, key vector, 64 unsigned-short lookup
entries, cached timing, three interpolation weights and two selected pointers.
Keys begin with time, flags and byte offsets for packed rotation and scale.
Translation is stored directly after the eight-byte key header. Packed rotation
stores signed XYZW components divided by 32767, then normalized as WXYZ.

The sampler retains the original exact-duration endpoint, negative-time wrap,
single-key behavior, repeated timestamps and the last-to-first looping seam.
Each component can use its own incoming/outgoing smooth interpolation flags.
Instruction inspection corrected the decompiler's floating-point grouping:
easing is `(3 - 2*w) * (w*w)`. Normalization also retains the observed float sum
order and double refinement grouping. The seed table is computed from the
recovered algorithm; original table bytes are not embedded.

Pose evaluation starts translation, scale and visibility from shared defaults.
Tracks add weighted translation/scale differences; quaternions accumulate with
hemisphere correction and are normalized before building their matrix basis.
Scale-only updates use the existing TRS scale cache, refreshing it when dirty.
A visibility channel chooses its first selected key only when track weight is
greater than 0.5. Matrix and visibility propagation use original origin zero.
If tracks share one clip, later sampling overwrites that clip's selections:
evaluation deliberately retains this native last-sample-wins behavior.

## Evidence

The [source-pinned original-code comparison](../analysis/animation-validation.json)
checks:

- All 512 generated seeds, 8,000 normalized quaternions and 8,000 nlerp outputs.
- 4,320 complete clip samples, comprising 42,444 complete channel records and
  compressed quaternion outputs.
- 1,680 clip-name lookups and 3,360 track-control records.
- 2,240 successive complete animation/scene arenas, including four active
  tracks, shared clip indices, reverse playback, negative/zero weights,
  disabled sets, visibility and scale-only cache refresh.
- 28,545 ordered observer notifications. Flags, opaque record bytes, selected
  keys and event origin/order are checked in addition to numeric results.

All compared float regions are bit-identical in the recorded Windows fixture.
The explicit numeric gate remains `8e-6` scaled error; this does not promise
universal bit identity across compilers or platforms. `sqrt` and `fmod` use the
platform library, while the original CRT implementations remain unrecovered.
Original routines execute only in an isolated pinned-image reference process.
No live game code is modified by these comparisons.

The asset-free `portable_animation` CTest independently checks loop endpoints,
easing flags, antipodal/zero quaternions, clamped dirty detection, pose/visibility
application, shared clips and invalid-input output preservation. Platform
results and their tested revision are recorded in the
[portable-core evidence](../analysis/portable-core-platform-validation.json).
All twenty-one asset-free core tests pass on Windows x64, Linux x64 and ARM64 Linux
under QEMU. Original-code differential execution remains Windows-only; these
checks do not validate a complete engine or Switch executable.

## Integration boundary and remaining work

`animation.h` exposes observed layouts and authored C interfaces. Callers own
valid decoded key storage, vectors and pointees; these APIs are not parsers for
untrusted animation files. Finite-value, capacity, index and track-count guards
are authored. Sampling stages its results before committing channel outputs.
Playback updates retain preceding mutations if a later host callback fails.

The host provides serialized TRS refresh, matrix setter and propagation
callbacks. The differential fixture delegates those to already recovered scene
arena services and uses side-effect-free observers. Arbitrary derived vtables,
callback reentrancy, concurrent scheduling and original exception/unwind paths
are outside this validation. Portable vtable values remain identity tokens.

The [binary animation resource path](ANIMATION_RESOURCE_RECOVERY.md) now builds
native-layout keys/clips/sets and reaches these playback services.
[XML callbacks](ANIMATION_XML_RECOVERY.md) also construct these resources.
Recover the original XML tokenizer/complete dispatch, file/archive/scene integration, scheduling, other resource
classes, skeleton/skin binding and renderer consumption next.
ReFarm still has its existing authored animation integration; this milestone
does not silently replace it or claim a complete GE6 or Switch build.

```powershell
python scripts/check_animation.py --dll build/native-recovery/Release/FS15Core.dll
ctest --test-dir build/portable-core -C Release --output-on-failure
```
