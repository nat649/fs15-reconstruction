# FS15 Reconstruction

Early-stage reverse engineering and component reconstruction of **Farming
Simulator 15 for Windows**, based on a locally installed copy of version
**1.4.2.0**.

**Source and tooling repository:** original game files, recovered game sources,
raw decompiler exports and playable asset packages are kept local. Reproducing
the game-resource results requires your own FS15 1.4.2.0 installation. The
repository does not provide a ready-to-download copy of the game.

[Progress checklist](PROJECT_CHECKLIST.md) · [Build instructions](#build) ·
[Linux tester guide](docs/LINUX_TESTER.md) ·
[Contributing](CONTRIBUTING.md) · [Project and dependency notices](NOTICE.md)

**Exclusive active focus: original GE6 startup on Linux.** The new
[native-instruction launch path](docs/GE6_LINUX_NATIVE_BOOT.md) preserves original
x64 engine code and works on missing OS dependencies. ReFarm and the object
preview remain fixtures. Complete original-engine Linux boot is not yet validated.

**This is not a complete game rebuild. The recovered frontend and original
map scenes and an experimental gameplay subset run in a compiled native host;
the full engine and farming simulation remain incomplete.**

The project follows decompilation and reconstruction of the installed GIANTS
Engine 6 game. Original scripts, geometry, textures, scene transforms and native
routines are reused or examined where practical. See the
[reconstruction strategy](docs/RECONSTRUCTION_STRATEGY.md) for component provenance.

**Linux direction:** port the actual GE6/game code to run natively without
Proton, using the original Windows version as the behavior and resource
compatibility reference. Matching that version is the target, not a result
already achieved. The current Linux build runs recovered components and an
experimental frontend host; complete GE6 startup, native graphics/physics/audio,
playable worlds and persistent simulation saves remain open. Original-resource
menu/input validation on a Linux desktop is also pending.

**Runtime target: the actual GIANTS Engine 6.** The Direct3D alpha is an
experimental viewer/test host. On Windows, the native entry point has reached the
original menu and Bjornholm, with 104 recovered-source helper comparisons
passing inside GE6. See the [native runtime instructions](docs/NATIVE_ENGINE_RUNTIME.md).

**Launch:** build with `scripts/build-launcher.ps1`, then double-click
`Launch FS15.cmd`. The graphical launcher offers GE6 with recovered wheel
helpers, **ReFarm** (the experimental reconstructed engine), and an original-GE6 baseline. It remembers local
settings and keeps native saves under `build/native-engine/profile`.
The **Resources** page detects the local installation and prepares/repairs map,
texture, mesh, recovered-script and starting-fleet caches with progress and
cancellation. See [resource preparation and fresh-workspace requirements](docs/LAUNCHER.md).
See [launcher modes and requirements](docs/UNIFIED_LAUNCHER.md).
The existing `FS15MenuRebuilt.exe` / `FS15EngineRebuilt.exe` packages are the
separate experimental host; they are not the actual-engine entry point.

The launcher has a visual library layout: original-map scene previews, clickable
engine cards, a persistent launch-profile bar and a prominent PLAY button.
**ReFarm** names the authored experimental host; the original GE6 runtime remains
a separate option. Existing executable filenames and the `--mode home` alias are
kept compatible; `--mode refarm` is also supported.

**Experimental local GE6 startup:** choose **Local GE6 (experimental)** under
Settings & activity > Native startup. An isolated modified original runtime now
reaches the original menu and Bjornholm without loading a Steam library. Steam
was already open during the test; a fully closed-client launch and complete
save/reload sessions remain unvalidated. Steam services and licensed DLC are
unavailable. See [preparation, evidence and scope](docs/LOCAL_ENGINE_RUNTIME.md).

## Current results

- **Original objects through recovered GE6 drawing:** a separate SDL/OpenGL
  integration target now displays original buildings, trees, roads and props
  using recovered GE6 transforms, projection/frustum, buffers and indexed draw.
  Bjornholm was captured on Windows/NVIDIA with original textures and DEM geometry.
  Materials, lighting, terrain color and LOD policy are temporary; gameplay is
  not connected. Double-click `Launch GE6 Objects.cmd` after building.
  See [capture, build/run instructions and limits](docs/GE6_OBJECT_RENDERING.md).

- **Connected physics results and scene/view preparation:** worker completion
  now runs recovered contact collection, native post-step order, wheel readback
  and enter/leave/stay events. The CPU probe connects 16 finalized steps to
  scene TRS, Camera/frustum/visibility and 17 shader-block uploads. Six more
  native entries have differential evidence. Backend samples and scene binding
  are authored fixtures; actual scene creation, solver and GPU rendering remain
  open. See [integration and limits](docs/PHYSICS_POST_STEP_RECOVERY.md).

- **Native physics scheduling:** the recovered dispatcher, synchronous step and
  worker loop now run over an authored real thread/auto-reset event transport.
  Comparisons cover 3,072 dispatch states, 1,024 synchronous states and 512
  worker lifetimes / 2,044 completed steps. The portable startup probe performs
  16 worker-thread force queries and joins cleanly. Full physics scene creation,
  solver, controller finalizers and GPU startup remain open. See
  [physics dispatch scope](docs/PHYSICS_DISPATCH_RECOVERY.md).

- **Native startup, 3D and physics:** recovered render-view creation/destruction
  now initializes state and requests/releases two buffers through explicit
  driver services. A recovered wheel-force adapter connects the native wheel
  registry, tangent and tire math to script results. Comparisons cover 1,024
  view lifetimes and 4,096 force frames. Both paths run in the portable CPU
  probe; real GPU and physics-world startup remain open. See
  [implementation and evidence](docs/STARTUP_3D_PHYSICS.md).

- **Native recompilation:** 30 uncovered binding targets
  were identified as functions and decompiled. The enlarged catalogue has
  **11,667 eligible exports**, after five scene-vtable targets and six
  memory-stream targets and 25 additional eligible physics-table targets were
  found through explicit table audits. A separate
  portable C++ core now builds the RLE
  decoders, tire math, wheel lookup/read adapters, seven bit operations and the
  native affine point transform. Together with subsequent scene, animation,
  Camera and file-stream recovery, the ledger now records
  **260 native entries on bounded tested paths**. The affine component passes
  36,000 math comparisons, 5,999 complete node-arena comparisons and 4,799
  complete script-frame comparisons against original code, bit for bit.
  ReFarm uses the recovered local/world adapters through an authored matrix bridge.
  Full GE6 linking remains pending; the future
  Switch homebrew target is deferred. See [recompilation scope](docs/NATIVE_RECOMPILATION.md).
- **Scene state recovery:** five native paths now reproduce dirty/visibility,
  clip-distance/mask propagation, observer-presence tracking and base/TRS matrix
  setters. 12,000 complete synthetic node arenas and 7,200 notification traces
  match original code, including 185 world-preserving child matrix changes.
  This is a portable component; native allocators, complete object destruction and arbitrary
  observer side effects are not reconstructed. See [scope](docs/SCENE_STATE_RECOVERY.md).
- **TRS update recovery:** eleven additional entries cover native rotation and
  composition/decomposition, lazy caches and six Lua setters/getters. 18,000 math
  comparisons and 7,200 complete scene arenas pass; all recorded Windows floats
  were bit-identical. Platform libm remains explicit. ReFarm now uses recovered
  TRS/affine math for original I3D nodes, animation hierarchies and vehicle poses.
  See [TRS scope and evidence](docs/SCENE_TRS_RECOVERY.md).
- **Scene hierarchy recovery:** ordered attachment, reparenting and removal now
  reproduce three additional bounded native paths. 19,200 successive complete
  arenas/child buffers and notification traces match the original code. Native
  allocation, object destruction and callback side effects remain open. See
  [hierarchy scope and evidence](docs/SCENE_HIERARCHY_RECOVERY.md).
- **Object lifecycle recovery:** thirteen additional paths initialize base,
  scene, matrix and TRS objects, rename them and destroy supported owned forests.
  Comparisons cover 2,500 constructors, 7,500 renames and 250 recursive forests
  containing 2,470 deleting destructions. Platform allocation, ID/registry and
  known dispatch services are explicit authored boundaries. Shared resources
  and active triggers remain open. See
  [lifetime scope and evidence](docs/SCENE_LIFECYCLE_RECOVERY.md).
- **Native attributes:** eight additional paths recover the actual attribute
  tree layout, insertion/balance, Lua reads/writes and nonempty-map destruction.
  12,600 setters, 25,200 getters and 12,780 complete trees match original code;
  25,200 original getter calls also read reconstructed trees directly. Native
  text-to-scalar allocation retention is measured explicitly. See
  [attribute scope and evidence](docs/SCENE_ATTRIBUTES_RECOVERY.md).
- **Shared animation ownership:** twelve additional paths recover CharacterSet/data
  construction, binding/rebinding, update membership and last-reference cleanup.
  220 scenarios cover 4,310 node destructions and 4,297 owned clip channels.
  See [ownership scope](docs/SHARED_SCENE_RECOVERY.md).
- **Native animation playback:** ten further entries recover key sampling,
  packed quaternion interpolation, four-track controls and bounded TRS pose
  application. 4,320 clip samples, 42,444 channel records and 2,240 successive
  animation/scene arenas match original code, including 28,545 notifications.
  Complete loading, scheduling and skeletal rendering remain open. See
  [animation scope and evidence](docs/ANIMATION_RECOVERY.md).
- **Binary animation resources:** seven further entries recover key construction,
  lookup generation and binary payload loading. 90 synthetic files and one
  original 56-channel/326-key resource match GE6; 637 updates verify loaded
  resources through recovered pose services. Complete
  scene/render startup remain open. See [resource scope](docs/ANIMATION_RESOURCE_RECOVERY.md).
- **XML animation callbacks:** nine further entries recover numeric grammar and
  group/clip/channel/key callbacks. 7,189 successive callback states match original
  code, including three original I3D files with 180 channels / 1,078 keys.
  The XML tokenizer, full scene integration and skeletal rendering remain open.
  See [XML scope and ownership quirks](docs/ANIMATION_XML_RECOVERY.md).
- **Common I3D node loading:** twelve further entries recover node attributes,
  TRS rebuilding and pending body/joint metadata. 2,043 complete common-node
  lifetimes match original code, including 1,403 attribute records sampled from
  the original maps and a vehicle tool. Active physics and complete scene loading
  remain open. See [scope and evidence](docs/SCENE_XML_RECOVERY.md).
- **Owned scene hierarchies:** recovered pointer-vector allocation now supports
  separately allocated scene objects through authored scatter transport. 10,800
  successive forest states and 120 complete lifetimes match original code,
  including 2,763 child-vector relocations and ordered observer notifications.
  See [ownership and allocation scope](docs/SCENE_OWNED_RECOVERY.md).
- **LOD recovery:** thirteen further paths recover group metadata, registration,
  distance-cache selection, child tokens and known teardown. 12,000 standalone
  distance queries and 36 complete mixed LOD/TRS forest lifetimes match original
  code. Full spatial/renderer consumption remains open. See
  [LOD scope and evidence](docs/SCENE_LOD_RECOVERY.md).
- **I3D group callbacks:** five additional entries connect TransformGroup/LOD
  creation, common attributes and owned attachment. 63 complete original subtrees
  and 435 animation updates on loaded nodes match native code. The tokenizer,
  other derived objects and full scene loading remain open. See
  [callback chain and evidence](docs/SCENE_GROUP_RECOVERY.md).
- **Camera recovery:** nine additional paths recover construction, I3D/script
  creation, parent-relative pose and native follow/smoothing. 400 camera lifetimes,
  127 XML creations and 1,524 motion updates match original code. Projection and
  rendering remain open. See [camera scope and evidence](docs/SCENE_CAMERA_RECOVERY.md).
- **Scene copy and cloning:** seven additional native paths recover attribute
  values/map assignment, detached body/joint property metadata, common TRS copy
  and Camera/LOD clone wrappers. 1,800 value clones, 900 property clones and 90
  complete scene lifetimes match original code. Recursive resource remapping
  remains open. See [copy scope and evidence](docs/SCENE_CLONE_RECOVERY.md).
- **Recursive scene cloning:** seven further paths recover the native pointer
  remap tree, clone-list allocation and recursive TRS/Camera/LOD assembly. 564
  copied nodes and 63 complete original subtrees match GE6. Shape/reference/resource
  finalization remains open. See [recursive scope](docs/SCENE_CLONE_TREE_RECOVERY.md).
- **Light recovery:** nine additional native paths recover construction, decoded I3D
  loading, cone metadata, dirty/LOD-token updates, pending-only cleanup and cloning.
  6,460 node records and 1,104 forest states match GE6, including six original Light
  subtrees from both maps and two tractors. The spatial extension below adds
  active registration and deferred consumption; GPU lighting remains open.
  See [Light scope](docs/SCENE_LIGHT_RECOVERY.md).
- **Spatial grid and Light consumption:** twelve further native paths recover
  cell indexing/storage, packed owner tokens, registration/movement/removal,
  Light bounds and the original LOD-then-Light pending order. 72,816 index
  queries, 48 nonempty grid migrations and 26 active forest lifetimes match
  GE6, including active deletion. Full Camera preparation, Shape/audio consumer paths
  and GPU resources remain open. See [spatial scope](docs/SCENE_SPATIAL_RECOVERY.md).
- **Native visibility queries:** eleven additional paths recover plane normalization,
  matrix-to-frustum extraction, fixed/dynamic volume predicates and ordered grid
  volume/point queries. 57,600 predicates and 5,392 queries match GE6, including
  output truncation and LOD cache effects. Full Camera/view preparation and
  rendering remain open. See [visibility scope](docs/SCENE_VISIBILITY_RECOVERY.md).
- **Camera projection math:** four additional native paths recover off-axis
  perspective/orthographic matrices, full projective multiplication and native
  Camera basis conditioning. 18,000 complete math outputs and 18 projection/view
  chains using original I3D parameters match GE6. Complete view/GPU preparation
  remains open. See [projection scope](docs/SCENE_PROJECTION_RECOVERY.md).
- **Camera view and shader block:** two more native paths assemble the Camera
  world/projection/frustum, stereo offsets and the original 196-byte shader
  block. 3,000 upload records and 2,418 view chains match GE6 through explicit
  world/map/unmap transport, including prepared-volume culling queries.
  Real GPU resources and driver integration remain open. See
  [view scope](docs/SCENE_VIEW_RECOVERY.md).
- **Linux x64 intermediate target:** `FS15PortableHost` is an installable headless
  executable that runs recovered Camera/view/shader, visibility and lifetime
  paths through authored CPU transport. It can inspect local resource presence
  without modifying the game. The complete engine, menu and renderer are still
  separate recovery milestones. See [Linux build/run instructions](docs/LINUX_PORT.md).
- **Linux frontend test:** `FS15MenuLinux` shares the existing recovered-menu
  host through authored SDL2 transport. A local package launcher checks hashes,
  exercises menu callbacks and collects screenshots/logs for a friend to return.
  Original-resource Linux desktop validation remains pending; world simulation
  is unavailable. See [tester instructions](docs/LINUX_TESTER.md).
  The optional `FS15ScriptHost` adds text-only localization loading with shared
  read-only XML/save adapters; Linux uses Expat and LuaJIT. Original-resource
  parity and asset-free Linux fixtures are reported separately.
- **Native file reads on Linux:** four recovered read/seek/size/tell paths
  match **2,492 operations** against original Windows routines. An authored
  read-only Win32/POSIX adapter connects them to frontend script, DDS/font and
  Linux XML loading. Signed 32-bit seek limits and EOF behavior are retained;
  full Windows namespace/locking and VFS parity remain open. See
  [file-stream recovery](docs/FILE_STREAM_RECOVERY.md).
- **Native buffered resource reads:** three further callbacks reproduce the
  original **32 KiB cache**, cached seeks and sticky refill-error state.
  **2,163 operations**, complete cache records, guarded outputs and **1,680
  ordered OS events** match the original callbacks and disk methods. The
  frontend now reads through this buffered path. Original allocation/open/close,
  writes and arbitrary derived streams remain separate work. See
  [buffered-reader recovery](docs/BUFFERED_FILE_RECOVERY.md).
- **Native memory streams:** six validity/read/seek/size/position/EOF paths
  match **13,366 original-operation comparisons** with complete records and
  guarded buffers. Script loading now follows disk → 32 KiB cache → memory
  stream → Lua. Two memory vtables exposed six previously uncatalogued small
  functions; original allocation/free and archive decryption remain open. See
  [memory-stream recovery](docs/MEMORY_FILE_RECOVERY.md).
- **GE6 physics backend:** native RTTI and factory analysis identify Bullet.
  A pinned 2014 upstream revision now supports recovered GIANTS world, dispatcher
  and solver hooks. **15,360 simulated frames** match original GE6 trajectories
  and contacts; **26,984 nonempty vehicle callback events** and 1,024 native
  sort fixtures match. This is actual rigid-body simulation, separate from
  ReFarm. Five wheel frame/filter/raycast/finish paths additionally match
  **6,656 native fixtures** with no observed arithmetic difference; actual
  middleware rays test owner/trigger exclusion and suspension limits. GIANTS
  terrain collision, the complete raycast vehicle constraint solver and full
  startup remain open. See [provenance and evidence](docs/GE6_PHYSICS_BACKEND.md).
- **GE6 suspension constraints:** three further paths recover the wheel Jacobian,
  spring setup and split-penetration phase. **2,048 Jacobian states, 2,048 spring
  states and 6,640 repeated correction iterations** match unchanged original
  code. An isolated integration fixture connects real world rays, recovered
  setup, solver iterations and actual chassis transform writeback. This
  isolates suspension correction; the assembled vehicle milestone follows below.
- **GE6 wheel velocity/friction:** the native `0x1401540b0` phase now applies
  suspension velocity impulses, tire-force limits and coupled wheel braking.
  **1,024 states / 6,640 successive iterations**, including 18,448 gravity
  queries, match unchanged original code with an empty differential network.
  A one-wheel integration fixture supports a chassis under gravity, transfers
  initial wheel spin into chassis motion, then brakes it to rest. Its fixed
  slip inputs and orchestration are authored test data. Original-vehicle loading
  remains open; motor/differential recovery follows below.
- **GE6 motor and vehicle assembly:** differential weight preparation and the
  complete motor/clutch/network update match **1,536 networks / 13,056 successive
  updates**. An experimental typed raycast-vehicle class connects all wheel
  phases and this drivetrain; four wheels accelerate from rest, reverse, steer
  and brake in component scenarios. The whole-class native comparison records
  unresolved motion/readback differences and is **not a parity pass**. Original
  vehicle XML/joints/script control, terrain/rendering and a playable Linux
  game remain open. See [measured limits](docs/GE6_PHYSICS_BACKEND.md).
- **OpenGL buffer recovery:** nine native paths now provide portable vertex/index
  storage, staging, direct mapping, discard, subdata upload and deletion.
  **1,024 paired lifetimes** match unchanged GE6 code through recording GL seams.
  The SDL/OpenGL check passes actual driver byte readback on Mesa in Linux CI;
  shaders, materials and scene drawing remain open. See
  [scope and build commands](docs/GL_BUFFER_RECOVERY.md).
- **GE6 OpenGL draw dispatch:** native array/index/range dispatch and point-size
  caching match **4,096 states / 3,853 ordered GL calls**, including original
  32-bit index-byte/range wrapping. The actual-driver check now adds primitive
  framebuffer readback; scene shaders, materials and complete draw submission
  remain open. See [scope](docs/GL_DRAW_RECOVERY.md).
- **Portability audit:** the current static catalogue has 31,396 known call
  edges. Its scripting roots reach 2,878 eligible functions, including 939 with
  computed calls. Those targets and concrete types still need recovery before
  full linking. All thirty-eight asset-free component/transport tests pass on
  Windows x64, Linux x64 and ARM64 Linux under QEMU, separately from the future
  Switch target. See
  [current connected-chain platform evidence](analysis/physics-post-validation.json),
  [current OpenGL/platform evidence](analysis/gl-buffers-validation.json),
  [Linux CI run](https://github.com/nat649/fs15-reconstruction/actions/runs/39226238887) and
  [porting blockers](docs/PORTABILITY_STATUS.md).
- **Actual GE6 execution:** the installed original engine initializes its OpenGL
  renderer and physics 5.9.4, opens its frontend and loads Bjornholm in a separate
  profile. The recovered WheelsUtil source executes through the native mod
  loader and passes all 104 helper comparisons. The hybrid mode now activates
  three recovered wheel helpers, with all three called by actual GE6 vehicles
  on Westbridge Hills. Most gameplay remains original; full recovered-game
  replacement is pending. Direct startup of this Steam copy still fails; the
  launcher retains normal Steam startup and an explicit direct mode.
  See the [bounded log evidence](analysis/native-engine-validation.json).
- **Native decompilation coverage:** a full pass through the existing Ghidra
  catalogue exported **11,667 / 11,667 eligible functions** as local pseudocode,
  with no remaining export failures. The last two exports were unblocked with
  larger response capacity and C-only output; 565 thunks or external entries
  are excluded. **498 explicit native scripting API
  descriptors** now guide wheel/physics interface recovery. This is analysis
  coverage, not a recompilable full engine. See [native recovery](docs/NATIVE_RECOVERY.md).
- **Recovered vehicle animations:** original `AnimatedVehicle` and `Trailer`
  loading/update logic now animate doors, trailer bodies and pipes. Tipping waits
  for original discharge timing and automatically closes when empty. Recovered
  source and original bytecode produce identical traces in **777 fixture samples**.
- **Reconstructed tire-force math:** native routine `0x1400c24c0` is now compiled
  from reconstructed C++, with **3,300 native comparisons / 13,200 float outputs**.
  It remains a standalone validated component; contacts, suspension and its
  integration into playable physics are still pending.
- **Experimental gameplay:** original starting fleets, selectable/drivable
  tractors and combine, animated wheels, compatible implement attachment,
  original field/crop data, cutting/cultivation/sowing, silo filling, tipping
  stations, combine-to-trailer transfer and seed refilling now have a running
  native/Lua bridge. Physics, density services and client rendering remain
  partial. No persistent session saves yet. See [gameplay scope and controls](docs/GAMEPLAY_RUNTIME.md).
- **Original density recovery:** the reconstructed 16-bit RLE leaf matches
  original GE6 in **107 comparisons / 35,133,356 values**, including both
  4096-square fruit maps. Crop block meshes and density states are reused.

- **Rebuilt frontend executable:** opens the recovered `MainScreen` in a
  Windows window without starting the original game executable or Steam.
  Runs 36 recovered Lua sources with authored XML, rendering, input and
  read-only save services. Career displays twenty slots and existing save
  metadata; empty slots lead to three difficulty choices and two base-map
  previews. Credits/Back and Quit work. Start launches the new terrain engine
  for an empty-slot selection; existing saved simulations cannot be loaded yet.
- **Original map scene reconstruction:** the Direct3D 11 host renders both
  original terrains, buildings, trees, roads and props. Local `.i3d.shapes`
  decoding recovers **1,155 / 833 meshes**, instanced through the original scene
  hierarchy. Original diffuse textures and **8 / 6 terrain weight layers** are
  loaded at their I3D repeat sizes. The camera starts at the original saved I3D
  viewpoint. Shaders are simplified; mission simulation and physics remain
  incomplete. See [engine notes](docs/ENGINE_BOOTSTRAP.md).
- **Native GE6 mesh-cipher validation:** **128 comparisons / 74,848 bytes**
  against the isolated original routine, including partial blocks and counter carry.
- **Recovered environment/shaders:** the original sky dome/textures, sampled
  Lua environment curves and five original shader injection bodies now run in
  the host. Flags and bushes animate, the hour changes sky/light/fog, and a local
  shadow map casts building/vegetation shadows. Dedicated CTest suites cover this,
  including fixed-image motion/night/shadow comparisons. Full GIANTS cascades
  and mission weather remain incomplete. See [environment notes](docs/ENVIRONMENT_RUNTIME.md).
- **Reconstructed C++ launcher:** reads the game's startup XML, chooses the
  x86 or x64 executable, and forwards command-line arguments. It still runs
  the original installed game engine.
- **Reconstructed engine routine:** an RLE8 decoder that writes packed bit
  values. Compiled and compared with the original native routine in
  **266 cases covering 853,086 decoded values**, including 64 KiB memory
  segment boundaries.
- **Analysis tools:** installation inventory, PE metadata and strings,
  a code-section snapshot tool, and reproducible Ghidra exports.
- **Native catalogue:** 12,207 functions catalogued after binding-target, scene-vtable and memory-stream discovery. The full export supersedes
  the earlier overlapping targeted selections, which remain local references.
- **Lua interface mapping:** 112 observed name/implementation/wrapper
  associations from one registration routine.
- **GAR extraction:** 1,121 local files recovered from `dataS.gar` and
  `dataS2.gar`, including all 470 LuaJIT chunks found in these archives.
- **Lua recovery and rebuild:** all 470 recovered Lua sources compile.
  **308** reproduce the original deterministic stripped bytecode exactly; **162** pass
  syntax checks but still need behavioral validation. A local rebuilt
  script package is generated without replacing installed game files.

Function counts include runtime and third-party library code. They are not
a measure of game reconstruction completeness. See [the project status](analysis/STATUS.md)
for evidence, limitations, and the next targets.

Track completed work and remaining milestones in the
[project checklist](PROJECT_CHECKLIST.md).

## Current 3D renders

### Experimental harvest scenarios

Original combine/cutter, original owned fields and density data. These explicit
test layouts relocate equipment; normal Career uses original fleet placements.
The captured runs harvested 140.4 L / 124.8 L. Physics, crop placement and native
services remain partial. All eighteen CTest suites pass, including the gameplay
and native RLE comparisons.

![Bjornholm original crop harvest scenario](docs/screenshots/gameplay-harvest-map01.png)

![Westbridge Hills original crop harvest scenario](docs/screenshots/gameplay-harvest-map02.png)

### Recovered trailer animation

Original Brantner XML and recovered animation/tipping scripts, shown four seconds
into explicit farm-discharge scenarios. Each scenario starts with the original
8,500 L capacity and checks trailer/farm grain conservation. Physics, procedural
grain surfaces and discharge particles remain incomplete.

![Bjornholm recovered trailer animation scenario](docs/screenshots/gameplay-tip-map01.png)

![Westbridge Hills recovered trailer animation scenario](docs/screenshots/gameplay-tip-map02.png)

### Original scene and environment captures

Gameplay scenario captures and their limitations are described in
[the gameplay notes](docs/GAMEPLAY_RUNTIME.md). The earlier scene captures below
remain useful for map/environment comparison.

Captured directly from the reconstructed scene runtime. Buildings, trees,
roads and props use recovered original meshes, placements and diffuse textures.
Terrain uses original height and weight maps. Sky textures and shader injection
bodies are recovered; lighting and shadows use the implemented host services.
Water/materials remain simplified. These are scene-viewer captures, not gameplay.

### Bjornholm

![Bjornholm reconstructed original scene](docs/screenshots/scene-map01.png)

### Westbridge Hills

![Westbridge Hills reconstructed original scene](docs/screenshots/scene-map02.png)

### Night preview using recovered environment curves

![Westbridge Hills at 22:00](docs/screenshots/environment-night-map02.png)

### Local shadow casting

![Original farm objects casting shadows](docs/screenshots/environment-shadow-map02.png)

Earlier terrain-only captures remain available for comparison:
[Bjornholm](docs/screenshots/terrain-map01.png),
[Westbridge Hills](docs/screenshots/terrain-map02.png).

## Repository contents

| Path | Purpose |
|---|---|
| `src/launcher/main.cpp` | Compilable behavioral reconstruction of the launcher |
| `src/core/rle8.cpp` | Compilable RLE8 engine routine |
| `src/core/tire_forces.h` | Compilable reconstructed tire-force calculation, validated separately from gameplay |
| `src/menu/` | Independently compiled GUI host and authored recovered-menu bootstrap |
| `src/engine/` | Native scene host, hierarchical I3D transforms, instanced original meshes and terrain layers |
| `src/shape_bridge/` | Local original-mesh staging bridge using pinned MIT I3DShapesTool source |
| `scripts/inventory.py` | Read-only inventory, local binary copies, PE reports, and SDK example extraction |
| `scripts/runtime_snapshot.py` | Capture only the normally loaded game's code section for analysis |
| `scripts/analyze.ps1` | Ghidra headless runner |
| `scripts/index_native_exports.py` | Verify full local exports, index engine descriptors and publish aggregate counts |
| `scripts/ghidra/` | Function, reference, and pseudocode export scripts |
| `scripts/check_rle.py` | Differential tests against a locally recovered native reference |
| `scripts/map_bindings.py` | Index observed Lua registrations |
| `scripts/gar.py` | Reconstructed GAR 5.3 header/index parser and script transform |
| `scripts/native_gar.py` | Version-pinned original cipher oracle; still depends on a local game code snapshot |
| `scripts/extract_gar.py` | Local extraction, hashes, XML diagnostics, and script normalization |
| `scripts/recover_lua.py` | Strict decompilation, recompilation checks, and bytecode comparisons |
| `scripts/compile_recovered_lua.py` | Build the local recompiled script package |
| `scripts/package-menu.ps1` | Validate and package the directly runnable local frontend |
| `scripts/prepare-engine-assets.ps1` | Copy original terrain inputs and material layers |
| `scripts/setup_shapes_tools.ps1` | Fetch/build the pinned upstream mesh decoder with a .NET stream adapter |
| `scripts/prepare_scene.py` | Stage both original mesh caches and textures with local provenance |
| `scripts/stage_environment.lua` | Execute recovered environment curve initialization and AnimCurve locally |
| `scripts/check_shapes_native.py` | Compare the decoder cipher with the isolated original GE6 routine |
| `scripts/verify-engine-package.ps1` | Verify native rendering and package parity for both maps |
| `src/menu/savegames.cpp` | Read-only save metadata snapshots and explicit mutation rejection |
| `scripts/verify_workspace.py` | Check hashes and provenance relationships in local outputs |
| `tools/bootstrap_ghidra.py` | Download and verify a pinned official Ghidra release |

Original game executables, archives, assets, SDK files, memory snapshots,
raw decompiler exports, and downloaded tools are **not included**. Generate
them locally from your own installation. This repository contains authored
source, analysis tools, documentation, and selected validation metadata.

## Build

Requirements: 64-bit Windows, CMake, and Visual Studio with the C++ desktop
workload and a Windows SDK. Development was validated with MSVC 19.51 and
Windows SDK 10.0.26100. The C++ runtime is linked statically. The engine and
frontend require the pinned LuaJIT static library; prepare it before building.

```powershell
git clone https://github.com/nat649/fs15-reconstruction.git
cd fs15-reconstruction
.\scripts\setup_lua_tools.ps1
.\scripts\build.ps1
.\FS15LauncherRebuilt.exe --self-test
.\FS15LauncherRebuilt.exe --dry-run --game-dir 'D:\SteamLibrary\steamapps\common\Farming Simulator 15'
```

CMake uses its default Visual Studio generator. Select another explicitly:

```powershell
.\scripts\build.ps1 -Generator 'Visual Studio 17 2022'
```

The build produces `FS15LauncherRebuilt.exe`, `FS15Core.dll`,
`FS15EngineRebuilt.exe` and the frontend, and runs the locally available CTest suites.
The launcher has nine behavioral checks. If the local native RLE reference
exists, the build also requires Python 3.11+ and runs the differential tests.
Pass `-PythonExe 'C:\path\to\python.exe'` to select the interpreter.

GitHub's **Repository checks** workflow uses only authored synthetic fixtures
and publication guards, with no game assets or native reference image. Its
result is separate from the eighteen-suite local game/native validation.
See [CONTRIBUTING.md](CONTRIBUTING.md) for asset-free checks and submission rules.

To start the installed game through this launcher, remove `--dry-run`.
Keep Steam available. This uses the **original game executable**. Set the
installation path with `--game-dir` or `FS15_GAME_DIR`; otherwise it defaults
to Steam's usual installation directory on C:.

### Launch the reconstructed frontend

After script/asset recovery below, run:

```powershell
.\scripts\setup_lua_tools.ps1
.\scripts\package-menu.ps1 -GameDir $env:FS15_GAME_DIR -PrepareOnly
.\scripts\setup_shapes_tools.ps1
& 'C:\path\to\python.exe' scripts\prepare_scene.py --game-dir $env:FS15_GAME_DIR
& 'C:\path\to\python.exe' scripts\prepare_mission.py --game-dir $env:FS15_GAME_DIR
.\scripts\build.ps1 -PythonExe 'C:\path\to\python.exe'
.\scripts\package-menu.ps1 -GameDir $env:FS15_GAME_DIR
.\build\FS15GameplayAlpha4\FS15MenuRebuilt.exe
```

The mesh preparation tools require Git and the .NET 10 SDK with the ASP.NET
reference pack; the packaged native runtime does not require .NET.
Preparation copies original files and stages geometry locally before the
build's menu/gameplay tests run. The engine's recovered gameplay bridge and the
frontend require the pinned local LuaJIT static library.

The local folder contains both executables, bootstrap, recovered Lua sources,
assets and LuaJIT copyright notice. A ZIP is generated at
`build/FS15MenuRebuilt-local.zip`. Both stay excluded from GitHub. Keep the
folder together; the executable finds resources beside itself regardless of
the working directory. No original game executable, runtime code snapshot,
archive cipher oracle or Steam process is required **to run this frontend and terrain engine**.

Mouse, wheel and arrow/Enter input use the recovered GUI's hit testing, focus
and callbacks. Career shows twenty slots, existing money/duration/map/date
metadata, and explicit unavailable labels for unsupported metadata. Save
files are read only; Delete is disabled. An empty slot opens the recovered
Difficulty and Map Selection screens. Start opens a new experimental session
with original map/fleet/crop data and partial native gameplay services. It creates
no persistent save.
Credits/Back and Quit also work.
Use WASD/ZQSD or arrows to drive, Space to brake, Tab to change vehicle, E to
attach, B to work, R to fill, I to tip and O for the combine pipe. Mouse drag/wheel
orbit/zoom the camera. P pauses/resumes; losing focus or minimizing also pauses
and clears pending input. F8 opens the earlier free camera. See the
[gameplay notes](docs/GAMEPLAY_RUNTIME.md) for limitations and test scenarios.
F5/F6 step the preview hour; F7 toggles local shadows.

Gameplay uses fixed 60 Hz host updates independently of rendering. Both maps
pass original-bytecode/recovered-source comparisons and timing/pause checks
in 24 native runs. See [reliability scope and evidence](docs/GAMEPLAY_RUNTIME.md#timing-pause-and-original-script-comparisons).

Interactive runs read the local Windows Documents/My Games/FarmingSimulator2015
profile. Override it with `--save-dir PATH`, or use `--empty-saves` for empty
slots. Smoke tests default to empty slots and inject authored metadata only
for deterministic input checks. Preview a screen explicitly:

```powershell
.\build\FS15GameplayAlpha4\FS15MenuRebuilt.exe --empty-saves --screen MapSelectionScreen
```

Other screens report their requested destination. The full `main.lua` startup, complete game worlds,
native physics parity, networking, audio and multiplayer remain unfinished.
See [menu reconstruction notes](docs/MENU_RUNTIME.md).

## Reproduce the local analysis

Requirements: Python 3.11+, JDK 21, and a local FS15 1.4.2.0 installation.
The Python tools use the standard library. Configure your own paths:

```powershell
$env:FS15_GAME_DIR = 'D:\SteamLibrary\steamapps\common\Farming Simulator 15'
$env:JAVA_HOME = 'C:\path\to\jdk-21'

py -3 scripts\inventory.py
py -3 tools\bootstrap_ghidra.py
.\scripts\analyze.ps1 -Target launcher -Limit -1

# Start the installed game normally through Steam and capture its code section.
py -3 scripts\runtime_snapshot.py --launch-game --via-steam
.\scripts\analyze.ps1 -Target game-runtime -Limit 250 -TimeoutSeconds 300

# Export from the existing project without re-running analysis.
.\scripts\analyze.ps1 -Target game-runtime -Existing -ExportName game-focus -Limit 80 -Addresses '140011a30','1400149d0','14001ea90','14002b8c0','140096580','140098630','14028f430','14028f770'
.\scripts\analyze.ps1 -Target game-runtime -Existing -ExportName native-leaf -LeafReference '14028f430'
Copy-Item inputs\reference\rle8-reference.json analysis\rle8-reference.json
Copy-Item inputs\reference\rle8-reference.asm analysis\rle8-reference.asm
py -3 scripts\map_bindings.py
py -3 scripts\verify_workspace.py
```

Pass `--game PATH` to the inventory or snapshot tool to override the game
directory, `--steam PATH` for a different Steam executable, and `-JavaHome PATH`
to the analysis runner to override `JAVA_HOME`.

The bootstrap downloads [Ghidra 11.4.2 from the official release](https://github.com/NationalSecurityAgency/ghidra/releases/tag/Ghidra_11.4.2_build)
and checks the published SHA-256 before extraction:

```text
795a02076af16257bd6f3f4736c4fc152ce9ff1f95df35cd47e2adc086e037a6
```

The scripts do not overwrite installed game binaries. Starting the game
normally can update its usual user profile. The snapshot copies only `.text`
into a local analysis image, not the heap, writable state, or other modules.
That image is an analysis input, **not a rebuilt executable**.

Ghidra `.c` files are pseudocode with inferred types and generated names;
they are not original source files and do not compile directly. Preserve
binary hashes, addresses, and raw exports separately from each reconstructed
implementation. Local inputs and generated reports are excluded by `.gitignore`.

## Recover and rebuild the script layer

After creating the verified native image above, place local copies of
`dataS.gar` and `dataS2.gar` in `inputs/archives/`. Then run:

```powershell
py -3 scripts\extract_gar.py inputs\archives\dataS.gar --decode-luajit --validate-xml
py -3 scripts\extract_gar.py inputs\archives\dataS2.gar --decode-luajit --validate-xml
py -3 scripts\verify_recovered_files.py
.\scripts\setup_lua_tools.ps1
py -3 scripts\recover_lua.py
py -3 scripts\compile_recovered_lua.py
```

The tool setup uses pinned revisions of [LuaJIT](https://github.com/LuaJIT/LuaJIT)
and [LJD](https://github.com/Dr-MTN/luajit-decompiler), retained locally with
their upstream licenses. LuaJIT is built for Windows x64 with `nogc64 static`
to match the observed chunk architecture. See `analysis/lua-toolchain.json`.
The authored adapter `scripts/ljd_driver.py` fixes repeated literal AST nodes
and preserves empty-table allocation before subsequent field assignments,
without modifying the third-party checkout. Authored regression fixtures
check evaluation order and round trip to identical bytecode. The table fix
adds 54 exact matches with no loss of the previous 254.

Outputs remain local:

- `recovered/gar/`: extracted assets and original compiled scripts.
- `recovered/luajit/`: normalized chunks; all 470 observed chunks already
  used the standard version-2 header and required no transformation.
- `recovered/lua/dataS/`: recovered, compilable Lua source.
- `build/recompiled/dataS/`: recompiled chunks with portable source names.
- `build/FS15ScriptsRebuilt.zip`: 470 rebuilt chunks and validation metadata.

**The GAR cipher has not been independently reconstructed.** Extraction
currently calls two routines from the exact hash-pinned original code image
inside the extraction process. Header/index parsing, bounds/path checks, and
the extraction pipeline are reconstructed code. See [GAR notes](docs/GAR.md).

Lua validation loads/compiles chunks without executing their top-level game
code. `exact_match` compares deterministic `string.dump(chunk, "sd")` from the
original and rebuilt chunks. This sorts template-table hash entries while
retaining all instructions, constants, prototype order and upvalue metadata.
`syntax_only` proves compilation, not equivalence.
The package is a rebuilt script layer, not a rebuilt native engine or a
playable standalone game. See [Lua recovery notes](docs/LUA_RECOVERY.md).

## Next work

Investigate the 162 Lua round-trip differences, reconstruct the GAR cipher
without original-code execution, and continue documenting the Lua/native
interfaces, original physics/component joints, complete farming behavior and
persistent sessions. The current native gameplay bridge is documented above.

The RLE routine does not implement the complete GRLE file format, physics,
rendering, or game simulation. Recovered game assets and Lua sources are
excluded from Git; the repository publishes tools and selected metadata.
