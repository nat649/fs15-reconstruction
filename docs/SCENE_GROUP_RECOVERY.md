# I3D TransformGroup/LOD callbacks and loaded-node animations

The portable core now reconstructs the TransformGroup begin callback and the
non-shape group-end path. They create actual recovered native-layout TRS/LOD
records, apply common attributes, register source IDs and attach owned children
in order. Recovered XML animations can resolve those source IDs and animate
the resulting hierarchy, including its LOD world-position register.

This is decoded callback integration. The original XML tokenizer, complete I3D
loader and derived rendering objects remain open.

## Recovered entries

| Native entry | Bounded reconstructed behavior |
|---|---|
| `0x14018c1a0` | Named TransformGroup/LOD construction, common attributes, append attachment and current-node update |
| `0x14018c370` | Pop current to its parent and return for non-shape nodes |
| `0x1400266d0` | Append parsed scalar segments to the dynamic `lodDistance` float vector |
| `0x140028150` | Grow a four-byte-element vector by the native 1.5x capacity rule |
| `0x1400281d0` | Reserve four-byte-element storage, copy used elements and release old storage |

These are five additional bounded entries. Constructors, common attributes,
source-ID transport, hierarchy/state/LOD dispatch and animation paths reuse
previously validated components and are not counted again.

The original code retains the first attribute-name pointer in `RDX` while its
name search advances `RCX`. Instruction inspection confirms that the subsequent
lodDistance search restarts at attribute zero. A decompiled loop representation
was ambiguous here; a lodDistance attribute preceding name is explicitly tested.

## Callback and numeric behavior

The first exact matching name wins. An absent or null first name returns without
allocation or current-node change, even when a later duplicate has a value.
The first exact lodDistance also wins; null keeps a plain TRS node, while an empty
nonnull value still selects a LOD object with its constructor defaults.

Common attributes run before attachment with the original physics, source-ID
registration and joint switches enabled. The parent receives the child at
`UINT32_MAX` append position. Current changes only after that call completes.
End always pops the current node; it does not maintain an authored parallel stack
that silently repairs a preceding ignored begin callback.

The dynamic float parser appends a scalar for every ASCII-space delimiter and
one final scalar. Repeated/leading/trailing spaces retain native duplication and
empty-value behavior. For example, `0  10 100` yields `(0,10,10,100)`, which the LOD
constructor retains. A direct parser call with scan length zero appends its final
scalar; the group callback skips that call for an empty lodDistance string.
The scalar grammar is the previously recovered native numeric grammar.

The four-byte vector preserves native capacity, spare bytes and prefix appends.
The scalar/count/finite/work guards are authored and exclude undefined inputs.
Raw allocation exception and native stack-alias corner cases are not claimed.

## Ownership and authored services

`FS15SceneGroupServices` transports decoded attributes and uses known recovered
scene services. A constructed-node callback adds each completed allocation to the
host's owned view before common attributes need it. A link callback invokes the
recovered hierarchy path. These services and the exposed result structure are
authored interfaces, not a recovered complete loader object.

After construction succeeds, the result transfers ownership of that node even
if common attributes or a later callback fails. Source-ID registration and a
partially performed link are not rolled back. Callers must retain and release
that prefix with its known lifetime path. Failed constructors release their
acquired members before the raw record is freed. Callbacks cannot throw/reenter.

The non-shape end path is recovered. When type flag `0x800` identifies a shape,
the API performs the original parent pop and reports the remaining branch as
unsupported without reading derived `+0x190` renderer state. Native render-resource
finalization, scheduling and the rest of that branch are still open.

The new owned matrix adapter calls the recovered matrix/TRS setter; inspected
LOD tables inherit that same TRS setter. Together with recovered cache refresh
and state propagation it connects animation playback to callback-created nodes.

## Native evidence

[Source-pinned comparison results](../analysis/scene-groups-validation.json) cover:

- 3,000 complete float-vector allocation states: 500 reserves, 500 growth calls
  and 2,000 parser appends, including truncated explicit scan lengths;
- 87 complete callback-created forest lifetimes, with 648 LOD and 440 TRS groups;
- 48 ignored begin calls and 1,088 group-end callbacks;
- 2,746 complete callback/playback states and 46,148 node-record comparisons;
- source-ID tree, names, pending body/joint metadata, ordered child allocations,
  parent/current pointers and LOD pending vectors compared throughout;
- 696 animation XML callbacks and 435 playback updates bound through IDs emitted
  by the recovered group callback, including animation of loaded LOD nodes;
- 368 loaded LOD resource updates and 87 complete loaded LOD manager allocations;
- recursive destruction with no remaining owned storage or object registrations.

The real-resource cases load **63 complete supported original subtrees**:
20 from `kroneSwadro2000.i3d`, 19 from `map01.i3d` and 24 from `map02.i3d`.
Each selected subtree contains only supported TransformGroup nodes. Its original
attributes and parent/child hierarchy are retained. Unsupported Shape/Light/
Camera nodes are not discarded and reparented to fabricate a supported map.
The complete maps are not loaded by this test.

Original native functions run unchanged in a private heap/FLS/C-locale fixture,
without starting the game or modifying a live process. XML decoding, external
file access, platform initialization and services remain authored. Pointer fields
are normalized; accumulated hierarchy/libm float regions use scaled tolerance
`1e-5`. Containers and non-float bytes are checked completely. Unactivated trigger
bits, if present, are cleared on both sides before cleanup; live trigger teardown
is not validated. Original resources and raw pseudocode remain local.

The asset-free `portable_scene_groups` test independently checks attribute
precedence, missing-name behavior, defaults, callback/allocator failure ownership,
unsupported shape finalization and the loaded-group-to-animation-to-LOD chain.
All sixteen portable tests pass on Windows x64, Linux x64 and ARM64 Linux
under QEMU. The exact tested revision and CI job IDs are recorded in
[platform evidence](../analysis/portable-core-platform-validation.json).

```powershell
python scripts/check_scene_groups.py --dll build/native-recovery/Release/FS15Core.dll
cmake --build build/portable-core --config Release
ctest --test-dir build/portable-core -C Release --output-on-failure
```

The next loading boundaries are native Camera/Light/Shape callbacks, their
derived layouts and resources, shape-end finalization and the original XML
tokenizer/full dispatcher. Native rendering/physics backends, complete map loading
and complete engine startup remain unverified.
