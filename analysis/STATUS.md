# Project status — October 15, 2026

**The native analysis and Lua recovery milestones are complete. The full game is not rebuilt.**

The reference is Farming Simulator 15 for Windows **1.4.2.0**. Its original
executables were checked after analysis and remain unchanged. Original files
and machine-specific analysis outputs remain local.

## Compiled components

The new [gameplay alpha](../docs/GAMEPLAY_RUNTIME.md) loads original starting
fleets and field/crop data, executes recovered motor/wheel and farm-transfer
rules, and supports basic cutting/cultivation/sowing through a partial native
bridge. Both maps pass 34 gameplay assertions. The RLE16 leaf matches original
code in 107 cases / 35,133,356 values, including both full fruit maps. Full
GE6/PhysX behavior, complete specialization startup and persistent saves remain
unfinished. Explicit harvest/silo scenarios relocate original equipment only
for reproducible trials; normal new-game placement uses original templates.

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
recovery manifest. The build now has eleven CTest
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

All **470 Lua sources recompile** after a repeated-literal adapter fixes an
LJD AST identity issue. Comparison against the original compiled chunks:

- **254 exact stripped-bytecode round trips**.
- **216 syntax-only results**, with behavioral equivalence still unverified.
- **Zero remaining syntax failures** in this recovery pass.

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
| x64 game, initial selection | 12,166 | 249 | 1 |
| x64 game, focused selection | Same catalogue | 44 | 0 |

The game selections overlap. Counts include C/C++ runtime and third-party
library code and are not completion percentages.

Automatic engine analysis completed within 300 seconds. **0x1400f8420**
exceeded its initial 20-second decompilation timeout. Two data-creation
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

1. Investigate the 216 non-identical Lua round trips and validate behavior.
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
