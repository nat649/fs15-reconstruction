# Reconstructed main-menu milestone

`FS15MenuRebuilt.exe` is a newly compiled Windows GUI host. It executes
recovered Lua source for the actual FS15 GUI classes, profiles and menu
callbacks. It does not launch or load the original game executable. It does
not use original machine-code routines or original compiled Lua chunks.

## Boot path and scope

The authored `src/menu/bootstrap.lua` loads **26 recovered sources**, including
the class utility, input constants, utilities, I18N, Gui, GUI element classes,
FocusManager, MainScreen and CreditsScreen. Recovered `Gui:loadProfiles` and
`Gui:loadGui` build widgets from the original XML; `Gui:showGui("MainScreen")`
invokes the recovered creation/open callbacks. Original widget draw/update,
mouse hit testing and keyboard focus logic then execute on the host.

This is an explicit subset bootstrap. Full `main.lua` does **not** execute.
The adapter supplies menu globals and a keyboard service. Presence and
notifications are offline; DLC changes, networking and audio are unavailable.
No arbitrary missing-global fallback hides runtime errors.

The Steam edition's seven main buttons retain their recovered callbacks.
Career and Multiplayer dispatch to a visible unavailable-screen report.
Credits opens the recovered credits screen, including scrolling text and
Back. Quit calls the host's exit service. Other game screens and all gameplay
still need engine reconstruction. The displayed original version string is
asset/script provenance; it is not a claim of a complete rebuilt game.

## Independently authored native services

- `main.cpp`: LuaJIT source compilation in text-only mode, resource resolution,
  window/message loop, mouse/keyboard forwarding, logs and exit.
- `xml_bridge.cpp`: XmlLite-backed read-only DOM, engine-style zero-based
  property paths, typed reads, handles and document cleanup. DTDs are rejected.
- `render.cpp`: GDI+ bitmap output, independently decoded DDS top mipmaps,
  rectangular UV crop/flip, color/alpha and text. The local game's actual
  bitmap font metrics/atlas are parsed; its signed-distance coverage is
  rasterized by authored code rather than the original shader. Large opaque
  static overlay scaling is cached. General nonrectangular UVs fail explicitly.

The host targets a fixed **1280 x 720** client area and English localization.
Original menu profile `.png` requests resolve to recovered `.dds` equivalents.
Required missing/corrupt resources cause startup errors instead of placeholders.

The runtime uses LuaJIT's pinned static library and normal Windows libraries.
No Steam DLL, original engine DLL, native reference blob or GAR is bundled.
Initial asset recovery still depends on the separately documented original
GAR cipher oracle; that dependency is absent from menu runtime execution.

## Local packaging

`scripts/package-menu.ps1` runs the recovered-menu smoke/input checks, reads
the source and asset trace, verifies all used recovered-source hashes against
the recovery manifest, and copies just those dependencies into
`build/FS15Menu`. Its manifest records SHA-256 and size for each packaged file.
The LuaJIT copyright notice accompanies the statically linked executable.

The package executes again from a different working directory without a
workspace override. Its rendered PNG must match the workspace rendering
byte-for-byte. A local ZIP includes only manifest-listed files and the
manifest, with directory structure preserved and no logs.

Recovered Lua sources, original game assets/fonts and local packages remain
excluded from Git. Only the authored host/adapter/tools and aggregate
validation metadata are published.

## Validation evidence and limits

The native smoke test renders **120 frames** and checks seven visible,
localized buttons with recovered callbacks. It exercises actual Lua mouse
Career dispatch, keyboard Down/Enter Multiplayer dispatch, Credits update
and Back, then mouse Quit. The host verifies the exit request. Missing asset
count must remain zero. A native window was launched and its main menu was
visually observed through Computer Use.

Independent local codec fixtures also exercised DXT1 color/transparency,
DXT3/DXT5 alpha, uncompressed masks and truncated-input rejection. XML
fixtures exercised property indexes, text/entities/Unicode, nil absence,
scalar parsing, DTD rejection and cleanup. Actual font metrics, UTF-8 glyphs,
wrapping and raster output were checked.

Key recovered GUI sources previously classified as `syntax_only` now have
evidence for these exercised runtime paths. This does not establish complete
bytecode equivalence, every GUI behavior, original pixel/shader parity or a
rebuilt game world. Native GUI startup replaces the original engine startup.

Aggregate metadata: [menu validation](../analysis/menu-validation.json).
