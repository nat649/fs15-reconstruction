# Recovered scene object construction and lifetime

The portable core now reconstructs thirteen additional native paths for base
objects, plain scene nodes, matrix nodes and the two observed TRS constructors.
It creates owned name/property/attribute-header allocations, initializes the
observed object layout, renames objects and destroys supported object forests.
These are recovered component paths, not a complete engine object system.

| Native entry | Recovered path |
|---|---|
| `0x14004d640` | Base object initialization, wrapping ID, optional name and registration |
| `0x14004d760` | Base destruction, unregister and release name |
| `0x14004d7a0` | Release old name, allocate and copy new NUL-terminated bytes |
| `0x1400b5ec0` | Scene-node initialization, empty child/observer vectors and owned attribute-map header |
| `0x1400b15e0` | Matrix-node identity, dirty world cache, properties and clip/mask defaults |
| `0x14010fa90`, `0x14010fb40` | Default-class and caller-typed TRS constructors |
| `0x1400b1710` | Matrix property release, empty attribute-map cleanup and observer storage release |
| `0x1401362a0` | Detach from parent, delete ordered children, free child storage, unregister and release name |
| `0x14010fbb0` | TRS destruction when no active-trigger teardown is required |
| `0x1400b5f90`, `0x1400b16e0`, `0x14010fb10` | Node/matrix/TRS deleting wrappers, including the low-bit delete flag |

## Recovered layout and ordering

`scene_state.h` now names the registration byte, name pointer, identifier, class
tag, attribute-map header/count and shared-resource pointer at their observed
offsets. Other bytes remain opaque. The default TRS constructor forces class tag
`0x1d`; the typed constructor retains its argument. Registration occurs before
the final class tag and derived type flags are written.

Matrix defaults include a local identity, dirty-world flag `0x40`, clip interval
`[1000000, 0]`, masks `0xffff`/`0xffffffff` and a separate property allocation.
The cached world matrix remains uninitialized until its lazy update; the
reconstruction preserves those bytes. TRS caches start at translation/rotation
zero and scale one. Empty construction names remain null; an empty rename
allocates one byte. Self-aliasing rename input is rejected before freeing.

Destruction changes dispatch identity as each base is reached. In particular,
after entering hierarchy-base destruction, child detach uses the raw hierarchy
operation rather than the live matrix-node propagation path. Children are
deleted from the front in original order, with descendants removed before their
parent. The parent detach requests suppression. The node's child-vector storage,
name and registration are released after its children.

## Recovered code versus authored services

`scene_lifecycle.cpp` contains recovered initialization/lifetime flow.
`FS15SceneLifecycleServices` supplies allocation/free, synchronized ID generation,
registry insertion/removal and known unlink/deleting-dispatch callbacks. Those
platform/container services are **authored integration boundaries**, not
recovered CRT heaps, Windows critical sections or native red-black-tree bodies.
The low-level API expects valid owned member pointers and correctly dispatched
known object kinds. It is not an untrusted-memory parser.

Observed vtable addresses are stored as stable identity tokens. They are never
called as original C++ function pointers by the portable library. The native
reference separately executes the actual observed vtables. ReFarm retains its
authored object ownership; these lifecycle paths are not yet integrated into a
fully recovered scene loader, physics world or complete engine startup.

Supported destruction now accepts valid nonempty maps through the
[recovered attribute component](SCENE_ATTRIBUTES_RECOVERY.md), while requiring
default property type, a supported shared-reference dispatcher and no active-trigger teardown. Per-object checks reject
those unsupported states before freeing that object's members. The owning host
must preflight descendants and supply correct callbacks. A callback failure
after teardown starts cannot undo external frees or registry changes.

Allocation-failure cleanup and bounded-input rejection are authored behavior.
The ID may already have advanced and a failed construction record may be
partially initialized; it must not be destructed. Original C++ exception/unwind
paths, arbitrary derived classes, shared-resource reference
counting, triggers, singleton startup and concurrent registry semantics remain
open. Full native allocation/ownership recovery is therefore still incomplete.

## Evidence

- **2,500 constructor and 2,500 nondeleting-destructor record comparisons**,
  including opaque bytes and empty/non-ASCII/long synthetic names.
- **7,500 renames**, **3,500 auxiliary allocation comparisons** and **4,970
  semantic registry snapshots**, including 32-bit identifier wraparound.
- **250 recursive forests**, containing **2,470 deleting object destructions**,
  compare complete retired records, deletion order and final ownership cleanup.
  Both references end with empty object registries and no remaining object-owned
  allocations. The native reference retains only its registry sentinel.
- A sixth asset-free CTest verifies ownership, defaults, registration timing,
  ordered recursive deletion, alias guards and failure cleanup independently.
- All eleven portable-core tests pass on Windows x64, Linux x64 and ARM64 Linux
  under QEMU. These invariant checks are separate from the Windows original-code
  comparisons; no complete ARM64 GE6 or Switch executable is validated.

The isolated Windows oracle rebinds five inspected kernel imports to a private
heap and synchronization services. Original CRT allocator/free and native
registry/tree bodies execute unchanged. No executable code is patched, no PE
entry point runs, and no live game process is modified. The already-initialized
singleton state is supplied explicitly; this is not a startup test.

See [comparison evidence](../analysis/scene-lifecycle-validation.json),
[platform results](../analysis/portable-core-platform-validation.json) and
[portability limits](PORTABILITY_STATUS.md). Original native code, allocations
and raw exports remain local.

```powershell
python scripts/check_scene_lifecycle.py --dll build/native-recovery/Release/FS15Core.dll
cmake --build build/portable-core --config Release
ctest --test-dir build/portable-core -C Release --output-on-failure
```

Known [CharacterSet/data/clip ownership](SHARED_SCENE_RECOVERY.md) now covers shared pose references, node rebinding,
update-list removal and nested clip destruction.
[Bounded animation playback](ANIMATION_RECOVERY.md) is now recovered separately.
Other resource classes, loading, scheduling, skinning, active triggers and
complete startup remain open.
