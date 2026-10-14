# Native 3D engine bootstrap

`FS15EngineRebuilt.exe` is an independently authored Win32/Direct3D 11 runtime.
It opens a native window, creates a device, swap chain, depth buffer and shaders,
uploads terrain geometry and a texture, and runs a free-camera render loop.
It does not load the original GIANTS executable or recovered native code.
This is a terrain foundation, not a reconstruction of the complete engine.

## Resources and rendering

The streaming I3D reader retains scene-node metadata and resolves one
untransformed root terrain. External `.shapes` geometry is counted but deferred;
node counts do not describe rendered objects. Each original 1025 x 1025 height
image produces a 2048 x 2048 metre terrain, sampled every two pixels into
263,169 vertices and 524,288 triangles with computed normals.

The installed RGB8 height PNGs contain a varying red/green pair and a zero blue
channel. The loader interprets this as `R * 256 + G`, scaled by
`heightScale / 65535`. This interpretation is inferred from the local resources;
it has not been compared with the original engine's native height queries.
Horizontal extent uses `(dimension - 1) * unitsPerPixel`, consistent with the
[GIANTS developer explanation](https://gdn.giants-software.com/thread.php?categoryId=16&threadId=893).

The surface currently uses each map's original `pda_map_H.dds` overview image,
including supplied BC mip levels. Buildings visible in that image are painted
details, not rendered building meshes. Original layered terrain materials,
foliage, vehicles, physics, audio and mission simulation are not implemented.
The grid has no terrain LOD or collision yet.

## Build and launch

The engine target needs Windows, MSVC, CMake and a Windows SDK; LuaJIT is only
required by the separate frontend. Copy the six map inputs from your own game:

```powershell
.\scripts\prepare-engine-assets.ps1 -GameDir $env:FS15_GAME_DIR
.\scripts\build.ps1 -PythonExe 'C:\path\to\python.exe'
.\FS15EngineRebuilt.exe --map Map01
.\FS15EngineRebuilt.exe --map Map02
```

The complete local package is `build/FS15EngineAlpha`. Launch its
`FS15MenuRebuilt.exe`, select Career, an empty slot, difficulty, map and Start.
Start creates the terrain process. Difficulty and slot are retained by the
frontend but do not configure simulation yet. Existing saved simulations
remain read only and cannot start. No save is created.

Controls: WASD/ZQSD or arrows to move, Shift for speed, Space/Page Up to rise,
Ctrl/Page Down to descend, left or right mouse drag to look, Home to reset,
Escape to close. The initial view shows the whole terrain from above.

Resources resolve beside the executable or under a supplied `--root PATH`.
Logs and JSON reports go to `logs/`. No original executable, Steam, code snapshot
or GAR cipher oracle is required at runtime. Original inputs and local packages
remain excluded from GitHub.

## Validation

The local build passes nine CTest suites. Two original-map suites initialize
Direct3D 11, render nine frames, require positive GPU occlusion-query samples,
and capture the actual color buffer before presentation. Both maps ran on the
hardware driver at feature level 11.0. A separate authored fixture exercises
WARP, packed height values, mesh counts, camera math, DTD rejection and missing
resources without requiring original game files.

`scripts/verify-engine-package.ps1` runs both maps from the workspace and the
package using a different working directory. Their PNG hashes must match.
[Aggregate results](../analysis/engine-validation.json) record these checks.
GUI smoke tests validate launch plans without spawning world windows; native
engine tests validate the renderer independently. Neither proves original
engine parity or a working farming simulation.

The packaged frontend was also operated through the live Windows UI:
Career, an empty slot, Normal difficulty, Bjornholm and Start opened the native
Map01 terrain window. Mouse dragging visibly changed the terrain projection;
Home restored the initial view. This observation is separate from smoke tests.
