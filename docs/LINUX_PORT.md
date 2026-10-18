# Linux x64 intermediate target

Linux x64 is the next platform target while the native GE6 recovery continues.
The later Switch homebrew target remains deferred. This stage keeps recovered
native behavior and original-resource compatibility as its direction; it does
not replace GE6 with a newly authored game engine.

## Available executable

`FS15PortableHost` is an authored **headless portability host**, linked to the
recovered C++17 core. Its `--probe` path runs recovered Camera construction,
world-cache refresh, view/projection/shader-block preparation, spatial grid
allocation/registration, Camera-volume visibility and Camera destruction.
Allocator/registry/world/buffer transport is authored. The buffer is CPU memory.

The process checks balanced map/unmap, one visible and one outside object,
and zero retained allocations/registry entries after cleanup. It emits JSON
and exits nonzero on failure. This is a runnable Linux executable, but it does
not initialize the complete original engine, open a window, draw the menu,
load a playable map or implement GPU/physics/audio backends.

Source: [host](../src/portable/main.cpp). The recovered algorithms stay in
`src/core`; no additional native entries are claimed for this authored host.

## Build and install on Linux x64

Use a C++17 compiler and CMake 3.20 or newer:

```sh
cmake -S . -B build/linux-host \
  -DFS15_PORTABLE_CORE_ONLY=ON \
  -DFS15_BUILD_PORTABLE_HOST=ON \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build/linux-host --parallel
ctest --test-dir build/linux-host --output-on-failure
cmake --install build/linux-host --prefix "$PWD/build/linux-install"
./build/linux-install/bin/FS15PortableHost --probe
```

The host uses no Windows APIs, Direct3D, original executables, game archives,
LuaJIT or GPU driver. It also builds on Windows for local verification. The
existing Windows GUI launcher and ReFarm are separate targets.

GitHub Actions builds and runs the installed Linux x64 executable, tests its
command line from another working directory and publishes an authored-only
`fs15-linux-x64-portable-host` workflow artifact. Download it from a successful
**Repository checks** run and extract the `.tar.gz` inside it:

```sh
tar -xzf fs15-linux-x64-portable-host.tar.gz
./bin/FS15PortableHost --probe --report probe.json
```

This workflow artifact is a portability tool, not a playable game release.
No original code image, recovered Lua or game resource is included.
The archive is built on the GitHub Ubuntu runner and uses its standard C/C++
runtime libraries. It includes `share/fs15-reconstruction/runtime-dependencies.txt`.
For a distribution with older runtime libraries, build locally rather than
assuming the CI binary is compatible.

## Optional local resource inspection

```sh
./bin/FS15PortableHost --probe \
  --game-dir "/path/to/your/Farming Simulator 15" \
  --report probe.json
```

The supplied directory must exist. The host reports presence and byte sizes
for `dataS.gar`, `dataS2.gar` and the two map I3D files. Missing files remain
explicit in the report. This is filesystem discovery only: file presence does
not prove a supported version, complete installation or successful scene load.
The host never copies or modifies those resources, and rejects a report path
inside that game directory. Original-resource version/hash verification and
full archive/scene loading remain separate recovery work.

## Remaining Linux milestones

- Recover and link complete native subsystem startup and the script bootstrap.
- Implement platform transport for window/input, filesystem, threading/timing,
  GPU resources and audio while recovering the original engine consumers.
- Recover complete scene/resource loading and native renderer/physics lifetime.
- Run the recovered menu, then original maps, vehicles and farming sessions.
- Validate persistent save/reload and package a playable Linux build.

The existing 23 asset-free core tests cover bounded recovered components.
Linux host integration checks exercise the installed executable and filesystem
contract separately. Neither establishes complete GE6 gameplay or Switch
hardware compatibility.
