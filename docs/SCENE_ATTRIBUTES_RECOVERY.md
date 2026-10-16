# Recovered native scene attributes

Eight additional native paths now have portable reconstructed code. Attribute
storage retains the original `0x50`-byte red-black-tree nodes, `0x10`-byte tagged
values, sentinel links, colors and observed key-string layout. This component
does not replace the tree with an unrelated authored key/value dictionary.

| Native entry | Recovered path |
|---|---|
| `0x1400246f0` | Case-sensitive byte-key lookup and value-pointer result |
| `0x140136870` | Find an existing value or allocate/insert a zero-initialized value; existing kind is retained |
| `0x140136980` | Allocate a native-layout entry and copy its inline/heap key |
| `0x14027e1c0` | Attach, update extrema/count and rebalance the observed red-black tree |
| `0x1400e6900` | Lua getter frame, exact result tag and partial value writes |
| `0x1400e69b0` | Lua setter frame, type dispatch and integer conversion |
| `0x14016edc0` | Replace text storage and update value kind |
| `0x1401365f0` | Release attribute values, key strings, tree nodes and the map sentinel |

## Storage and script semantics

Keys compare unsigned bytes and preserve case. Up to 15 key bytes fit inline;
longer keys allocate the observed `(length | 15) + 1` buffer. String padding,
unused value bytes and node padding remain unchanged. Native allocator and
exception internals are not recovered by these routines; allocation uses the
existing authored platform service. Temporary lookup-string allocation is not
reproduced, since the original key comparison can read the supplied bounded
bytes directly. Normal lookup results and tree contents retain native behavior.

The setter recognizes exactly `Boolean`, `Integer`, `Float` and `String`.
Unknown type names leave the map unchanged. Boolean writes preserve the input
byte, not just zero/one. Integer input with the original integer tag copies its
32 bits; other tags follow observed `CVTTSS2SI`, including `INT_MIN` for NaN or
out-of-range input. Floats preserve their 32-bit representation. Getter tags and
write widths match the original: bool 1 byte, integer/float 4 bytes and text
pointer 8 bytes. Missing/null getters clear only the result tag.

The original setter can retain an allocation when changing a String to a scalar:
it overwrites only the scalar bytes and kind without freeing the previous text.
**1,177 such retained allocations were observed** in these fixtures. Upper
bytes can also retain part of the former pointer. This behavior is preserved
and reported; it is not hidden under a leak-free claim. The owning allocation
context must track/reclaim those blocks separately. Full-map cleanup frees text
for kind 3, while kind 4 is returned as text but is not freed by that cleanup.
Replacement through the text setter has its separately observed ownership rules.

## Lifetime integration and guards

The recovered object destructor chains now accept valid **nonempty** attribute
maps and call recovered cleanup before releasing observers/children/name state.
Cleanup first frees values in key order, then releases key strings and tree
nodes in the original right-subtree/left-chain order, followed by the sentinel.
The public clear operation can instead retain/reset the sentinel for reuse.

Bounds on input strings/frames, tree validation, depth checks, rejection of
self-aliasing text replacement and cleanup after failed staged allocation are
authored guards. They are not claims about native malformed-input handling or
C++ exception unwinding. These low-level APIs require valid owned member
pointers. They do not parse arbitrary untrusted addresses. Individual map-entry
erasure, remaining shared resources, active trigger cleanup,
remaining derived objects and full scene loading remain separate recovery work.

ReFarm's object ownership and resource loader remain authored. This component
is not evidence that the entire engine or a console executable is rebuilt.

## Evidence

- **12,600 setter frames** and **25,200 getter frames** match the original.
- **12,780 complete tree comparisons**, covering **218,914 entry observations**,
  include topology, colors, key capacity/buffers, values and opaque padding.
- **25,200 original getter calls directly consume reconstructed trees**. The
  original code reads the portable component's actual native-layout storage;
  there is no intermediate dictionary or marshalled lookup result.
- 360 additional ensure-existing/kind-4/unknown-kind frames and 180 null-owner
  getter/setter pairs validate the remaining observed frame branches.
- **180 nonempty-map destructor and complete member-free-order traces** match.
  Residual allocations are checked explicitly against the observed native
  String-to-scalar retention and kind-4 behavior, then reclaimed by the fixtures.
- A seventh asset-free CTest checks sorted insertion/balance, conversion and
  frame invariants, sentinel reuse, destructor integration, malformed-map
  rejection, allocation-failure cleanup and the retained-text behavior.
- All seventeen portable-core tests pass on Windows x64, Linux x64 and ARM64 Linux
  under QEMU. The original-code comparisons remain Windows-only. These checks
  do not validate a complete engine or Switch executable.

Addresses are normalized between the two independent allocations. Scalar
pointer residues are first checked against their actual originating allocation,
then normalized; unrelated value bytes remain exact. The Windows oracle uses
the existing private-heap native reference without running the game entry point
or modifying a live process. Original code and raw allocations stay local.

See [native evidence](../analysis/scene-attributes-validation.json),
[lifetime scope](SCENE_LIFECYCLE_RECOVERY.md),
[platform results](../analysis/portable-core-platform-validation.json) and
[remaining porting work](PORTABILITY_STATUS.md).

```powershell
python scripts/check_scene_attributes.py --dll build/native-recovery/Release/FS15Core.dll
cmake --build build/portable-core --config Release
ctest --test-dir build/portable-core -C Release --output-on-failure
```

The later [scene-copy component](SCENE_CLONE_RECOVERY.md) recovers typed value
cloning and whole-map assignment, including self-copy and kind-4 retention.
