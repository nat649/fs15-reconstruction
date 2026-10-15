# Project progress checklist

Updated: **2026-10-15**. Checked items have supporting local build, analysis
or execution evidence. They do not represent a percentage of the full game.

**Current milestone:** the recovered frontend can launch an independently
compiled Direct3D 11 scene host for either base map. It renders original
height data, decoded buildings/trees/props, diffuse textures and terrain layers
at their original hierarchical transforms, with a free camera.
Original sky/shader resources, recovered hourly environment curves and a local
shadow service are also running. A gameplay subset now connects original
fleets/crops and recovered motor, field and transfer rules to partial native
services. Full gameplay remains incomplete. See [gameplay scope](docs/GAMEPLAY_RUNTIME.md),
[engine validation](analysis/engine-validation.json),
[menu validation](analysis/menu-validation.json) and
[detailed status](analysis/STATUS.md).

## Installation and analysis

- [x] Identify the local FS15 installation and version 1.4.2.0.
- [x] Inventory installed files and record original executable/archive hashes.
- [x] Capture the normally loaded native code section for local analysis.
- [x] Set up a pinned Ghidra toolchain and reproducible export scripts.
- [x] Catalogue 12,166 native functions and export targeted pseudocode.
- [x] Attempt every non-thunk/non-external function in the existing catalogue with resumable exports.
- [x] Verify 11,599 local pseudocode exports and record two unresolved functions after 120-second retries.
- [x] Index 205 additional explicit engine API descriptors and trace selected wheel/physics call paths.
- [ ] Resolve the two remaining full-export timeouts and audit function discovery beyond this catalogue.
- [ ] Recover compilable native source, types, globals and subsystem initialization from the exported pseudocode.
- [x] Map 112 observed Lua/native registrations from one routine.
- [ ] Recover signatures and behavior for the wider engine interface.

## Reconstructed native components

- [x] Compile the reconstructed Windows launcher and check its behavior.
- [x] Reconstruct the RLE8-to-bit-vector routine in C++.
- [x] Compare RLE8 with the original routine in 266 cases / 853,086 values.
- [x] Reconstruct the 16-bit RLE leaf and compare 107 cases / 35,133,356 values.
- [x] Read the observed version-1 GRLE8/16 grids with bounds and size checks.
- [x] Reconstruct the native tire-force calculation and compile it into FS15Core.
- [x] Compare tire-force math with original code in 3,300 cases / 13,200 float outputs.
- [ ] Integrate validated tire-force math with recovered wheel slip, contact, load and rigid-body services.
- [ ] Complete the remaining GRLE variants and native density-service parity.
- [ ] Reconstruct the native GAR cipher independently.

## Archive and script recovery

- [x] Reconstruct GAR 5.3 header/index parsing and extraction checks.
- [x] Recover 1,121 files from dataS.gar and dataS2.gar locally.
- [x] Verify extracted file hashes and selected binary format signatures.
- [x] Recover all 470 observed LuaJIT scripts as compilable Lua source.
- [x] Fix the decompiler's repeated-literal issue with an authored adapter.
- [x] Preserve empty-table initialization and later field assignments; verify evaluation order in an authored regression.
- [x] Use deterministic LuaJIT dumps to remove hash-order variation from bytecode checks.
- [x] Recompile 470 scripts and generate a local script package.
- [x] Establish exact deterministic stripped-bytecode matches for 308 scripts, including 54 additional matches with no previous matches lost.
- [ ] Fully validate the remaining 162 syntax-only scripts.

## Main-menu milestone

- [x] Compile a separate menu executable with no original-engine dependency.
- [x] Execute 36 recovered GUI/dependency Lua sources in the new host.
- [x] Load the original menu XML, profiles and English localization.
- [x] Implement the required read-only XML/native property interface.
- [x] Render recovered textures and the original bitmap font.
- [x] Display all seven Steam-edition main-menu buttons.
- [x] Check recovered mouse callbacks and keyboard focus/activation.
- [x] Exercise recovered Credits, Back and Quit behavior.
- [x] Render 120 native smoke-test frames without missing assets.
- [x] Observe the reconstructed main menu in a real Windows window.
- [x] Package a directly runnable local folder and ZIP.
- [x] Verify package execution from a different working directory and matching render.
- [x] Display the recovered Career screen with twenty selectable slots.
- [x] Read existing save metadata without changing save files.
- [x] Block save deletion/writes and mark unsupported metadata unavailable.
- [x] Exercise list wheel scrolling and selection through slot twenty.
- [x] Open the recovered Difficulty screen and test all three choices.
- [x] Open the recovered Map Selection screen with both base-map previews.
- [x] Preserve selected slot, difficulty and map at the world-loading boundary.
- [x] Test Career/Difficulty/Map Back navigation and compare packaged captures.
- [x] Observe Career, mouse-wheel scrolling, Difficulty and Map Selection in the live Windows frontend.
- [x] Exercise native metadata/XML fixtures, malformed inputs and read-only guards.
- [x] Support UTF-8 text truncation and fix dark-text rendering artifacts.
- [ ] Implement Settings, Tutorials and Achievements screens.
- [ ] Execute a reconstructed equivalent of the full game startup path.

## Toward a playable game

- [x] Start an authored native Direct3D 11 device, swap chain and depth buffer.
- [x] Parse original I3D scene metadata and resolve both base-map height resources.
- [x] Render both original height maps as 3D terrain (524,288 triangles each).
- [x] Upload the original overview DDS including its mip chain as a temporary terrain material.
- [x] Replace the overview surface with original diffuse/weight terrain layers at I3D repeat sizes.
- [x] Decode all original shape-container entities for both maps with a pinned existing decoder.
- [x] Compare the decoder cipher with original GE6 native code in 128 cases / 74,848 bytes.
- [x] Load original building, tree, road and prop meshes, UVs, subsets and diffuse DDS textures.
- [x] Compose original parent transforms, rotation and scale in the I3D right-handed coordinate system.
- [x] Render original mesh instances and filter hidden, nonrenderable and named lower-LOD nodes.
- [x] Check Bjornholm relief alignment against 272 sampled original road vertices (0.05 m median error).
- [x] Start at the camera stored in each original I3D and support an overview camera.
- [x] Observe the packaged Westbridge Hills scene launching from the live frontend, with mouse look and reset.
- [x] Load the original sky dome mesh and day/sunrise/sunset/night/cloudy texture sets.
- [x] Execute recovered Environment curve initialization and AnimCurve sampling locally.
- [x] Compile original sky, wind, flag and foliage-lighting injection bodies with host bindings.
- [x] Apply original animation parameters and animate flags/bushes using elapsed time.
- [x] Preview recovered hourly sky, sunlight and fog, with sRGB texture/output handling.
- [x] Render a local shadow map using original sun settings and object shadow flags.
- [x] Compare rendered motion, night brightness and visible shadow casting in fixed image tests.
- [x] Observe packaged Map01 sky/hour changes and shadow toggling in a live native window.
- [x] Support free-camera keyboard movement, mouse look and reset.
- [x] Launch the terrain engine from a new Career selection through Start.
- [x] Observe native Start launch, mouse camera rotation and Home reset in the live window.
- [x] Validate visible GPU output for both maps and identical packaged renders.
- [x] Exercise software-rendered authored fixtures and invalid-resource rejection.
- [ ] Reconstruct scene graph and complete I3D resource loading.
- [ ] Recover remaining GIANTS terrain/object shaders, shadow cascades and animated tree skeletons.
- [ ] Restore distance-based LOD, culling, crop density and terrain detail rendering.
- [ ] Execute the complete mission startup and game-world services.
- [x] Stage both original starting fleets, eleven configurations and linked wheel models locally.
- [x] Render thirteen / twelve original starting objects and switch among motor vehicles.
- [x] Execute recovered motor curves, gearing, pedal/brake and wheel-control Lua.
- [x] Execute recovered AnimatedVehicle loading/update and original XML animation sequences.
- [x] Execute recovered Trailer loading/state transitions, discharge timing and automatic closing.
- [x] Match recovered animation/dispatch/tipping source with original bytecode in 777 fixture samples.
- [x] Preserve typed I3D user attributes and render vehicle visibility changes through native adapters.
- [x] Validate original animated farm discharge and inventory conservation on both maps.
- [x] Observe Alpha2 Career Start, original pipe opening/closing through O, and cutter E/B input in a live window.
- [x] Verify packaged original animation resources and matching starting-fleet/discharge renders on both maps.
- [x] Drive through temporary terrain/contact services and animate original wheel nodes.
- [x] Block centre-capsule movement against original static wall/trunk triangles.
- [x] Attach compatible nearby tools with a temporary kinematic hitch service.
- [x] Recover original field ownership/parallelograms and both 4096-square fruit grids.
- [x] Render original crop blocks/textures from recovered type and growth state.
- [x] Execute recovered harvest, cultivation and sowing functions on the exercised density interface.
- [x] Harvest original mature crops with original combine/cutter work areas in reproducible map scenarios.
- [x] Fill trailers at original silo triggers with stock/capacity/type conservation.
- [x] Discharge through recovered trailer and tipping-station rules.
- [x] Transfer grain through the original combine pipe animation/raycast and recovered unloading rules.
- [x] Refill the original seed tool at original supply triggers and charge the original unit price.
- [x] Exercise 43 gameplay assertions on each map and native collision/GRLE fixtures.
- [x] Observe packaged silo filling through R, 5,000 L conservation, E detach/attach, Tab and camera orbit in a live window.
- [x] Observe Career Start opening the new gameplay path and attach/activate the original starting cutter through E/B.
- [ ] Observe and validate a complete field-to-silo cycle through live user input.
- [ ] Reconstruct physics and vehicle simulation.
- [ ] Implement terrain, crops and farming interactions.
- [ ] Complete native drivetrain, suspension, articulation and vehicle/implement collisions.
- [ ] Restore exact foliage/detail rendering, procedural grain fill surfaces and unloading particles/effects.
- [ ] Implement crop growth, fertilizer/straw behavior and the complete specialization pipeline.
- [ ] Reconstruct audio playback and mixing.
- [ ] Implement save/load and validate a complete single-player session.
- [ ] Reconstruct networking and multiplayer behavior.
- [ ] Validate complete engine/game behavior against the original.

## Repository and local delivery

- [x] Publish authored code/tools and English documentation on GitHub.
- [x] Keep original binaries, recovered game code/assets and local packages out of Git.
- [x] Publish this checklist and aggregate validation metadata.
- [x] Show captured renders of both maps in the GitHub README.
- [x] Document public contribution rules, local-only resources and dependency notices.
- [x] Audit tracked files and reachable history for excluded payloads and common credential patterns.
- [x] Add asset-free GitHub checks and regression tests for historical credential/path detection.
- [x] Make the repository public after history review and successful asset-free GitHub checks.
- [ ] Select a project-wide license for the authored code; public visibility alone does not select one.
- [ ] Provide a fully reconstructed playable game executable.

Asset extraction currently uses a pinned original cipher oracle. The menu
and terrain runtimes do not use that oracle, an original code snapshot or the
original game executable. Native routines are used separately as local validation
oracles. Further original-component bridges remain part of the reconstruction
strategy. A new-game selection starts original-map scene exploration;
existing saved simulations and unsupported destinations remain unavailable.
