# Project progress checklist

Updated: **2026-10-21**. Checked items have supporting local build, analysis
or execution evidence. They do not represent a percentage of the full game.

**Exclusive active focus:** original GE6 startup on Linux. The
[native-instruction launch path](docs/GE6_LINUX_NATIVE_BOOT.md) keeps the original
engine code and targets its OS dependencies. ReFarm and object-preview work
are fixtures, not the active porting target.

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
- [x] Catalogue 12,207 native functions, including 30 binding targets, five scene-vtable targets and six memory-stream targets.
- [x] Attempt every non-thunk/non-external function in the existing catalogue with resumable exports.
- [x] Verify all 11,642 eligible local pseudocode exports with no remaining catalogue export failures.
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
- [x] Recover native key selection/easing, compressed quaternion interpolation, four-track controls and bounded TRS pose application; compare 4,320 clips, 42,444 channel records and 2,240 successive animation arenas.
- [x] Retain native shared-clip selection order, scale-cache refresh, visibility threshold and origin-zero propagation; compare 28,545 observer notifications.
- [x] Recover the binary animation payload reader, variable-size key packing, ordered insertion, lookup generation and clip registration; compare 90 synthetic files and one original 56-channel/326-key resource.
- [x] Connect loaded animation resources to recovered playback/scene services; compare 637 resource-to-pose updates with original code.
- [x] Recover seven XML animation callbacks and native scalar/vector numeric grammar; compare 7,189 callback states, 12,015 scalar results and 6,000 vector frames.
- [x] Validate decoded animations from three original I3D resources (180 channels / 1,078 keys), including native interpolation, degree conversion and rejected-key retention.
- [x] Recover common I3D node attributes and pending body/joint construction, flags, density and detached destruction; compare 2,043 common node lifetimes and 800 property constructors.
- [x] Compare 1,403 common attribute records sampled from both original maps and an original vehicle tool; preserve native ordering and source-ID duplicate behavior.
- [x] Recover owned pointer-vector reserve/grow/alias-append and run existing hierarchy/state/TRS paths on separately allocated nodes; compare 10,800 successive forest states, 2,763 child-vector relocations and 120 complete lifetimes.
- [x] Audit the LOD constructor table and discover its missing child-token function; preserve cumulative creation provenance across 32 explicit slots including the later Camera audit.
- [x] Recover LOD construction, registration/move/swap-removal, packed child/inherited tokens, distance cache, scheduling and supported lifetime paths; compare 12,000 distance queries and 36 mixed LOD/TRS forest lifetimes.
- [x] Recover TransformGroup/LOD begin and non-shape end callbacks plus their dynamic float-vector parser/growth; compare 87 complete forest lifetimes, including 63 complete original subtrees.
- [x] Bind recovered XML animations through source IDs emitted by the group callbacks; compare 435 playback updates, owned matrix/state propagation and loaded LOD registration.
- [x] Recover Camera construction/lifetime, I3D/script creation, parent-relative world pose and original follow/smoothing; compare 400 lifetimes, 127 XML cameras and 1,524 motion updates.
- [x] Audit the Camera table and retain inherited TRS/LOD dispatch across four tables / 32 slots; validate Camera-to-LOD world updates.
- [x] Recover typed attribute cloning/map assignment, detached body/joint property copy, common TRS copy and Camera/LOD clone wrappers; compare 1,800 values, 900 properties and 90 complete scene lifetimes.
- [x] Preserve native map self-copy clearing, independent string allocation, kind-4 retention and LOD clone distance-count behavior.
- [x] Recover native pointer remap insertion/RB balancing/cleanup and clone-list vector instantiations; compare 5,490 complete remap states and 3,000 vector states.
- [x] Recover recursive TRS/Camera/LOD cloning and ordered child attachment; compare 564 copied nodes, 99 complete lifetimes and 63 complete original subtrees.
- [x] Verify duplicate-root null behavior and run 564 original lookup calls directly on reconstructed remap storage.
- [x] Recover Light construction, decoded I3D begin, cone/dirty/LOD-token state and pending-only cleanup; compare 300 constructors/destructors and 1,104 complete forest states.
- [x] Add explicit Light virtual dispatch and clone handling; compare 6,460 complete node records, 206 cloned nodes and six original Light subtrees.
- [x] Audit the Light table; retain cumulative discovery provenance across five tables, 40 slots and 17 distinct targets.
- [x] Recover spatial grid initialization/migration, cell storage, packed tokens and add/move/swap-removal; compare 72,816 index queries, 5,760 grid mutations and 48 nonempty migrations.
- [x] Recover Light bounds, active registration/removal/destruction and the original manager consumer with Shape/audio queues empty; compare 208 flushes, 26 forest lifetimes and 66 active Light deletions.
- [x] Verify retained old per-cell allocations during grid reinitialization and compare all 662 retained buffers; keep authored reclamation explicit.
- [x] Recover plane normalization/matrix extraction and six fixed/dynamic volume predicates; compare 4,000 plane normalizations, 1,200 frustums and 57,600 predicates.
- [x] Recover ordered spatial volume/point queries and LOD/clip filtering; compare 5,392 queries, 3,363 truncated outputs and six original Light subtrees.
- [x] Reuse original-resource copies from the ignored local package only after matching recorded hashes when the installation drive is unavailable.
- [ ] Recover remaining render-view setup/variants, dynamic shadow-volume construction, Shape/audio pending/render consumers and real GPU resources.
- [ ] Recover Shape/reference clone branches, top-level finalization, skin remapping and shared-animation rebinding.
- [x] Recover perspective/orthographic matrices, full projective multiplication and native Camera basis conditioning; compare 18,000 complete math outputs and 18 original-I3D-parameter projection/view/frustum chains.
- [x] Recover the supported Camera view/stereo preparation and 196-byte shader block; compare 3,000 uploads, 2,418 view/cache chains and prepared-volume queries through explicit map/unmap transport.
- [ ] Recover render-view allocation/initialization, other view variants, real GPU buffer/backend operations and render consumption.
- [ ] Recover other derived I3D node callbacks, shape-end finalization, live body/joint backends, property observer effects and trigger teardown.
- [ ] Recover the original XML tokenizer/complete dispatch, file/archive/scene-loader integration, update scheduling, skin/skeleton binding and renderer consumption.
- [x] Track 260 bounded reconstructed native entries separately from 11,407 pseudocode-only entries, with source-pinned evidence.
- [ ] Recover native scene object allocation/destruction, ownership, remaining derived vtables, callback side effects and complete resource lifecycle.
- [x] Use recovered RLE8/RLE16 in host field-grid loading and reject incomplete run terminators, sample-count mismatches and nonfinite collision radii.
- [x] Recover render-view creation and teardown, compare 1,024 native lifetimes including partial buffer failures, and link them into the portable host.
- [x] Connect native wheel registry lookup and tire math through the recovered script force adapter; compare 4,096 complete frames and execute it in the portable host.
- [ ] Connect the recovered view's resource requests to an actual Linux graphics driver and validate drawing consumers.
- [ ] Recover full physics-manager creation and backend world initialization; bounded event/worker dispatch is covered below.
- [x] Recover native dispatch, synchronous stepping and the worker loop; compare 3,072 dispatch states, 1,024 synchronous states and 512 worker lifetimes / 2,044 completed steps.
- [x] Connect the recovered worker to real portable threads/auto-reset events, verify 24 transport lifetimes and execute 16 force-query steps in the CPU startup probe.
- [x] Recover native post-step ordering, wheel readback, nonempty contact collection, notification and history processing; compare six native entries.
- [x] Connect worker completion through native finalization to wheel readback, scene TRS, Camera/view, visibility and shader packing in the CPU probe (16 finalized steps / 17 view frames).
- [ ] Recover full controller finalizers, successful native deletion, backend scene/solver creation and physical contact generation.
- [x] Identify embedded Bullet classes through GE6 RTTI/factory analysis; pin a compatible upstream 2014 candidate and compare nine Windows structure sizes.
- [x] Compile recovered GE6 world/dispatcher/solver hooks over reused Bullet; compare 64 native lifetimes / 15,360 trajectory frames including generated rigid-body contacts.
- [x] Compare 96 nonempty native vehicle callback fixtures / 26,984 events and preserve native equal-key island sort order in 1,024 fixtures.
- [x] Recover five GE6 wheel frame/filter/closest-hit/raycast/finish paths; compare 6,656 original-code fixtures and exercise actual middleware rays against rigid bodies, triggers, ignored pairs and suspension limits.
- [x] Recover the GE6 wheel Jacobian, suspension spring setup and split-penetration phase; compare 2,048 Jacobians, 2,048 spring states and 6,640 correction iterations. Connect these paths to actual rays and chassis transform writeback in an isolated component integration test.
- [x] Recover the GE6 wheel velocity/friction/braking phase with an empty differential network; compare 1,024 states / 6,640 successive iterations and 18,448 gravity queries. Connect suspension support, transfer of initial wheel spin to chassis motion and braking in an isolated component test.
- [x] Recover motor/clutch/differential preparation and updates; compare 1,536 networks / 13,056 successive updates with original code.
- [x] Connect an experimental typed four-wheel raycast vehicle to the GE6 world/solver; demonstrate acceleration from rest, reverse, steering and braking in component tests. Its whole-class native comparison still records differences and is not a parity pass.
- [ ] Resolve whole-vehicle native parity differences, load original vehicle XML/joints/script control, complete GIANTS terrain algorithms/broadphase concurrency, rendering, scene loading and startup for an actual playable session.
- [ ] Integrate validated tire-force math with recovered wheel slip, contact, load and rigid-body services.
- [ ] Complete the remaining GRLE variants and native density-service parity.
- [ ] Reconstruct the native GAR cipher independently.

## Linux x64 intermediate target

- [x] Choose Linux x64 as the next platform target before Switch packaging.
- [x] Add an installable headless portability host using recovered Camera/view/shader, spatial visibility and lifetime paths, with authored CPU/platform transport.
- [x] Verify six installed-host CLI/filesystem checks on Windows; original resources are optional and read-only.
- [x] Verify all six installed-host checks on Linux x64 and publish an authored-only CI artifact; verify the downloaded ELF and archive hashes.
- [x] Port shared read-only XML/property and save-metadata adapters to Linux through Expat; retain XmlLite on Windows.
- [x] Add a text-only localization subset host and compare 1,425 English texts, 165 descriptions, seven labels and three factors with original Lua bytecode locally on Windows.
- [x] Verify Linux Lua/XML/save-metadata and all six installed-script-host checks; distribute both executables in the verified Linux artifact.
- [x] Compile the shared recovered-frontend host on Linux with authored SDL2 window/render/input transport and package a local tester with hash checks and result archives.
- [x] Recover four native file read/seek/size/tell paths; compare 2,492 operations with original routines and connect a read-only Windows/POSIX transport to frontend resource loading.
- [x] Recover native 32 KiB buffered read/seek/status callbacks; compare 2,163 operations and ordered OS events, then connect them to frontend resource reads.
- [x] Audit two memory-stream vtables, recover six read/seek/query paths, compare 13,366 operations and connect them to staged script loading.
- [ ] Validate the recovered menu with original local resources and interactive input on a Linux tester's desktop.
- [ ] Recover/link full native GE6 startup and scripting on Linux; the SDL frontend host does not establish this.
- [x] Execute the private original GE6 x64 instructions under a real Linux kernel in a local QEMU VM, without Wine/Proton or authored replacement engine; original CRT/global initialization reaches application entry `0x14000c9a0`.
- [x] Supply the native stack-probe NT_TIB prefix, bounded CRT clocks/heap/locks/text services, and original singleton/debugger calls through Linux OS transport; verify authored PE ABI/stack-argument and cross-process fixtures in dedicated Linux CI.
- [ ] Resolve the original application platform boundary: the current unmodified-instruction run stops at `SteamAPI_RestartAppIfNecessary`; no Steam service is synthesized. See [evidence and remaining startup dependencies](docs/GE6_LINUX_NATIVE_BOOT.md).
- [x] Port the existing Local GE6 platform adapter to the Linux instruction transport and accept its separately pinned, previously prepared image; retain its documented Windows bootstrap modification and keep unavailable Steam services false/null.
- [x] Execute original profile creation, startup configuration copy/timestamp checks and XML read on Linux through synchronous POSIX file and UTF-8/UTF-16 bindings; expose Linux absolute paths as `Z:/` to GE6's original Windows path resolver.
- [x] Reach original application construction after configuration parsing; preserve optional COM initialization failure.
- [x] Bind original Winsock startup/cleanup and bounded IPv4 UDP operations to POSIX sockets; validate actual loopback datagrams, error/lifetime behavior and authored PE ordinal-import/WSADATA ABI checks.
- [x] Enter original central engine initialization and write its own startup journal on Linux, with actual Linux CPU/memory information, main-thread timer/affinity services and a bounded read-only CPU registry view.
- [x] Pass original script-system unnamed-event creation and embedded Lua NT memory reservation/commitment through Linux OS bindings; test event wakeups and actual page protections/lifetimes.
- [x] Execute two original workers through `CreateThread` on Linux with per-thread NT_TIB stacks, separate TLS/FLS pools/values and synchronized OS registries; verify PE callbacks, suspended startup/resume, waits, return codes and FLS cleanup with authored Linux fixtures.
- [x] Transport the observed debugger thread-name packet to Linux names without swallowing other exceptions; connect non-alertable script-startup waits and retain explicit stops for unsupported APC waits.
- [x] Stage the unchanged original CRC manifest, checked map files and both GAR archives privately; observe original Physics System/Input System startup logs and reach `OpenAL32.dll!alcOpenDevice`.
- [x] Connect all 32 original OpenAL imports to pinned Linux OpenAL Soft; observe original audio initialization with 512 sources in the headless VM and verify real PCM loopback mixing and PE float ABI fixtures.
- [ ] Validate audible original game audio on a Linux desktop; the VM explicitly uses OpenAL Soft's silent backend.
- [x] Preserve the optional original D3D9 adapter probe's unavailable/failure path and let native startup continue without a synthetic GPU.
- [x] Connect original SDL/CRT console-handler registration to Linux SIGINT/SIGQUIT dispatch; verify real signals, handler order/ignore/removal, fresh callback threads/GS/TLS/FLS and a separate authored PE callback fixture.
- [x] Read the original PE resource directory, preserve the missing optional SDL icon result and register its original window procedure/class metadata; verify PNG/DIB pixels, resource errors/lifetimes and six-argument PE ABI with authored fixtures.
- [x] Connect Unicode module lookup and dynamic USER32 image/class exports; keep optional touch exports unavailable and let original SDL format their errors through a bounded UTF-16 message adapter.
- [x] Enumerate actual Linux displays/modes for original SDL through X11/RandR, including current-mode fullscreen requests validated against the server.
- [x] Create and show the original GE6 main window on X11 at 1280 x 720, retaining its native procedure at `0x14037cf00`; verify actual server map state.
- [x] Create and activate the original main-window OpenGL context through GLX/Mesa; distinguish it from the two hidden 10 x 10 probe contexts. See [bounded evidence](analysis/ge6-linux-x11-validation.json).
- [x] Verify authored X11 window lifetime, MS-ABI callbacks/stack arguments, real GL pixel readback and unavailable display-mode rejection in CI, without game files.
- [x] Read actual RandR gamma ramps and validate unchanged ramp writes/readback against the X server.
- [x] Bind original OpenGL resource/shader/buffer/draw operations through typed Microsoft-ABI adapters; test real shader compilation, texture upload, draw pixels and nine-argument calls in CI.
- [x] Stage the original shader archive, splash and font privately; observe original shader generation/compilation and successful OpenGL initialization in GE6's journal.
- [x] Render and visually inspect the original 1280 x 720 FS15 loading screen on Linux, with original draw/swap callers and source-pinned executable evidence. See [capture and boundaries](docs/GE6_LINUX_NATIVE_BOOT.md#first-original-loading-screen-2026-10-21).
- [x] Connect counting semaphores, drive-relative cache paths, actual POSIX address resolution, Linux account queries and OpenSSL random-provider requests; test buffer, error and resource-lifetime contracts.
- [x] Observe original FS15 startup scripts reporting loaded vehicle types; this does not validate complete scripting, menu interaction or vehicle simulation.
- [x] Resolve the original introduction timing fault caused by Xvfb's zero-Hz mode using a real configured 60 Hz Xorg virtual display, without changing original GE6 instructions; capture an original FOCUS introduction frame.
- [ ] Resolve the subsequent original startup wait during introduction playback; the current bounded test is terminated after 90 seconds and does not establish a functional menu.
- [ ] Complete original window/input event processing and remaining startup dependencies; the loading image does not establish a working menu.
- [ ] Validate complete original worker shutdown/CRT cleanup and extend thread/fiber/SEH/APC services where required; authored lifecycle checks do not prove complete game shutdown.
- [ ] Finish original Lua/script initialization, original game-resource loading and complete central engine initialization on Linux.
- [ ] Reach original GE6 core initialization, original window/renderer creation, menu and map loading through this Linux native-code path.
- [x] Recover nine OpenGL vertex/index buffer paths and compare 1,024 paired lifetimes with original GE6 code; add an optional SDL/OpenGL driver readback check. See [scope](docs/GL_BUFFER_RECOVERY.md).
- [x] Recover GE6 array/index/range draw dispatch and point-size cache; compare 4,096 driver states and 3,853 ordered calls with original code. Add actual-driver primitive framebuffer readback to the optional GL check.
- [x] Pass actual OpenGL buffer upload/map/readback/deletion on Linux x64 with Mesa llvmpipe under Xvfb.
- [x] Connect recovered GE6 TRS, projection/frustum, buffers and indexed draw to original object meshes, hierarchy and diffuse textures in an SDL/OpenGL integration target independent of ReFarm; capture Bjornholm on Windows/NVIDIA. See [evidence and temporary material/LOD limits](docs/GE6_OBJECT_RENDERING.md).
- [x] Display the full-resolution original Bjornholm DEM beside the objects, with a temporary uniform terrain material.
- [ ] Validate original-map object rendering and interactive camera on a Linux desktop; asset-free framebuffer integration checks are separate.
- [ ] Connect native GE6 shaders/material dispatch, terrain layers, dynamic LOD, vegetation effects, fleet rendering and mission/physics state to this draw path.
- [ ] Recover and connect Linux graphics/input/audio/filesystem/threading backends and complete original-resource loading.
- [ ] Validate original maps, vehicles, farming and persistent save/reload on Linux.

See [Linux build/run instructions and current limits](docs/LINUX_PORT.md).

## Future Nintendo Switch target

- [x] Confirm Nintendo Switch as the requested future target.
- [x] Isolate Windows symbol exports and add a portable-only build path.
- [x] Make RLE16 output endianness and 32-bit shift wrapping explicit.
- [x] Choose Switch homebrew `.nro` as the future packaging route; console work is deferred.
- [x] Cross-compile the recovered core for ARM64 Linux and pass all thirty-two asset-free component/transport tests under QEMU in CI; this is separate from Switch hardware validation.
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
