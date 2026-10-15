# Reconstruction strategy and component provenance

The target is Farming Simulator 15 / GIANTS Engine 6 reconstruction through
decompilation, recovered scripts, original resources and native behavior.
Prioritize existing engine components and verified interfaces when extending
the runtime. Keep track of what was recovered, reused, reconstructed or newly
authored. Original meshes and scene placements are the source of map objects.

## Components currently used

| Component | Origin | Current use |
|---|---|---|
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
| Native engine catalogue | Full Ghidra pseudocode export | 11,599 eligible functions exported locally, two timeouts; analysis rather than compilable recovered engine source |
| Field/crop data | Original I3D definitions and GRLE8/16 | Ownership, density states and original crop block rendering |
| Windowing, XML adapters and Direct3D host | Authored code | Provides the implemented services around recovered resources/scripts |

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

The full original engine startup and mission services are not yet integrated
into the compiled host. Their globals, native interfaces and dependencies
need recovery before using additional components reliably. Reusing original
components remains an available implementation path; each bridge should have
an explicit scope and reproducible validation.

Original executable/code bytes, decoded geometry, textures and recovered Lua
stay local. GitHub contains authored code, dependency provenance/licenses,
aggregate validation and selected rendered screenshots.

## Next reconstruction targets

The [native recovery report](NATIVE_RECOVERY.md) records full-export scope,
remaining timeouts and 205 observed engine descriptors. Wheel creation, property
updates, slip lookup, tire forces and differential dispatch are now traced to
native entry points; their full services are not integrated yet.

Recover the remaining native physics/density interfaces, original component
joints, complete specialization startup and persistent sessions. Restore full
terrain/crop material behavior, density placement, distance-based LOD, shadow
cascades and tree skeletons. The current [gameplay subset](GAMEPLAY_RUNTIME.md)
keeps authored physics/contact/trigger approximations explicit.
Validate placement, rendering and gameplay separately as those components land.
