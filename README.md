# FS15 Reconstruction

Early-stage reverse engineering and component reconstruction of **Farming
Simulator 15 for Windows**, based on a locally installed copy of version
**1.4.2.0**.

**This is not a complete game rebuild. The engine and the full game cannot
be compiled from this repository yet.**

## Current results

- **Reconstructed C++ launcher:** reads the game's startup XML, chooses the
  x86 or x64 executable, and forwards command-line arguments. It still runs
  the original installed game engine.
- **Reconstructed engine routine:** an RLE8 decoder that writes packed bit
  values. Compiled and compared with the original native routine in
  **266 cases covering 853,086 decoded values**, including 64 KiB memory
  segment boundaries.
- **Analysis tools:** installation inventory, PE metadata and strings,
  a code-section snapshot tool, and reproducible Ghidra exports.
- **Initial analysis:** 12,166 native functions catalogued; 249 pseudocode
  exports in an initial selection and 44 in an overlapping focused selection.
- **Lua interface mapping:** 112 observed name/implementation/wrapper
  associations from one registration routine.

Function counts include runtime and third-party library code. They are not
a measure of game reconstruction completeness. See [the project status](analysis/STATUS.md)
for evidence, limitations, and the next targets.

## Repository contents

| Path | Purpose |
|---|---|
| `src/launcher/main.cpp` | Compilable behavioral reconstruction of the launcher |
| `src/core/rle8.cpp` | Compilable RLE8 engine routine |
| `scripts/inventory.py` | Read-only inventory, local binary copies, PE reports, and SDK example extraction |
| `scripts/runtime_snapshot.py` | Capture only the normally loaded game's code section for analysis |
| `scripts/analyze.ps1` | Ghidra headless runner |
| `scripts/ghidra/` | Function, reference, and pseudocode export scripts |
| `scripts/check_rle.py` | Differential tests against a locally recovered native reference |
| `scripts/map_bindings.py` | Index observed Lua registrations |
| `scripts/verify_workspace.py` | Check hashes and provenance relationships in local outputs |
| `tools/bootstrap_ghidra.py` | Download and verify a pinned official Ghidra release |

Original game executables, archives, assets, SDK files, memory snapshots,
raw decompiler exports, and downloaded tools are **not included**. Generate
them locally from your own installation. This repository contains authored
source, analysis tools, documentation, and selected validation metadata.

## Build

Requirements: 64-bit Windows, CMake, and Visual Studio with the C++ desktop
workload and a Windows SDK. Development was validated with MSVC 19.51 and
Windows SDK 10.0.26100. The C++ runtime is linked statically.

```powershell
.\scripts\build.ps1
.\FS15LauncherRebuilt.exe --self-test
.\FS15LauncherRebuilt.exe --dry-run --game-dir 'D:\SteamLibrary\steamapps\common\Farming Simulator 15'
```

CMake uses its default Visual Studio generator. Select another explicitly:

```powershell
.\scripts\build.ps1 -Generator 'Visual Studio 17 2022'
```

The build produces `FS15LauncherRebuilt.exe` and `FS15Core.dll` and runs CTest.
The launcher has nine behavioral checks. If the local native RLE reference
exists, the build also requires Python 3.11+ and runs the differential tests.
Pass `-PythonExe 'C:\path\to\python.exe'` to select the interpreter.

To start the installed game through this launcher, remove `--dry-run`.
Keep Steam available. This uses the **original game executable**. Set the
installation path with `--game-dir` or `FS15_GAME_DIR`; otherwise it defaults
to Steam's usual installation directory on C:.

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

## Next work

Trace the native Lua `source` implementation and file providers to identify
the GAR reader, recover the version-specific scripts, and document the
interfaces between Lua and native code. Reconstruct isolated modules with
direct comparisons against the original before rebuilding engine startup.

The GAR archives have not been extracted. The RLE routine does not implement
the complete GRLE file format, physics, rendering, or game simulation.
