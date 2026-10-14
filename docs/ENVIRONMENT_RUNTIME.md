# Recovered environment and shader milestone

The scene host now loads the original `sky_day_night.i3d` dome mesh and its ten
day/sunset/night/sunrise/overcast DDS textures. It compiles five original GIANTS
shader injection bodies directly from the locally prepared XML:

| Resource | Original injection | Host binding |
|---|---|---|
| `skyShader.xml` | `VS_END`, `EMISSIVE_FS` | Dome depth, texture samplers and sky blend weights |
| `windShader.xml` | `GET_POSITION_VS` | Original material direction/frequency, mesh UVs and elapsed seconds |
| `flagShader.xml` | `GET_POSITION_VS` | Original displacement parameters, mesh UVs and elapsed seconds |
| `treeBranchShader.xml` | `GET_DIFFUSE_LIGHTING_FS` | Original two-sided foliage lighting with the sun direction/color |

The shader bodies stay unchanged inside authored HLSL input/sampler wrappers.
They are read from the user's local game files at runtime; they are not embedded
in repository source. Original material parameters and casts/receives-shadow
flags are preserved. The wind shaders animate bushes/flowers and flags; animated
tree skeletons have not been reconstructed.

## Lighting and time

`scripts/stage_environment.lua` executes the curve-initialization slice from the
recovered `Environment.lua` plus the recovered `AnimCurve` evaluator. It calls
no mission or native engine services. The original sky, ambient, diffuse, fog
and sun-rotation curves are sampled at all 1,441 minute positions, including
their original fog gamma conversions. A local versioned float cache retains
the results and source hashes are recorded in scene provenance.

The native host interpolates this cache for a selected hour, starting at 08:00
as specified by the recovered career mission. It applies the original fixed
sun Y/Z rotations and uses sRGB diffuse textures and an sRGB output view.
This is a clear-weather environment preview: full `Environment:new/update`,
weather scheduling, rain effects and mission time simulation do not run yet.

F5/F6 step the preview hour forward/back; F7 toggles local shadows. Animation
time advances while the native window runs. Deterministic captures use:

```powershell
.\FS15EngineRebuilt.exe --map Map02 --hour 8 --time 0 --smoke --capture day.png
.\FS15EngineRebuilt.exe --map Map02 --hour 22 --time 0 --smoke --capture night.png
.\FS15EngineRebuilt.exe --map Map02 --no-shadows
```

`--camera X Y Z --look-at X Y Z` selects an explicit reproducible viewpoint.
Both options must be provided together; they do not change the scene resources.

## Shadow service and limits

The D3D11 host renders terrain and alpha-tested object casters into one local
orthographic depth texture. It uses the original sun's resolution (2,048 square
for these maps), coverage distance, extrusion and slope setting. A comparison
sampler with a 3 x 3 kernel filters the result; hidden/nonrenderable nodes and
original shadow flags are respected.

The camera-relative projection and depth bias mapping are authored adapters.
They are not the full original GIANTS cascade implementation. The original
depth bias is adapted to this D3D depth range; exact native bias/cascade parity
has not been established. Outside the local coverage, surfaces remain lit.
Terrain/object normal maps, complete terrain blending, specular/reflection
behavior, skeletal foliage motion and the full light/shadow pipeline remain
incomplete.

## Verification

All eleven local CTest suites pass. `recovered_environment_render` renders five
fixed scenarios from actual Map02 resources and decodes the PNGs with standard
library code. It checks visible object samples, five compiled original shader
injections and the following image differences (RGB channel difference > 2):

- Original animation at t=0 versus t=1: **17,978 changed pixels**.
- 08:00 versus 22:00: **921,600 changed pixels**, with lower average night brightness.
- Local shadows enabled versus disabled at a second farm viewpoint:
  **212,089 changed pixels**.

A populated depth map is checked separately; it is not treated as sufficient
evidence of visible casting. Both maps also retain workspace/package render
parity checks from a different working directory. These checks validate the
implemented resource/shader path, not original full-game rendering equivalence.

[Environment results](../analysis/environment-validation.json),
[engine results](../analysis/engine-validation.json),
[source/cache provenance summary](../analysis/scene-recovery.json).

The packaged Map01 runtime was also opened as a visible Windows window.
F6 changed 08:00 to the recovered sunrise appearance; F7 visibly removed the
ground/building/tree shadows, and a second F7 restored them. This live observation
is separate from the deterministic GPU captures and tests.
