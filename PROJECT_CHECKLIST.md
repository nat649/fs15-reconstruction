# Project progress checklist

Updated: **2026-10-14**. Checked items have supporting local build, analysis
or execution evidence. They do not represent a percentage of the full game.

**Current milestone:** a newly compiled executable displays the recovered
main menu without starting the original game engine. Full gameplay is still
unimplemented. See [menu validation](analysis/menu-validation.json) and
[detailed status](analysis/STATUS.md).

## Installation and analysis

- [x] Identify the local FS15 installation and version 1.4.2.0.
- [x] Inventory installed files and record original executable/archive hashes.
- [x] Capture the normally loaded native code section for local analysis.
- [x] Set up a pinned Ghidra toolchain and reproducible export scripts.
- [x] Catalogue 12,166 native functions and export targeted pseudocode.
- [x] Map 112 observed Lua/native registrations from one routine.
- [ ] Recover signatures and behavior for the wider engine interface.

## Reconstructed native components

- [x] Compile the reconstructed Windows launcher and check its behavior.
- [x] Reconstruct the RLE8-to-bit-vector routine in C++.
- [x] Compare RLE8 with the original routine in 266 cases / 853,086 values.
- [ ] Reconstruct the 16-bit RLE variant and complete GRLE handling.
- [ ] Reconstruct the native GAR cipher independently.

## Archive and script recovery

- [x] Reconstruct GAR 5.3 header/index parsing and extraction checks.
- [x] Recover 1,121 files from dataS.gar and dataS2.gar locally.
- [x] Verify extracted file hashes and selected binary format signatures.
- [x] Recover all 470 observed LuaJIT scripts as compilable Lua source.
- [x] Fix the decompiler's repeated-literal issue with an authored adapter.
- [x] Recompile 470 scripts and generate a local script package.
- [x] Establish exact stripped-bytecode matches for 254 scripts.
- [ ] Fully validate the remaining 216 syntax-only scripts.

## Main-menu milestone

- [x] Compile a separate menu executable with no original-engine dependency.
- [x] Execute 26 recovered GUI/dependency Lua sources in the new host.
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
- [ ] Implement Settings, Career/save selection, Tutorials and Achievements screens.
- [ ] Execute a reconstructed equivalent of the full game startup path.

## Toward a playable game

- [ ] Reconstruct scene graph and complete I3D resource loading.
- [ ] Render a game map and support camera/world input.
- [ ] Reconstruct physics and vehicle simulation.
- [ ] Implement terrain, crops and farming interactions.
- [ ] Reconstruct audio playback and mixing.
- [ ] Implement save/load and validate a complete single-player session.
- [ ] Reconstruct networking and multiplayer behavior.
- [ ] Validate complete engine/game behavior against the original.

## Repository and local delivery

- [x] Publish authored code/tools and English documentation on GitHub.
- [x] Keep original binaries, recovered game code/assets and local packages out of Git.
- [x] Publish this checklist and aggregate validation metadata.
- [ ] Provide a fully reconstructed playable game executable.

Asset extraction currently uses a pinned original cipher oracle. The menu
runtime itself does not use that oracle, an original code snapshot or the
original game executable. Unsupported menu destinations report their name;
they do not start a game session. This checklist will advance as those engine
services are implemented and verified.
