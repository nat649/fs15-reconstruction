# Linux frontend tester package

For the separate original-object OpenGL preview, see
[GE6 object rendering](GE6_OBJECT_RENDERING.md). Its new executable is included
in current workflow runtime artifacts, but needs one's own staged scene assets.
The frontend tester described below remains a menu test.

The tester runs the recovered FS15 English frontend through the authored SDL2
renderer/input transport, shared DDS/font code, LuaJIT and Expat XML transport.
This is a **frontend test**, not a playable Linux game or complete GE6 rebuild.
No original executable or Steam process is started. World simulation, audio,
saved simulation loading and native GE6 graphics startup remain unavailable.

## Run a prepared local package

Target: Linux x86_64; the prebuilt executable is compiled on Ubuntu 24.04.
Use Ubuntu 24.04 or a compatible/newer runtime. Other distributions may need a
local build. Python 3 and runtime libraries are required. On Ubuntu 24.04:

```sh
sudo apt-get install python3 libluajit-5.1-2 libexpat1 libsdl2-2.0-0 libsdl2-image-2.0-0
```

Extract the entire local tester archive, open a terminal in its folder and run:

```sh
./start-linux.sh
```

If executable permissions were lost while copying, use `sh start-linux.sh`.
The launcher checks the package hashes and runtime libraries, renders the
frontend in a dummy SDL display, exercises recovered menu callbacks, captures
screenshots, then opens an interactive 1280x720 window. Resources are read
only. It uses empty career slots and never scans a personal save directory.
The startup window title identifies this as an experimental Linux frontend.

Check Career, scroll the twenty slots, open an empty slot, choose a difficulty,
select both base maps, then return using Back. Start on a map displays an
unavailable-world notice and keeps the menu open. Check Credits/Back and Quit.
Other incomplete destinations display a notice. Use mouse, wheel, arrow keys,
Enter and Escape. Report visual/layout errors and missing input behavior.

For a machine without a graphical desktop:

```sh
./start-linux.sh --smoke
```

After exit, return the indicated `test-results/<timestamp>.tar.gz`. It contains
this run's logs, system version, check results and screenshots; no save files
or resource directories. Nothing is sent automatically. Inspect it before
sharing. A successful smoke check alone does not prove a visible window or
usable mouse/keyboard on the tester's desktop.

## Optional recovered OpenGL buffer check

Newly prepared packages also include `bin/FS15GLDriverCheck`. From a Linux
graphical desktop, run:

```sh
./bin/FS15GLDriverCheck > gl-buffer-check.txt 2>&1
```

It opens a hidden SDL/OpenGL context, prints the GL renderer, transfers and
reads back test bytes through recovered GE6 buffer code, then deletes the
buffers. Return the text file with the normal tester results. It uses authored
data and does not draw a map or test the complete GE6 renderer/physics engine.
The normal frontend tester does not invoke this optional check automatically.
It requires desktop OpenGL; `--smoke` with SDL's dummy video driver cannot run
it. See [buffer recovery and headless Mesa commands](GL_BUFFER_RECOVERY.md).

## Prepare resources locally

Original game assets and recovered game scripts are excluded from public
GitHub artifacts. A tester needs a locally prepared resource set from their
game copy. The preparation command requires a successful Windows frontend
trace and verifies recovered source hashes against `analysis/lua-recovery.json`.
It copies only traced menu resources plus the two map previews, not full maps,
vehicles or original executables. The resulting local package is ignored by
Git; do not upload it as a public release or redistribute original assets.

Download the authored-only `fs15-linux-x64-portable-host` artifact from a
successful Repository checks run, then prepare locally:

```sh
python scripts/prepare_linux_tester.py \
  --runtime build/download/fs15-linux-x64-portable-host.tar.gz \
  --trace logs/menu-runtime.log \
  --destination build/FS15LinuxTester
```

The command creates the folder, a `.tar.gz` with executable permissions, a
SHA-256 sidecar and an internal file manifest. It rejects an existing output
folder, untrusted archive paths and source/hash mismatches. The trace must
contain `BOOT_OK`, `INPUT_CHECK_OK` and `SMOKE_OK` from the same successful run.
To regenerate it on Windows, use the current rebuilt frontend:

```powershell
.\build\Release\FS15MenuRebuilt.exe --root . --smoke --test-inputs
```

The source checkout needs recovered Lua and extracted local assets first;
these steps do not extract/decompile a friend's fresh game installation.
No French mode is offered until original French XML tokenizer compatibility
is resolved. SDL source rectangles currently round subpixel texture bounds;
filtering, rotation and clip rasterization are authored approximations, not
validated GE6 rendering parity.

## Recorded validation

[Repository checks run 39152321706](https://github.com/nat649/fs15-reconstruction/actions/runs/39152321706)
passes the thirty-two component/transport tests on Linux x64 and emulated ARM64, the existing
headless-host checks, and ten SDL frontend/tester checks using authored resources.
It also passes the recovered buffer check against actual Mesa OpenGL calls.
The local `FS15LinuxTesterGL.tar.gz` package adds that optional executable;
its archive and all 68 manifest files were verified after preparation.
The latter cover image pixels, UV orientation/flip, clipping, font drawing,
dummy-display window presentation, package verification and result archives.
Separately, the Windows frontend rendered the original English menu and passed
its recovered input checks with 36 manifest-matching game sources. This does
not establish that those original resources have already run on Linux.
See the [current connected-chain evidence](../analysis/physics-post-validation.json)
and [frontend evidence](../analysis/linux-menu-validation.json).
