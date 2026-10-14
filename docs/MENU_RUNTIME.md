# Reconstructed frontend milestone

`FS15MenuRebuilt.exe` is a newly compiled Windows GUI host. It executes
recovered Lua source for the actual FS15 GUI classes, profiles and menu
callbacks. It does not launch or load the original game executable. It does
not use original machine-code routines or original compiled Lua chunks.

## Boot path and scope

The bootstrap and authored `career.lua`/`newgame.lua` adapters load **36 recovered sources**, including
the class utility, input constants, utilities, I18N, Gui, GUI element classes,
FocusManager, MainScreen, CreditsScreen, CareerScreen, mission metadata classes,
MapsUtil, DifficultyScreen and MapSelectionScreen. Recovered `Gui:loadProfiles` and
`Gui:loadGui` build widgets from the original XML; `Gui:showGui("MainScreen")`
invokes the recovered creation/open callbacks. Original widget draw/update,
mouse hit testing and keyboard focus logic then execute on the host.

This is an explicit subset bootstrap. Full `main.lua` does **not** execute.
The adapter supplies menu globals and a keyboard service. Presence and
notifications are offline; DLC changes, networking and audio are unavailable.
No arbitrary missing-global fallback hides runtime errors.

The Steam edition's seven main buttons retain their recovered callbacks.
Career opens twenty selectable slots. Empty slots reach recovered difficulty
and base-map selection; three difficulty choices and both original map previews
execute recovered callbacks. Selected slot/map/difficulty reach an explicit
world-loading boundary. For empty slots, Start now creates the independently
native scene process with original terrain, buildings and trees for the chosen map. Existing saves are displayed
read only and Start reports the missing saved-simulation loader. Multiplayer remains unavailable.
Credits opens the recovered credits screen, including scrolling text and
Back. Quit calls the host's exit service. Other game screens and all gameplay
still need engine reconstruction. The displayed original version string is
asset/script provenance; it is not a claim of a complete rebuilt game.

## Independently authored native services

- `main.cpp`: LuaJIT source compilation in text-only mode, resource resolution,
  window/message loop, mouse/keyboard forwarding, logs and exit.
- `xml_bridge.cpp`: XmlLite-backed read-only DOM, engine-style zero-based
  property paths, typed reads, file/memory input, handles and document cleanup.
  DTDs are rejected and XML memory input is capped at 8 MiB.
- `savegames.cpp`: snapshots only `savegame1..20/careerSavegame.xml` from a
  selected profile, protects callbacks, and rejects write/delete operations.
  Error identifiers are shared with Lua by name; their numeric correspondence
  with the original engine is unverified.
- `render.cpp`: GDI+ bitmap output, independently decoded DDS top mipmaps,
  rectangular UV crop/flip, color/alpha and text. The local game's actual
  bitmap font metrics/atlas are parsed; its signed-distance coverage is
  rasterized by authored code rather than the original shader. Large opaque
  static overlay scaling is cached. General nonrectangular UVs fail explicitly.
  UTF-8 length/substrings and prefix-width measurements support original widget
  text truncation, including normal/bold font advances.
  Already rasterized SDF glyphs avoid a second high-quality filtering pass,
  which otherwise produces rectangular artifacts around dark text.

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
`build/FS15SceneAlpha`. Its manifest records SHA-256 and size for each packaged file.
The LuaJIT copyright notice accompanies the statically linked executable.
The two authored adapters accompany the bootstrap. Preparation also copies
the installed base-map preview DDS files and terrain inputs into the local asset tree.
The package includes `FS15EngineRebuilt.exe`, local original-mesh caches,
diffuse textures and weight maps traced by the native renderer. Scene verification runs
for both maps before ZIP creation. See [engine notes](ENGINE_BOOTSTRAP.md). Pass
`-Destination build/AnotherFrontend` to stage a separate build while an earlier
executable is running.

The package executes again from a different working directory without a
workspace override. Its rendered PNG must match the workspace rendering
byte-for-byte, including Career, Difficulty, Map Selection and Credits captures.
A local ZIP includes only manifest-listed files and the
manifest, with directory structure preserved and no logs.

Recovered Lua sources, original game assets/fonts and local packages remain
excluded from Git. Only the authored host/adapter/tools and aggregate
validation metadata are published.

## Validation evidence and limits

The native smoke test renders **120 main-menu frames** plus input-path screen
captures and checks seven visible,
localized buttons with recovered callbacks. It exercises actual Lua mouse
Career/save selection, wheel scrolling through all twenty slots, all three
difficulties, both base-map previews, keyboard Down/Enter Multiplayer dispatch, Credits update
and Back, then mouse Quit. The host verifies the exit request. Missing asset
count must remain zero. A native window was launched and its main menu was
visually observed through Computer Use.
The updated frontend was also operated through the real Windows mouse/wheel:
Career opened, scrolling exposed an empty slot, Start reached Difficulty,
and choosing Normal displayed the two base-map items. The frontend was left
open at map selection. This observation is separate from the automated Lua
input checks and bitmap captures.

Independent local codec fixtures also exercised DXT1 color/transparency,
DXT3/DXT5 alpha, uncompressed masks and truncated-input rejection. XML
fixtures exercised property indexes, text/entities/Unicode, nil absence,
scalar parsing, DTD rejection and cleanup. Actual font metrics, UTF-8 glyphs,
wrapping and raster output were checked.

`menu_metadata_read_only` uses authored native fixtures for empty/populated
slots, malformed/wrong-root/oversized XML, external DTD rejection, callback
lookup failures, refresh, handle cleanup and every exposed mutation guard.
Lua input fixtures cover unsupported maps, invalid difficulty, NaN/infinite
money, negative duration, supported negative money, and scan failure/recovery.
Occupied unreadable slots remain unavailable rather than becoming new games.

A local profile run exercised actual existing metadata and confirmed its
SHA-256 hashes unchanged before/after. User metadata and that screenshot stay
local. Interactive runs discover Windows Documents/My Games/FarmingSimulator2015;
`--save-dir PATH` selects another read-only profile and `--empty-saves` forces
empty slots. Smoke tests default to empty slots and temporarily inject authored
metadata through a Lua getter; the original provider is restored afterward.

Key recovered GUI sources previously classified as `syntax_only` now have
evidence for these exercised runtime paths. This does not establish complete
bytecode equivalence, every GUI behavior, original pixel/shader parity or a
rebuilt game world. Native GUI startup replaces the original engine startup.

New-game smoke checks validate engine launch plans without spawning windows.
The separate native engine tests perform GPU rendering. Interactive Start
launches the terrain process; its free camera is independent of mission simulation.

Aggregate metadata: [menu validation](../analysis/menu-validation.json).
