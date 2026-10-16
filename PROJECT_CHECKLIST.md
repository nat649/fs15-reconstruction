# Project progress checklist

Updated: **2026-10-16**. Checked items have supporting local build, analysis
or execution evidence. They do not represent a percentage of the full game.

**Current native milestone:** the installed GIANTS Engine 6.0.2 opens its menu,
loads Bjornholm and executes recovered WheelsUtil source with 104 passing helper
comparisons in an isolated profile. The hybrid mode activates three recovered
wheel helpers and records all three being called by GE6 on Westbridge Hills.
Most original global gameplay remains in use;
this is an integration baseline, not a complete recovered-game rebuild.
Use `Launch FS15.cmd`; see the [native runtime](docs/NATIVE_ENGINE_RUNTIME.md).

**ReFarm experimental engine milestone:** the recovered frontend can launch an independently
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
- [x] Catalogue 12,200 native functions, including 30 entries from binding targets and four from explicit scene-vtable slots.
- [x] Attempt every non-thunk/non-external function in the existing catalogue with resumable exports.
- [x] Verify all 11,635 eligible local pseudocode exports with no remaining catalogue export failures.
- [x] Unblock the two large exports with a larger response limit and C-only output; retry Windows progress-file sharing conflicts.
- [x] Index 498 explicit native scripting descriptors across three registration functions, preserving the earlier 205 mappings.
- [x] Audit 32 code-label references, identify 30 distinct uncovered executable functions and decompile all 30.
- [x] Verify 16 slots in two constructor-observed scene vtables, discover four uncovered targets and export all four without splitting existing functions.
- [ ] Audit function discovery and code labels beyond this catalogue.
- [x] Export the catalogue's 31,393 known static call edges and count computed calls/import boundaries separately.
- [x] Audit known scripting and CRT dependency closures without describing them as a complete link graph.
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
- [x] Recover explicit 0x160-byte wheel records and typed registry/object views; keep unknown fields explicit.
- [x] Compare wheel registry lookup in 1,440 valid native cases and seven read adapters in 11,760 complete script frames.
- [x] Reconstruct seven 32-bit scripting bit operations and compare 21,763 complete native script frames.
- [x] Build these recovered components as a static portable C++17 core without the Windows frontend or original payloads.
- [x] Recover the native affine point-transform leaf and compare 12,000 complete vectors bit for bit; use it in the host Lua transform service.
- [x] Recover affine composition/inversion, rigid inverse points, WXYZ quaternion conversions and scale extraction; pass 36,000 bit-exact native comparisons.
- [x] Recover cached hierarchical world transforms and four script-frame conversions; pass 5,999 complete node arenas and 4,799 complete frames.
- [x] Connect recovered local/world coordinate adapters to ReFarm's existing world matrices and repeat 24 two-map gameplay reliability runs.
- [x] Recover bounded base/TRS dirty-state propagation, clip/mask inheritance, observer tracking and matrix setters; match 12,000 complete node arenas and 7,200 notification traces.
- [x] Check the world-preserving child path in 185 observed local-matrix changes, with singular-parent rejection in authored tests.
- [x] Recover axis rotation, TRS composition/decomposition, lazy cache/rebuild and six Lua getter/setter frames; compare 18,000 math results and 7,200 complete node arenas.
- [x] Integrate recovered TRS/affine math into ReFarm's I3D node hierarchy, animation updates and vehicle-root poses.
- [x] Recover bounded ordered link/reparent/unlink paths, including observer/state updates, native identity threshold and suppression; match 19,200 successive complete arenas/child buffers and notification traces.
- [x] Recover supported base/node/matrix/TRS construction, naming and recursive destruction with explicit allocator/registry/dispatch services; compare 2,500 constructor pairs, 7,500 renames and 250 owned forests.
- [x] Name recovered object header, identifier/class, attribute-map and shared-resource fields while preserving opaque bytes.
- [x] Recover native-layout attribute lookup/insertion/RB balance, Lua get/set frames and nonempty-map cleanup; compare 12,780 complete trees and directly run 25,200 original getter calls on reconstructed storage.
- [x] Document native String-to-scalar retained allocations and pointer residues separately from ordinary owned cleanup.
- [x] Recover known CharacterSet/data construction, node binding/rebinding, reference release, active/pending update removal and nested clip cleanup; compare 220 complete shared lifetimes.
- [x] Recover default property cloning/replacement and dispatch known shared references through the node destructor chain.
- [x] Track 82 bounded reconstructed native entries separately from 11,553 pseudocode-only entries, with source-pinned evidence.
- [ ] Recover native scene object allocation/destruction, ownership, remaining derived vtables, callback side effects and complete resource lifecycle.
- [x] Use recovered RLE8/RLE16 in host field-grid loading and reject incomplete run terminators, sample-count mismatches and nonfinite collision radii.
- [ ] Integrate validated tire-force math with recovered wheel slip, contact, load and rigid-body services.
- [ ] Complete the remaining GRLE variants and native density-service parity.
- [ ] Reconstruct the native GAR cipher independently.

## Future Nintendo Switch target

- [x] Confirm Nintendo Switch as the requested future target.
- [x] Isolate Windows symbol exports and add a portable-only build path.
- [x] Make RLE16 output endianness and 32-bit shift wrapping explicit.
- [x] Choose Switch homebrew `.nro` as the future packaging route; console work is deferred.
- [x] Cross-compile the recovered core for ARM64 Linux and pass all eight asset-free tests under QEMU in CI; this is separate from Switch hardware validation.
- [ ] Configure the console toolchain and packaging target.
- [ ] Cross-compile and run the recovered core on the Switch; host builds do not validate this.
- [ ] Recover/link the complete native engine and its platform/middleware interfaces.
- [ ] Validate console startup, menu, rendering, input, physics and complete farming/save sessions.

See [native recompilation and console limits](docs/NATIVE_RECOMPILATION.md).

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

## Actual GIANTS Engine 6 integration target

- [x] Confirm the native `-profile` argument and original mod source-loading path through local analysis.
- [x] Prepare a separate native profile and a local recovered-source probe without modifying the installation or normal saves.
- [x] Check the probe's 104 helper comparisons and mod isolation in an authored standalone fixture.
- [x] Validate a fresh actual-GE6 boot with the isolated profile through normal Steam startup.
- [x] Execute the recovered-source probe during actual-engine mission loading; all 104 comparisons pass.
- [x] Observe the original menu, successful Bjornholm loading and the in-world player HUD.
- [x] Add a dedicated native launch command, Steam routing, running-session guard and log evidence verifier.
- [x] Build/open a graphical launcher for native baseline, GE6 with recovered helpers, and the home runtime.
- [x] Add Play, Resources and Diagnostics pages, streamed progress, cancellation and locked configuration during preparation.
- [x] Add an illustrated library-style launcher, engine cards, a fixed launch bar and themed keyboard-accessible profile menus.
- [x] Name the experimental engine ReFarm in the launcher and game window; retain existing filenames and the home-mode alias.
- [x] Detect local FS15 through Steam libraries/app manifests, environment and saved workspace paths.
- [x] Prepare/repair original local resources with the existing recovery pipeline, content hashes, an import lock and interrupted-import launch guards.
- [ ] Remove the original-native GAR-oracle dependency from fresh-workspace asset extraction.
- [x] Preserve explicit direct startup without a silent Steam fallback; observe exit code 53 on the local Steam copy.
- [x] Activate three recovered wheel helpers after parity checks and record all three native vehicle call paths.
- [x] Check custom tire preservation and conditional unload restoration in an authored fixture.
- [ ] Validate native unload restoration and a complete save/load farming session with recovered helpers.
- [x] Add an isolated local-GE6 startup with a pinned modified native image and an authored local platform adapter; retain the unmodified direct/Steam modes.
- [x] Reach the original menu and Bjornholm HUD without Steam libraries loaded in the local game process; pass 104 comparisons and observe all three recovered wheel helpers.
- [ ] Validate local-GE6 startup with the Steam client completely closed; the first successful test left the preexisting client untouched.
- [ ] Validate complete local-GE6 farming, save/reload and graphics-restart sessions; Steam services/licensed DLC remain unavailable.
- [x] Prepare/check a profile junction without spaces to avoid native restart argument truncation.
- [ ] Validate a full graphics restart round trip through the new profile alias.
- [ ] Integrate recovered game behavior into the actual engine and validate complete sessions.

The Direct3D alpha remains an experimental viewer/test host. See the
[native entry-point scope](docs/NATIVE_ENGINE_RUNTIME.md).

## Experimental reconstructed frontend milestone

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
- [x] Advance gameplay with fixed host time steps and bounded stall recovery independently of rendering.
- [x] Pause gameplay with P and on focus loss/minimization; clear held/pending input and prevent suspended-time catch-up.
- [x] Match vehicle/crop/inventory state and captured renders across 15/30/60/144 Hz frame partitions on both maps.
- [x] Compare 28 original whole Lua chunks with recovered sources in 24 native scenario/fixture runs; keep shared source-slice/native-host limits explicit.
- [x] Observe the packaged Alpha4 focus-loss pause notice in a live native window.
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
