# ReFarm: original map scene reconstruction

**ReFarm** (`FS15EngineRebuilt.exe`) is an independently authored Win32/Direct3D 11 runtime.
It opens a native window, creates a device, swap chain, depth buffer and shaders,
uploads original terrain/object geometry and textures, and runs a free-camera render loop.
It does not load the original GIANTS executable. Bounded native routines recovered
into C++ provide GRLE decoding, Lua coordinate conversions and original TRS/affine
composition for I3D hierarchy, animated nodes and vehicle poses; the rest of the
host is authored and does not constitute a rebuilt GE6 engine.
Its resource pipeline reuses original GIANTS Engine 6 data and a verified mesh
decoder. See [component provenance](RECONSTRUCTION_STRATEGY.md). Full engine
startup and mission simulation remain incomplete.
The new experimental gameplay path, controls, recovered rules and physics
limits are described in [GAMEPLAY_RUNTIME.md](GAMEPLAY_RUNTIME.md). The sections
below describe the retained scene-viewer rendering path.

## Resources and rendering

The streaming I3D reader composes original translation, scale and XYZ Euler
rotations through the hierarchy in the documented right-handed coordinate system.
It resolves one untransformed root terrain. Each original 1025 x 1025 height
image produces a 2048 x 2048 metre terrain, sampled every two pixels into
263,169 vertices and 524,288 triangles with computed normals.

The installed RGB8 height PNGs contain a varying red/green pair and a zero blue
channel. The loader interprets this as `R * 256 + G`, scaled by
`heightScale / 65535`. This interpretation is inferred from the local resources;
it has not been compared with the original engine's native height queries.
An additional geometric check compares 272 sampled original Bjornholm road
vertices with the full-resolution DEM: median absolute height difference is
0.05 metres, versus 21.64 metres with the Z axis reversed. This checks alignment
against original geometry; it does not prove every height-query behavior.
Horizontal extent uses `(dimension - 1) * unitsPerPixel`, consistent with the
[GIANTS developer explanation](https://gdn.giants-software.com/thread.php?categoryId=16&threadId=893).

The surface uses original terrain weight maps, diffuse DDS mip chains and I3D
`unitSize` values: eight layers for Bjornholm and six for Westbridge Hills.
Their normalized blend is authored; GIANTS priority, parallax, normal-map and
dynamic crop/detail shader behavior is incomplete. Overview images are only a
terrain-only fixture fallback when no layers are defined.

Original binary meshes are decoded with pinned MIT I3DShapesTool source and an
authored .NET stream adapter. Float positions, normals, UVs, indices, subset
ranges and IDs are retained in a versioned local cache. Original shape IDs and
subset materials resolve through the scene. GPU instances preserve hierarchical
transforms; inverse-transpose normal matrices handle scaling. Object diffuse
textures retain mip chains and use alpha clipping. Water currently uses its
original underwater-fog color with simplified shading.

| Map | Decoded meshes | Rendered shape instances | Submitted object triangles |
|---|---:|---:|---:|
| Bjornholm | 1,155 | 338,165 | 15,071,727 |
| Westbridge Hills | 833 | 107,698 | 6,501,536 |

Instances include individual leaf/branch attachments; these are not tree or
building counts. Nonrenderable/trigger nodes, inherited hidden nodes and named
lower-LOD variants are filtered. LOD0 selection uses a static name rule;
distance-based LOD, clip distances and frustum culling remain incomplete.
The grid has no terrain LOD or collision. Original sky resources and shader
injections now provide sky blending, flags/bush wind and foliage lighting;
the native host provides a local shadow map. See [environment scope](ENVIRONMENT_RUNTIME.md).
Full cascades, tree skeletons, crop density, simulation and audio remain incomplete.

## Build and launch

The graphical launcher's **Resources > Prepare / repair assets** action now
orchestrates the existing local recovery/staging tools. See the
[resource preparation guide](LAUNCHER.md), including the original GAR-oracle
requirement for a fresh workspace. Interrupted preparation blocks host startup
until repaired. The manual commands below remain available.

The engine target needs Windows, MSVC, CMake and a Windows SDK. The frontend
uses LuaJIT; environment preparation also uses the local LuaJIT CLI and recovered
Lua sources. Mesh staging additionally needs Git and the
.NET 10 SDK/ASP.NET reference pack. Prepare inputs from your own game:

```powershell
.\scripts\prepare-engine-assets.ps1 -GameDir $env:FS15_GAME_DIR
.\scripts\setup_shapes_tools.ps1
& 'C:\path\to\python.exe' scripts\prepare_scene.py --game-dir $env:FS15_GAME_DIR
.\scripts\build.ps1 -PythonExe 'C:\path\to\python.exe'
.\FS15EngineRebuilt.exe --map Map01
.\FS15EngineRebuilt.exe --map Map02
.\FS15EngineRebuilt.exe --map Map01 --view overview
```

The current local package is `build/FS15GameplayAlpha4`. Launch its
`FS15MenuRebuilt.exe`, select Career, an empty slot, difficulty, map and Start.
Start now creates the experimental gameplay process and forwards difficulty.
Slot selection remains in the frontend; no persistent session is written. Existing saved simulations
remain read only and cannot start. No save is created.

Controls: WASD/ZQSD or arrows to move, Shift for speed, Space/Page Up to rise,
Ctrl/Page Down to descend, left or right mouse drag to look, Home to reset,
Escape to close. The initial view uses the camera saved in the original I3D.
`--view overview` selects the whole-map view. The packaged runtime needs no .NET.
F5/F6 change the preview hour; F7 toggles local shadows.

Resources resolve beside the executable or under a supplied `--root PATH`.
Logs and JSON reports go to `logs/`. No original executable, Steam, code snapshot
or GAR cipher oracle is required at runtime. Original inputs and local packages
remain excluded from GitHub.

## Validation

The scene-viewer checks remain part of CTest alongside the newer gameplay tests.
Two original-map suites initialize
Direct3D 11, render nine frames, require positive GPU occlusion-query samples,
and capture the actual color buffer before presentation. Separate queries require
positive object-mesh samples as well as terrain samples. Both maps ran on the
hardware driver at feature level 11.0. A separate authored fixture exercises
WARP, packed heights, hierarchy/rotation/scale, a mesh instance, hidden-LOD
filtering, material upload, camera math, DTD rejection, truncated caches and
missing resources without requiring original game files.

`scripts/verify-engine-package.ps1` runs both maps from the workspace and the
package using a different working directory. Their PNG hashes must match.
[Aggregate results](../analysis/engine-validation.json) record these checks.
The mesh cipher also passes 128 comparisons against original GE6 routine
`0x1402b3fa0`: 74,848 bytes, four seeds, partial/full blocks and counter carry.
[Native results](../analysis/shapes-native-validation.json).
Strict decoding recovered all 1,184 / 857 original entities, including splines,
before staging triangle meshes. [Recovery counts](../analysis/scene-recovery.json).
GUI smoke tests validate launch plans without spawning world windows; native
engine tests validate the renderer independently. Neither proves original
engine parity or a working farming simulation.

The packaged frontend was also operated through the live Windows UI:
Career, an empty slot, Normal difficulty, Bjornholm and Start opened the native
Map01 terrain window. Mouse dragging visibly changed the terrain projection;
Home restored the initial view. This observation is separate from smoke tests.

The updated scene package was then tested through Career, an empty slot, Normal,
Westbridge Hills and Start. The live native window displayed the original farm,
trees and terrain textures. Mouse look changed the view and Home restored the
saved I3D camera. The updated map window was left open for local exploration.
