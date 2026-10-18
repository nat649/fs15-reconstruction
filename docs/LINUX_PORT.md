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

Source: [host](https://github.com/nat649/fs15-reconstruction/blob/master/src/portable/main.cpp). The recovered algorithms stay in
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

## Script/XML and localization stage

`FS15ScriptHost` is a second headless executable. It runs a text-only subset
bootstrap using caller-supplied `class.lua`, `Utils.lua`, `I18N.lua` and local
localization XML. The original menu window and full `main.lua` do not execute.
The shared read-only XML/property and save-metadata adapters now build on Linux.
Windows keeps XmlLite; Linux uses **Expat**, a third-party decoder. This is
platform transport, not recovery of the native GE6 XML tokenizer.

Install LuaJIT development headers, Expat development headers and pkg-config
through your distribution. On Ubuntu these packages are `libluajit-5.1-dev`,
`libexpat1-dev` and `pkg-config`.

The prebuilt script executable needs the corresponding runtime packages
(`libluajit-5.1-2` and `libexpat1` on the CI Ubuntu distribution); they are not
bundled into the archive. The original CPU-only host keeps its earlier
standard C/C++ runtime dependencies. Add the script option when building:

```sh
cmake -S . -B build/linux-host \
  -DFS15_PORTABLE_CORE_ONLY=ON -DFS15_BUILD_PORTABLE_HOST=ON \
  -DFS15_BUILD_PORTABLE_SCRIPTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build/linux-host --parallel
./build/linux-host/src/portable/FS15ScriptHost --i18n-probe \
  --root "/path/to/your/local/reconstruction/workspace" --language en
```

The root supplies `recovered/lua/dataS/scripts/...` and
`recovered/gar/dataS/...`, or the equivalent `lua/...` and `assets/...`
directories in a prepared local package. Required missing files fail explicitly;
bytecode is rejected by the runtime. Sources and assets are never modified.
The JSON report labels input as caller-supplied text; it does not certify its
provenance simply because it has the expected filename.

Local Windows parity checks compare all 1,425 English texts, 165 descriptions,
seven menu labels and three conversion factors with the original compiled Lua
chunks. The reference-bytecode entry is test-only and is never installed.
Recovered `I18N.lua` is classified `syntax_only` by the earlier recovery manifest;
this comparison establishes these exercised outputs, not universal equivalence.

Linux CI uses independently authored Lua/XML fixtures, never uploaded game
assets. Its installed-host checks are separate from that local original-code
comparison. XML tests cover repeated zero-based paths, Unicode/entities/CDATA,
typed values, DTD rejection, size/depth limits and VM document cleanup. The
same tests exercise existing read-only save snapshots; mutations still reject.

The local French `descriptions_fr.xml` is rejected by strict XML parsing at
line 217 (adjacent attributes without whitespace). It remains unchanged. No
French runtime/parity success is claimed; native-tokenizer compatibility needs
separate recovery. The current validated original-resource language is English.

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

## Verified first Linux build

The [first Linux host run](https://github.com/nat649/fs15-reconstruction/actions/runs/39114046074)
passes all six installed-host integration checks and all 23 portable core tests.
The downloaded artifact contains an ELF64 x86-64 executable whose hash matches
the tested binary. The Windows host passes the same six checks. See
[aggregate evidence](https://github.com/nat649/fs15-reconstruction/blob/master/analysis/linux-host-validation.json).

The [verified Lua/XML Linux run](https://github.com/nat649/fs15-reconstruction/actions/runs/39119592405)
passes the shared XML/save suite, six installed-script-host checks, the existing
six CPU-host checks and all 23 core tests. Both ELF executables are in its
authored-only artifact. [Evidence and input hashes](https://github.com/nat649/fs15-reconstruction/blob/master/analysis/linux-script-host-validation.json)
keep Linux fixture execution separate from local Windows original-source parity.
