# Reconstruction strategy and component provenance

The target is Farming Simulator 15 / GIANTS Engine 6 reconstruction through
decompilation, recovered scripts, original resources and native behavior.
Prioritize existing engine components and verified interfaces when extending
the runtime. Keep track of what was recovered, reused, reconstructed or newly
authored. Original meshes and scene placements are the source of map objects.
Linux x64 is the intermediate portability target before the deferred Switch
homebrew port. The [headless Linux host](LINUX_PORT.md) exercises recovered CPU
components through authored platform transport. The shared recovered-frontend
host also builds with authored SDL2 transport and a local tester package;
original-resource Linux desktop validation and full native startup/rendering
remain separate milestones. See the [tester guide](LINUX_TESTER.md).

## Components currently used

The runtime target is **the actual GIANTS Engine 6**, including its renderer,
physics, scene/resource loading and lifecycle. Recovered game code must run on
that engine. Native analysis supports understanding and modifying it; building
a replacement engine around selected GIANTS services is not the project target.
The existing authored Direct3D host, named **ReFarm**, remains an experimental viewer and test
fixture. Its earlier milestones do not establish actual-GE6 integration.
See the [native engine entry point](NATIVE_ENGINE_RUNTIME.md).

| Component | Origin | Current use |
|---|---|---|
| Native runtime entry point | Installed original GIANTS Engine 6.0.2 executable | Normal Steam boot, original menu and Bjornholm observed with an isolated profile |
| Unified launcher | Authored C# Windows Forms / Python process orchestration | One interface selects the home runtime, original GE6 baseline, or GE6 with recovered helpers; native renderers remain separate |
| Native recovered-source integration | Recovered WheelsUtil source plus authored parity/activation adapters | 104 helper comparisons pass; three recovered helpers are activated and called by GE6 vehicles on Westbridge Hills |
| GUI logic | Recovered FS15 Lua sources | Compiled from source and executed in the frontend |
| Map hierarchy, camera, dimensions and repeat sizes | Original I3D XML | Parsed without rewriting the installed game |
| Buildings, trees, roads and props | Original version-3 `.i3d.shapes` containers | Decoded into local, lossless float/index caches; native GPU instancing |
| Terrain and materials | Original DEM, weight PNGs and diffuse DDS mipmaps | Height geometry, metre-based repeats and simplified material rendering |
| Sky and environment curves | Original sky I3D/DDS and recovered Environment/AnimCurve Lua | Original dome/textures and locally evaluated minute samples for preview lighting |
| Sky/wind/flag/foliage shaders | Original XML injection bodies | Compiled unchanged in authored HLSL bindings; original material animation parameters |
| Mesh decoder | Pinned MIT [I3DShapesTool](https://github.com/Donkie/I3DShapesTool/tree/1a2ecf3125d6754d9100c342419003bf048dbec5) | Existing format implementation reused with an authored .NET stream-range adapter |
| GE6 mesh cipher | Original native `0x1402b3fa0` | Executed in an isolated comparison process; 128 comparisons pass |
| GAR cipher | Original native initializer/decoder | Existing local extraction oracle |
| RLE8 routine | Reconstructed C++ from native analysis | Differentially checked against original machine code |
| RLE16 routine | Reconstructed C++ from native `0x14028f770` | 107 comparisons including both full original fruit grids |
| Tire-force calculation | Reconstructed C++ from native `0x1400c24c0` | 3,300 comparisons / 13,200 float outputs; not yet integrated with playable physics |
| Starting fleets and dynamic wheels | Original career templates, vehicle XML/I3D | Thirteen / twelve objects, eleven distinct prepared configurations |
| Motor, wheel and farm-transfer rules | Recovered Lua | Executed with partial native services; no full PhysX integration |
| Vehicle animation and trailer state transitions | Recovered AnimatedVehicle / Trailer / SpecializationUtil | Original XML loader, timing, visibility, discharge and closure; 777 matching original-bytecode fixture samples |
| Native engine catalogue | Ghidra export plus explicit binding/vtable discovery | All 11,636 eligible functions exported locally; 30 binding targets and five scene-vtable targets discovered; analysis rather than compilable recovered engine source |
| Portable recovered core | Reconstructed native RLE/tire math, wheel lookup/read adapters, bit operations, affine/quaternion math, scene ownership/state, owned child-vector allocation and LOD registry/distance selection, common I3D attributes/pending physics metadata, TransformGroup/LOD and Camera callbacks/motion, supported attribute/property/node copy, recursive known-node clone/remap, Light metadata/XML/dirty/lifetime, spatial grid storage/migration and active LOD/Light pending consumption, plane/frustum extraction and volume/point queries, perspective/orthographic matrices and projective/view-basis math, supported Camera view/stereo assembly and shader-block packing through explicit GPU transport, and binary/XML animation loading/playback | 209 bounded native entries compiled as C++17; coordinate and TRS/affine composition run in ReFarm; native-layout scene/animation paths use authored allocator/registry/dispatch services; complete resource loading, rendering and startup remain open |
| Trigonometric math backend | Platform C++ math library | Recovered TRS algorithms call sin/cos/atan2/sqrt; original CRT math bodies are not recovered; Windows fixture outputs match bit for bit |
| Field/crop data | Original I3D definitions and GRLE8/16 | Ownership, density states and original crop block rendering |
| Windowing, XML adapters and Direct3D host | Authored code | Provides the implemented services around recovered resources/scripts |
| Linux frontend transport | Authored SDL2/SDL2_image window/draw/input adapter plus Expat XML transport | Shared frontend host and tester package; Linux CI uses authored fixtures, original-resource desktop test pending |
| File-stream operations | Four recovered native read/seek/size/tell paths with authored Win32/POSIX transport | 2,492 original-operation comparisons; connected to frontend script/DDS/font loading and Linux XML input; full VFS and Windows namespace/locking parity remain open |
| Buffered default-disk callbacks | Three recovered native read/seek/status paths over the existing disk methods | 2,163 complete-record/output/OS-trace comparisons; 32 KiB cached resource reads now used by the frontend; original open/close/write and wider VFS remain open |
| Simulation scheduling and pause | Authored host code | Fixed 60 Hz gameplay updates, bounded stall recovery and pause/input clearing; not recovered GE6 physics timing |

## Original engine components

The installed x64 runtime contains the game executable plus Steam and OpenAL
libraries. A standalone GIANTS Engine 6 renderer DLL was not found there.
The SDK includes GIANTS Editor 6.0.3 installers; they are additional local
references for editor/engine behavior, not a rebuilt game executable.

Native code analysis traced `externalShapesFile` loading through
`0x1401866c0`, the cipher-state constructor `0x14029f350`, the stream reader
`0x14029f490` and its buffer routine `0x1402b3fa0`. The native routine is
already reused directly as a local oracle. The comparison harness uses only
the pinned analysis image and does not inject into a running game.

The native entry point now uses the installed original engine with a separate
profile. The recovered WheelsUtil source runs through its supported script
loader and passes 104 helper comparisons. The original menu and Bjornholm were
observed in the actual engine. Hybrid mode now additionally replaces three
wheel helper methods after parity checks; native vehicle calls to all three
were recorded. Most original game code and all original native engine services
remain in use. Wider code replacement and full session validation remain
separate milestones. The experimental host remains useful for fixtures.

Original executable/code bytes, decoded geometry, textures and recovered Lua
stay local. GitHub contains authored code, dependency provenance/licenses,
aggregate validation and selected rendered screenshots.

## Next reconstruction targets

The [native recovery report](NATIVE_RECOVERY.md) records full-export scope,
catalogue limits and 498 observed native scripting descriptors. Wheel creation, property
updates, slip lookup, tire forces and differential dispatch are now traced to
native entry points; their full services are not integrated yet.

Recover the remaining native physics/density interfaces, original component
joints, complete specialization startup and persistent sessions. Restore full
terrain/crop material behavior, density placement, distance-based LOD, shadow
cascades and tree skeletons. The current [gameplay subset](GAMEPLAY_RUNTIME.md)
keeps authored physics/contact/trigger approximations explicit.
Validate placement, rendering and gameplay separately as those components land.
