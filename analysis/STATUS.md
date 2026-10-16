# Project status — October 16, 2026

**Additional local GE6 milestone:** a separate modified original runtime now
reaches the original menu and Bjornholm with the native HUD/vehicles visible,
using an authored local platform adapter and no Steam library in the game
process. The source probe passes 104 comparisons and all three hybrid helpers
are called. The preexisting Steam client was left running; closed-client startup
and full save/reload remain untested. Steam services/licensed DLC are unavailable.
See [local runtime evidence](local-engine-validation.json) and
[launcher instructions](../docs/LOCAL_ENGINE_RUNTIME.md). This is not a fully
recompiled GE6 engine.

**Recovered scripts, original-map rendering and a gameplay alpha are available. The full game is not rebuilt.**

The reference is Farming Simulator 15 for Windows **1.4.2.0**. Its original
executables were checked after analysis and remain unchanged. Original files
and machine-specific analysis outputs remain local.

## Compiled components

The future target is Nintendo Switch homebrew (.nro); console work is deferred.
A portable C++ core now compiles without
Windows/Direct3D/LuaJIT or original game payloads. Recovered wheel registry/read
logic passes 1,440 lookup / 11,760 full-frame comparisons, and seven bit
operations pass 21,763 full-frame comparisons. RLE/tire checks still pass.
The affine point-transform leaf now passes 12,000 bit-exact native comparisons
and runs in the experimental host's Lua transform service. Host GRLE8/16 loading
uses recovered decoders behind strict file/run validation; invalid collision
radii are rejected. The [recompilation ledger](native-recompilation-summary.json)
separates 92 bounded reconstructed entries from 11,543 pseudocode-only entries. This is host component
validation; the complete engine and console build remain pending.
See [scope and remaining work](../docs/NATIVE_RECOMPILATION.md).

The latest scene component reconstructs six affine/quaternion math entries,
hierarchical world-transform caching and four coordinate script adapters.
36,000 math outputs, 5,999 complete node arenas and 4,799 complete frames match
original code bit for bit. ReFarm uses recovered local/world conversions through
an authored matrix bridge; its scene ownership remains authored. Both maps pass
the 24-run gameplay reliability suite after this change. The portable build now
has nine asset-free component tests passing on Windows. Of 64 Python fixtures,
62 pass and two Windows symlink checks are skipped. All fourteen native
differential suites also pass.

Eleven additional TRS entries now reconstruct axis rotation, composition,
Euler extraction, lazy cache refresh, rebuild propagation and six Lua
setters/getters. The Windows run matches all 18,000 math comparisons and 7,200
complete scene arenas, with exact flags/events/metadata and bit-identical float
regions. Platform libm remains an explicit dependency rather than reconstructed
CRT math. ReFarm's original I3D hierarchy, animated nodes and vehicle-root poses
now use recovered TRS/affine composition. Both maps repeat all 24 reliability
runs and match packaged scene/fleet/discharge renders after integration. See
[TRS scope](../docs/SCENE_TRS_RECOVERY.md).

The additional scene-state component recovers five native paths with a partial
typed node layout and known base/TRS dispatch. All 12,000 complete synthetic
node arenas and 7,200 notification traces match original code. The exercised
world-preserving branch changes 185 child local matrices. Notifications are
queued through an authored bounded transport; object allocation/destruction,
arbitrary derived classes and callback side effects remain unresolved. Two
constructor-observed vtables expose 16 audited slots and four newly catalogued
functions. See [state scope](../docs/SCENE_STATE_RECOVERY.md).

Three hierarchy entries now recover ordered attachment, reparenting and removal
with their observer/state effects. 19,200 successive complete node arenas and
child buffers match original code, including 18,726 ordered notifications.
The native root-identity threshold is `1e-4f`. Storage remains preallocated;
native vector growth, constructors/destructors, registries and object/resource
ownership are not recovered. ReFarm's ownership remains authored. See
[hierarchy scope](../docs/SCENE_HIERARCHY_RECOVERY.md).

Thirteen additional lifetime entries initialize supported base/node/matrix/TRS
objects and recover naming and recursive destructor chains. 2,500 constructor
records, 7,500 renames, 2,500 nondeleting destructor records and 250 recursive
forests / 2,470 deleting destructions match the original. The native reference
uses original CRT allocation and registry/tree code with a private heap and five
kernel import seams. Portable allocator/ID/registry/dispatch services are authored.
Shared resources, triggers, arbitrary derived classes and
singleton/concurrent behavior remain open. See
[lifetime scope](../docs/SCENE_LIFECYCLE_RECOVERY.md).

Eight native attribute paths now retain the actual RB tree, inline/heap key
layout and Lua value/frame semantics. 12,780 complete trees, 12,600 setters and
25,200 getters match original code; the original getter also directly reads
reconstructed storage in 25,200 calls. Nonempty-map destruction matches 180
member-free-order traces. The native String-to-scalar behavior retains 1,177
text allocations in these fixtures; those and their pointer residues are
reported explicitly and reclaimed separately by the allocation contexts. See
[attribute scope](../docs/SCENE_ATTRIBUTES_RECOVERY.md).

A new static [portability audit](native-portability-summary.json) records
31,393 known call edges. The observed scripting roots reach 2,878 eligible
functions, including 939 with computed calls. Types, virtual/dynamic dispatch
and full linking remain unresolved. The recovered core cross-compiles for ARM64
Linux and passes all eight asset-free tests under QEMU in CI. This is an intermediate
CPU check; no full ARM64 GE6 or Switch build is available. See
[platform evidence](portable-core-platform-validation.json).

Ten additional animation entries recover the generated normalization table,
compressed quaternion interpolation, key selection/easing, four-track controls
and bounded known-TRS pose evaluation. 4,320 clip samples, 42,444 complete channel
records and 2,240 successive animation/scene arenas match original code, with
28,545 ordered notifications and bit-identical recorded float regions. Loading,
scheduling, arbitrary callbacks, skinning and renderer consumption remain open.
See [animation scope](../docs/ANIMATION_RECOVERY.md).
See [concrete porting blockers](../docs/PORTABILITY_STATUS.md).

The [unified graphical launcher](../docs/UNIFIED_LAUNCHER.md) now selects the
home runtime, original GE6 baseline or GE6 with three recovered wheel helpers.
The native menu, Westbridge Hills and tractor HUD were observed; all three
recovered helper methods were called by native vehicles after 104 comparisons.
The home engine also reached its ready state from the same launcher. Direct
startup of the tested Steam copy exits with code 53; normal Steam startup works.
This combines launch configuration and bounded recovered logic, with separate
native renderer/physics processes. See [native evidence](native-engine-validation.json)
and [launcher evidence](unified-launcher-validation.json).

The launcher now has Play, Resources and Diagnostics pages. It detects local
Steam libraries and prepares/repairs original terrain, mesh, texture, script and
starting-fleet resources through the existing tools, with progress/cancellation.
The real GUI worker completed local preparation and recorded 1,609 file hashes;
fresh archive extraction still requires the pinned original native GAR oracle.
See [preparation instructions](../docs/LAUNCHER.md). Latest validation passes all
the earlier 24 Windows CTest checks. Latest component and Python checks are
listed above; directory-symlink checks require extra local Windows privileges.

The new [gameplay alpha](../docs/GAMEPLAY_RUNTIME.md) loads original starting
fleets and field/crop data, executes recovered motor/wheel and farm-transfer
rules, and supports basic cutting/cultivation/sowing through a partial native
bridge. Both maps pass 43 gameplay assertions. Recovered AnimatedVehicle and
Trailer loading/update now control original XML animations, discharge delay and
automatic closing. The source matches original bytecode in 777 fixture samples.
Both maps also render animated farm discharge with inventory conservation.
The host now uses fixed 60 Hz simulation updates, bounded stall recovery and
P/focus-loss pause with input clearing. In 24 native runs, both maps match
vehicle/crop/inventory state and captured images across 15/30/60/144 Hz frame
partitions and pause/resume. The same exercised paths match 28 original whole
Lua chunks loaded in a separate reference mode; source slices and authored
native services remain shared. See [reliability evidence](mission-reliability.json).
The RLE16 leaf matches original
code in 107 cases / 35,133,356 values, including both full fruit maps. Full
GE6/PhysX behavior, complete specialization startup and persistent saves remain
unfinished. Explicit harvest/silo scenarios relocate original equipment only
for reproducible trials; normal new-game placement uses original templates.

The native catalogue now has **11,635 verified local pseudocode exports** from
11,635 eligible functions, with zero remaining export failures in this catalogue.
Thirty uncovered entries were discovered from 32 explicit scripting code-label
references and then decompiled; four additional explicit scene-vtable targets
bring the catalogue to 12,200 functions.
Another 565 catalogue entries are thunks or external functions. There are also
498 explicit native scripting descriptors indexed separately from earlier Lua mappings.
The tire-force calculation at `0x1400c24c0` is reconstructed in C++ and matches
the original in **3,300 cases / 13,200 float outputs**. It is not yet integrated
with playable contacts/slip/rigid-body physics. See [native recovery](../docs/NATIVE_RECOVERY.md),
[export summary](native-export-summary.json) and [force validation](tire-forces-validation.json).

A newly compiled **FS15MenuRebuilt.exe** now launches a visible main menu
without starting the original game engine or Steam. It executes **36
recovered Lua sources** with authored XML/rendering/input/read-only save services,
including profiles, layouts, localization, textures
and the game's bitmap font. The native window was visually observed.

The frontend includes recovered Career, Difficulty and Map Selection screens.
Career displays twenty slots and reads existing metadata; mutations are
disabled. Empty slots lead through three difficulties and two base maps,
with Start launching the reconstructed original map scene for a new selection. Existing
saved simulations cannot load yet. Read failures,
unsupported maps and invalid numeric metadata cannot become new-game slots.

The smoke check renders 120 main-menu frames plus input-path screen captures,
exercises wheel scrolling to slot twenty, metadata display, new-game choices,
Back navigation, Credits and Quit. The package passes from a different working
directory with matching frontend PNGs. Source hashes are checked against the
recovery manifest. The build now has twenty-one CTest
suites when all local inputs/tools are present.

This is an independently implemented GUI host with a subset bootstrap.
Original `main.lua` and complete engine startup do not execute. The remaining
game screens, complete worlds, physics, audio and networking are unimplemented.
Career/Difficulty/Map selection, Credits/Back and Quit work; other destinations produce explicit progress
dialogs. See [menu notes](../docs/MENU_RUNTIME.md),
[validation](menu-validation.json) and the [checklist](../PROJECT_CHECKLIST.md).

| Component | Source | Validation | Scope |
|---|---|---|---|
| Launcher | `src/launcher/main.cpp` | Nine behavioral checks; dry run against the installed startup configuration | Starts the original game binary; not binary-identical source recovery |
| Frontend runtime | `src/menu/` | Native launch, recovered input paths, read-only metadata fixtures and portable package checks | Main/Career/Difficulty/Map Selection/Credits; new selections launch terrain exploration |
| Original map scene | `src/engine/`, `src/shape_bridge/` | Both original maps on hardware, authored WARP transformed-mesh fixture, separate terrain/object GPU samples and package parity | Original terrain, meshes, transforms and diffuse/weight textures; simplified shaders, no simulation |
| GE6 mesh cipher | `scripts/check_shapes_native.py` | 128 comparisons / 74,848 bytes with original native routine | Local isolated native oracle, not full engine integration |
| RLE8-to-bit-vector decoder | `src/core/rle8.cpp` | 266 comparisons with the original native function, covering 853,086 values | One engine routine with a documented valid-input domain |

The rendering host is authored code around recovered GIANTS resources and verified
format behavior, not recovered full GIANTS renderer source.
Both 1025-square original DEMs produce 2048-square-metre terrain meshes with
263,169 vertices and 524,288 triangles each. Strict version-3 `.shapes` decoding
recovers 1,155 / 833 triangle meshes. Original I3D transforms place 338,165 /
107,698 rendered shape instances, including individual leaf attachments.
Original diffuse textures and eight / six terrain layers are loaded. A 272-point
Bjornholm road/DEM check has 0.05 m median height difference (21.64 m for reversed Z).
See [engine scope](../docs/ENGINE_BOOTSTRAP.md)
and [engine validation](engine-validation.json).

The original sky dome/textures, Environment curve setup and AnimCurve evaluator
are now reused. Five original XML shader injections compile inside the native
host: sky depth/blending, wind, flags and two-sided foliage lighting. A local
2,048-square shadow map uses original sun settings and object shadow flags.
Fixed Map02 comparisons verify animation (17,978 changed pixels), night
brightness and visible shadows (212,089 changed pixels). Full GIANTS cascades,
skeletal trees, weather scheduling and mission simulation remain incomplete.
See [environment notes](../docs/ENVIRONMENT_RUNTIME.md) and
[validation](environment-validation.json).

The RLE reconstruction comes from **0x14028f430**, reached through references
to `src/base/util/RLEUtil.cpp`. Its descriptive name is inferred from code
and those references, not recovered from a debug symbol.

Differential tests execute the original function's **823 bytes** in the test
process and compare its outputs with recompiled C++. The Ghidra export checks
that the leaf has no external calls or memory references. The harness accepts
only the pinned SHA-256 reference; workspace verification also matches it to
the captured game code. No test code is injected into the running game.

Coverage includes 1–8-bit values, selected strides up to 16 bits, literals,
repeat-count extensions, prefilled outputs, and 64 KiB segment boundaries.
The reconstruction adds a checked output-capacity failure. Complete GRLE
files, the 16-bit input variant, arbitrary malformed engine inputs, and
gameplay are not validated yet.

Validation metadata: [reference](rle8-reference.json),
[differential results](rle8-validation.json). Instruction listings and native
reference bytes are generated locally and excluded from Git.

## Decompilation results

### Script layer recovery

The native `source` call chain and archive provider have now been traced.
Both local GAR archives have been extracted:

| Archive | Files | Relevant validation |
|---|---:|---|
| `dataS.gar` | 619 | 470 LuaJIT v2 chunks; 114 strict XML/I3D checks passed; seven original description XML files have missing attribute whitespace |
| `dataS2.gar` | 502 | 20 strict XML/I3D checks passed; asset headers and PNG CRCs checked |

All **1,121** extracted file sizes and hashes were rechecked against their
manifests. Independent checks passed for **374 DDS headers, 81 PNG CRC sets,
21 WAV headers, and 470 LuaJIT headers**. Seven strict XML diagnostics remain
recorded without changing the original extracted bytes.

The original native cipher is still used as a version-pinned oracle.
It is not a reconstructed, independent cipher implementation. The GAR
header/index parser and extraction checks are new code. The installed
game files were not replaced and no DLC path was used for this extraction.

All **470 Lua sources recompile** after authored adapters fix repeated literals
and prevent empty-table initialization from absorbing later stores. Comparison
against the original compiled chunks uses deterministic stripped dumps:

- **308 exact deterministic stripped-bytecode round trips**, 54 more than before.
- **162 syntax-only results**, with behavioral equivalence still unverified.
- **Zero remaining syntax failures** in this recovery pass.

All 254 earlier matches are retained under the deterministic check. The authored
table regression also verifies that a global table exists before a later field's
callback executes. Deterministic mode sorts template-table hash entries only;
it does not remove instructions, change constants or ignore control flow.

The rebuilt script package contains 470 recompiled chunks and a per-file
manifest. Chunks use portable game-relative names. The package was not
installed into the game, and game scripts were not executed by validation.
It is available locally at `build/FS15ScriptsRebuilt.zip`.

Public summaries: [recovery](lua-recovery-summary.json),
[script build](lua-build-summary.json), [file verification](recovered-files-verification.json).

### Native analysis

| Analysis | Functions catalogued | Pseudocode exports | Export failures |
|---|---:|---:|---:|
| Original launcher | 654 | 651 | 0 |
| x64 game, initial selection | 12,166 (historical catalogue) | 249 | 1 |
| x64 game, focused selection | Same catalogue | 44 | 0 |
| x64 game, expanded eligible catalogue | 12,200 (11,635 eligible) | 11,635 | 0 |

The game selections overlap. Counts include C/C++ runtime and third-party
library code and are not completion percentages.

Automatic engine analysis completed within 300 seconds. The formerly failing
**0x1400f8420** and **0x1401f59a0** now export after increasing response capacity,
omitting unused syntax-tree output and using a 300-second function limit.
The full-export script preserves successful files and retries short Windows
progress-file sharing conflicts. Two data-creation
diagnostics remain at `0x140457006` and `0x140457047`; exports and project
saving succeeded. The original PDB is unavailable. Raw pseudocode still
requires type recovery, naming, and reconstruction before compilation.

## Runtime code snapshot

On disk, the game's `.text` section has entropy close to 8 and cannot be
analysed directly as normal machine code. The installed game was started
normally through Steam, and only its loaded code section was copied into
a local PE analysis image.

- Captured code: **4,657,066 bytes**.
- Loaded code entropy: **6.5405**.
- Original x64 binary SHA-256:
  `21ce6fa0f776ad69f5a707a8c391cba6d2b80bdb612ffdf47f109372478b5488`.
- Analysis image SHA-256:
  `6d1beaf9d7e3c57dd51cec03385a53bda82b4ac946e622d8d2fa592a56d71f9f`.

Bytes outside the documented code range were verified unchanged. This partial
snapshot is an analysis input, not a runnable reconstruction or a complete
saved process state.

## Initial native function map

Virtual addresses use the x64 image's preferred base `0x140000000`.

| Address | Observation |
|---|---|
| `0x140011a30` | Lua registration routine; 112 observed name/implementation/wrapper associations indexed locally |
| `0x14000e200` | Native implementation registered as Lua `source`; traced into resource loading |
| `0x1400149d0` | Lua adapter associated with `source` |
| `0x14001ea90` | Reference to `dataS.gar` and initialization checks; full role undetermined |
| `0x14002b8c0` | Resource opening through a provider list under a critical section |
| `0x140096580`, `0x140098630` | Archive-related routines with an MD5 collision diagnostic; format unconfirmed |
| `0x14028e3d0` | Selects decoding variants for one- or two-byte input elements |
| `0x14028f430` | RLE8 decoder reconstructed and compared with the original |
| `0x14028f770` | Reconstructed RLE16; 107 differential comparisons, both original fruit maps included |
| `0x14027d5f0` | Archive factory; GAR is attempted before other formats |
| `0x1402a6d70` | GAR header/index loading and original cipher setup |
| `0x1402e6e60`, `0x1402e7500` | Original cipher initializer and buffer decoder used by the local oracle |
| `0x140280060` | Lua resource load; prefers `.luc` and handles the custom version-3 chunk transform |

Lua adapter signatures are partly inferred. Ghidra `undefined8` parameters
must not be treated as established source types.

## Remaining work

1. Investigate the 162 non-identical Lua round trips and validate behavior.
2. Reconstruct the GAR cipher independently and document native Lua interfaces.
3. Reconstruct RLE/GRLE variants and other isolated modules using differential
   comparisons with the original code.
4. Reconstruct engine startup, rendering, resources, audio, input, physics,
   and game behavior.
5. Validate a complete rebuilt executable through actual launches and play.

The recovered game scripts and assets remain local and excluded from Git.
Current validation establishes the analysis pipeline, launcher behavior,
one native routine, archive recovery, a recompiled Lua layer, a visible
recovered-menu launch and an authored native terrain-engine startup. It does
not establish full original engine or gameplay equivalence.

Known [CharacterSet/data/clip ownership](../docs/SHARED_SCENE_RECOVERY.md) now covers shared pose references, node rebinding,
update-list removal and nested clip destruction. Other resource classes,
animation evaluation, active triggers and complete loading remain open.
