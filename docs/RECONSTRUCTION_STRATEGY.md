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
| Mesh decoder | Pinned MIT [I3DShapesTool](https://github.com/Donkie/I3DShapesTool/tree/1a2ecf3125d6754d9100c342419003bf048dbec5) | Existing format implementation reused with an authored .NET stream-range adapter |
| GE6 mesh cipher | Original native `0x1402b3fa0` | Executed in an isolated comparison process; 128 comparisons pass |
| GAR cipher | Original native initializer/decoder | Existing local extraction oracle |
| RLE8 routine | Reconstructed C++ from native analysis | Differentially checked against original machine code |
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

Recover GIANTS material/shader behavior, terrain detail and crop density
rendering; restore distance-based LOD, sky and shadows; then recover mission
startup and the native interfaces required for vehicle/physics simulation.
Validate placement, rendering and gameplay separately as those components land.
