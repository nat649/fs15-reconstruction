# Native GE6 portability status

The future target is **Nintendo Switch homebrew (.nro)**. The native Windows
runtime still executes original x64 machine code. Its successful local launch
does not make that executable portable. The port requires recovered compilable
engine code, recovered game scripts and platform/middleware integration.
ReFarm remains an authored experimental host for resources and recovered logic.

## Current measured boundary

| Evidence | Observed result | What remains open |
|---|---|---|
| Existing native catalogue | 12,200 entries, 11,635 eligible pseudocode exports | Wider discovery and a complete typed compilable engine |
| Recovered portable C++17 core | 82 native entries with validated bounded paths | Remaining 11,553 eligible entries only have pseudocode in this ledger |
| Scene recovery added in this milestone | 36,000 complete math results, 5,999 complete node arenas and 4,799 complete Lua frames match original code bit for bit | Object allocation, ownership, dirty propagation, resource/renderer lifecycle |
| Scene state/vtables | 16 explicit slots audited; five state paths match 12,000 node arenas and 7,200 notification traces | Allocation/destruction, remaining derived classes and callback side effects; state component is not yet connected to a complete engine lifecycle |
| Known static dependency graph | 31,393 distinct catalogue call edges | Computed/virtual calls, dynamic loads and callback targets are not closed |
| TRS updates | Eleven entries, 18,000 math results and 7,200 complete scene arenas; integrated TRS/affine composition in ReFarm | Platform libm, logger/observer callback effects and full object lifecycle remain separate boundaries |
| Hierarchy membership | Three entries; 19,200 successive complete arenas/child buffers and notification traces match native link/reparent/unlink behavior | Native allocation/growth, constructors/destructors, registries, resource ownership and arbitrary derived callbacks |
| Object lifecycle | Thirteen supported constructor/name/destructor paths; 250 recursive forests / 2,470 deleting destructions match original code | Authored allocator/registry/dispatch services; shared resources, triggers, derived types and singleton/concurrent behavior |
| Scene attributes | Eight paths; 12,780 complete trees, 12,600 setter / 25,200 getter frames; original getter reads reconstructed storage | Native allocation-retention quirks, individual erasure/cloning and complete resource/loader integration |
| 491 distinct observed script-descriptor roots | 2,878 eligible reachable functions, of which 60 have validated components | 2,818 reachable entries without a validated component; 939 functions contain 2,495 computed call sites |
| Native CRT startup root | 1,488 eligible reachable functions in the known static graph | Static initialization/dynamic dispatch prevent treating this as full engine startup coverage |
| Prototype recovery | 11,311 eligible prototypes still contain Ghidra `undefined` types | Accurate argument/return types, structures, globals, ABI and ownership |
| Original PE import boundary | 424 named/ordinal imports across 20 libraries | Platform replacements and statically linked middleware are not covered by this count |
| CPU portability | Asset-free core tests run on Windows x64, Linux x64 and ARM64 Linux under QEMU | ARM64 Linux tests are not libnx, `.nro` packaging or hardware execution |

See the [aggregate dependency evidence](../analysis/native-portability-summary.json),
[component ledger](../analysis/native-recompilation-summary.json),
[scene comparisons](../analysis/scene-core-validation.json) and
[platform checks](../analysis/portable-core-platform-validation.json). The graph's external
address-space targets are recorded separately from missing native entries.
Zero missing native targets in these closures does not imply complete discovery.

## Work required before a complete port

1. Extend the recovered partial scene layout and base/TRS propagation with
   allocation/destruction, remaining derived vtables, callbacks and ownership.
   The [state component](SCENE_STATE_RECOVERY.md) supplies bounded caller-owned
   memory and notification events, not native scene construction.
   Ordered [link/reparent/unlink](SCENE_HIERARCHY_RECOVERY.md) is recovered for
   preallocated vectors; native allocation and object lifetime remain open.
   [Supported lifetime chains](SCENE_LIFECYCLE_RECOVERY.md) now run through
   authored allocator/registry/dispatch services. Native-layout
   [nonempty attributes](SCENE_ATTRIBUTES_RECOVERY.md) are supported; shared
   resources, triggers, complete scene ownership and native startup remain open.
2. Recover the resource lifecycle, archive decoding without an original-code
   oracle, complete I3D scene loading and original terrain/density services.
3. Recover and link subsystem startup and scripting, following the observed
   dependencies and resolving virtual/callback targets instead of fabricating
   replacement implementations for unknown services.
4. Recover rendering/material behavior and physics world, bodies, shapes,
   joints, suspension and drivetrain, keeping original middleware boundaries
   explicit. ReFarm's Direct3D renderer and temporary planar physics do not
   fulfill these items.
5. Supply the chosen platform's filesystem, threading, timing, input, graphics,
   audio and required networking interfaces. Windows/OpenGL/Direct3D/OpenAL
   imports identify only part of this work; dynamic symbols and statically
   linked services need separate recovery.
6. Configure libnx/devkitPro packaging, then validate hardware startup, menu,
   maps, vehicles, farming and persistent save/reload sessions. This stage
   remains deferred; no complete GE6 ARM64 or Switch executable is available.

The full engine is **not yet decompiled into buildable portable source**.
Pseudocode export completeness and bounded component counts are not completion
percentages. Original executable bytes, raw graph/pseudocode details, recovered
game sources and original resources remain local.

## Reproduce the dependency audit locally

```powershell
scripts/analyze.ps1 -Target game-runtime -Existing -Dependencies -ExportName native-dependencies -JavaHome 'C:\path\to\jdk-21'
python scripts/index_recompilation.py
python scripts/index_portability.py
# Inspect exact instructions when pseudocode loses floating-point grouping:
scripts/analyze.ps1 -Target game-runtime -Existing -Instructions '140280380','14009da60' -ExportName scene-math -JavaHome 'C:\path\to\jdk-21'
```

These commands require the existing pinned local analysis project. The public
repository contains authored tooling and aggregate evidence, not the inputs.

Known [CharacterSet/data/clip ownership](SHARED_SCENE_RECOVERY.md) now covers shared pose references, node rebinding,
update-list removal and nested clip destruction. Other resource classes,
animation evaluation, active triggers and complete loading remain open.
