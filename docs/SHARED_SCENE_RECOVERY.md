# Recovered shared animation ownership

Twelve additional native paths reconstruct known GE6 CharacterSet/shared pose
data lifetime and binding. The native record layouts and embedded interfaces
are retained; this is not a complete recovered animation system.

| Entries | Recovered behavior |
|---|---|
| `0x140174c10`, `0x140174cc0`, `0x140174e10` | Construct/release shared pose data, clips, channels and owned keyframe blocks |
| `0x140174e40`, `0x1401750f0`, `0x1401750c0` | Construct/destroy CharacterSets, data references, update registration, names and transform storage |
| `0x1401752f0` | Bind/rebind TRS nodes, create missing default poses/visibility and validate clip channels |
| `0x140176800`, `0x1401bd100` | Release clip names, channel vectors and keyframe allocations |
| `0x140041980` | Remove observers from active and marked pending update vectors |
| `0x1400b1520`, `0x1400b1e10` | Clone default property flags and replace/release the original property record |

The CharacterSet reference interface is embedded at `+0x28`, its count at
`+0x30`, update observer at `+0x38`, shared data at `+0x48` and transform vector
at `+0xa8`. Each bound node stores that embedded reference in its `+0x120`
field. Rebinding releases the previous reference. Unsigned counts below two
dispatch deleting destruction; higher counts decrement. Shared data has its
own reference count, so deleting one set can leave another set's poses intact.

Default pose binding records translation, WXYZ quaternion, scale and visibility
bits. It retains native partial-prefix updates when a non-TRS node or short clip
raises a warning; the portable API reports a diagnostic rather than invoking
the original logger. Update-vector ordering, capacity and unused slots remain
native-compatible. Calls on one portable context must be serialized by its host.

## Evidence and boundaries

- 220 explicit data constructor records, plus 220 constructor/destructor pairs
  that allocate their own shared data.
- 440 CharacterSet constructor and 440 bind/rebind records; 1,760 complete
  default-pose/visibility buffers.
- 220 shared lifetime scenarios, including 4,310 node destructions and 4,297
  owned clip channels with nested keyframe allocations.
- 220 default property clone/replacement pairs and 220 update-list snapshots.
- 32 non-TRS and 24 short-clip diagnostics retain their observed prefix effects.
- Final object registries and object-owned allocations are empty. Persistent
  update-vector capacity is explicitly released by the fixture context.
- An eighth asset-free CTest checks sharing, rebinding, pose defaults, node
  lifetime integration, warning prefixes and failed-constructor cleanup.

Records and vector storage are compared after pointer normalization. Pose
floats use the explicit `8e-6` scaled gate; recovered TRS/quaternion flow still
uses platform math rather than reconstructed CRT transcendental bodies.
Recorded maximum error is included in the evidence; universal bit identity is
not claimed. Original code runs only in the isolated private-heap reference,
with an explicitly initialized update registry, never in a modified live game.

Allocation, ID/object registry, synchronization, known dispatch and failure
guards are authored integration services. Original exception/unwind behavior,
other derived/shared resource classes, skinning, active triggers and complete
scene startup remain open. [Bounded animation evaluation](ANIMATION_RECOVERY.md)
is now recovered separately. Vtable values
in portable records are identity tokens, never callable original pointers.
ReFarm retains authored ownership/loader code. No complete portable GE6 or
Switch executable is validated by this component.

See [native evidence](../analysis/shared-scene-validation.json),
[platform checks](../analysis/portable-core-platform-validation.json) and
[remaining porting work](PORTABILITY_STATUS.md).

```powershell
python scripts/check_shared_scene.py --dll build/native-recovery/Release/FS15Core.dll
ctest --test-dir build/portable-core -C Release --output-on-failure
```

All eight asset-free portable-core tests pass on Windows x64, Linux x64 and
ARM64 Linux under QEMU. Original-code comparisons remain Windows-only; these
platform checks do not validate a complete engine or console executable.
