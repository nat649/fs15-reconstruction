# Project and dependency notices

This is an independent, unofficial research and reconstruction project.
Farming Simulator and GIANTS Engine names and original game content identify
the reference product; they do not imply endorsement by GIANTS Software.

## Repository scope

The public repository contains authored tools/host code, reconstructed
components, documentation, aggregate validation and selected rendered captures.
Original executables, archives, game scripts/resources, memory/code snapshots,
raw native pseudocode and local playable packages are not distributed here.
Screenshots illustrate local results using original reference resources and
are not a license to reuse the game's assets.

No project-wide license has been selected for the authored repository code.
Public visibility does not place the code, original game content or dependency
code under MIT or another blanket license. Existing third-party notices apply
to their respective material and are preserved separately.

## Dependencies

| Dependency | Use | License / notice location |
|---|---|---|
| [LuaJIT](https://github.com/LuaJIT/LuaJIT) | Locally built static Lua runtime | Upstream `COPYRIGHT` (MIT); copied into local runtime packages |
| [LJD fork](https://github.com/Dr-MTN/luajit-decompiler) | Local script recovery tool, invoked by the project adapter | Upstream `LICENSE` (GPL-3.0) and `LICENSE-upstream` (MIT), retained in its local checkout |
| [I3DShapesTool](https://github.com/Donkie/I3DShapesTool) | Local mesh decoder build with an authored stream adapter | MIT; notice retained in [licenses/I3DShapesTool-LICENSE.txt](licenses/I3DShapesTool-LICENSE.txt) |
| [Ghidra](https://github.com/NationalSecurityAgency/ghidra) | Local native analysis/export tool | Upstream Apache-2.0 license and bundled third-party notices |

The setup scripts pin dependency revisions; see
[analysis/lua-toolchain.json](analysis/lua-toolchain.json) and
[scripts/setup_shapes_tools.ps1](scripts/setup_shapes_tools.ps1).
Downloaded dependency checkouts and builds are excluded from this repository.
The repository's `src/shape_bridge/` directory contains the authored adapter;
upstream decoder sources are resolved from the separate local checkout.
