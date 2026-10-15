# Native GE6 portability status

The future target is **Nintendo Switch homebrew (.nro)**. The native Windows
runtime still executes original x64 machine code. Its successful local launch
does not make that executable portable. The port requires recovered compilable
engine code, recovered game scripts and platform/middleware integration.
ReFarm remains an authored experimental host for resources and recovered logic.

## Current measured boundary

| Evidence | Observed result | What remains open |
|---|---|---|
| Existing native catalogue | 12,196 entries, 11,631 eligible pseudocode exports | Wider discovery and a complete typed compilable engine |
| Recovered portable C++17 core | 30 native entries with validated bounded paths | Remaining 11,601 eligible entries only have pseudocode in this ledger |
| Scene recovery added in this milestone | 36,000 complete math results, 5,999 complete node arenas and 4,799 complete Lua frames match original code bit for bit | Object allocation, ownership, dirty propagation, resource/renderer lifecycle |
| Known static dependency graph | 31,391 distinct catalogue call edges | Computed/virtual calls, dynamic loads and callback targets are not closed |
| 491 distinct observed script-descriptor roots | 2,878 eligible reachable functions, of which 30 have validated components | 2,848 reachable entries without a validated component; 939 functions contain 2,495 computed call sites |
| Native CRT startup root | 1,488 eligible reachable functions in the known static graph | Static initialization/dynamic dispatch prevent treating this as full engine startup coverage |
| Prototype recovery | 11,307 eligible prototypes still contain Ghidra `undefined` types | Accurate argument/return types, structures, globals, ABI and ownership |
| Original PE import boundary | 424 named/ordinal imports across 20 libraries | Platform replacements and statically linked middleware are not covered by this count |
| CPU portability | Windows portable-core checks and an ARM64 Linux/QEMU CI job | ARM64 Linux tests are not libnx, `.nro` packaging or hardware execution |

See the [aggregate dependency evidence](../analysis/native-portability-summary.json),
[component ledger](../analysis/native-recompilation-summary.json),
[scene comparisons](../analysis/scene-core-validation.json) and
[CI workflow](../.github/workflows/repository-checks.yml). The graph's external
address-space targets are recorded separately from missing native entries.
Zero missing native targets in these closures does not imply complete discovery.

## Work required before a complete port

1. Recover scene-object types and vtables, setter/dirty propagation and object
   ownership around the newly validated matrix/cache code. The current arena
   API supplies bounded caller-owned memory, not native scene construction.
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
