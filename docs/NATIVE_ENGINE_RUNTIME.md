# Actual GIANTS Engine 6 runtime path

The target is to run recovered/reconstructed game code on **the actual GIANTS
Engine 6**, retaining native rendering, physics, scene loading and lifecycle.
The authored Direct3D alpha is an experimental viewer/test host, not the final
engine architecture. Launching the original game alone does not make it a
reconstructed game.

## Local native entry point

After local script recovery and launcher compilation, build the graphical
launcher with `scripts/build-launcher.ps1`, then double-click `Launch FS15.cmd`.
Its default mode activates three recovered wheel helpers after parity checks.
See [the unified launcher](UNIFIED_LAUNCHER.md). Command-line baseline startup
and a nonstandard Steam client can still be selected explicitly:

```powershell
scripts/launch-native.ps1 -GameDir $env:FS15_GAME_DIR
scripts/launch-native.ps1 -GameDir $env:FS15_GAME_DIR -SteamExe 'D:\Steam\steam.exe'
# For a non-Steam installation:
scripts/launch-native.ps1 -GameDir $env:FS15_GAME_DIR -Direct
# Prepare only, without launching:
python scripts/prepare_native_engine.py --game-dir $env:FS15_GAME_DIR
```

Preparation creates `build/native-engine/profile` and a local script probe under
its `mods/FS15_ReconstructionProbe` directory. The existing reconstructed launcher
uses the normal Steam app 313160 startup for Steam installations. Steam selects
the installed architecture; the observed session was x64. Direct launch is
available for non-Steam installations. `-profile` points to the isolated
directory, without a trailing slash. Original executables/assets are
used in place; preparation does not modify the installation or normal saves.
The installed game's normal launch/ownership requirements still apply.

The script remembers the game directory in the ignored local manifest and
refuses to launch while an FS15 session is already running. Python 3.11+ and
the compiled `FS15LauncherRebuilt.exe` are required. The root command is the
actual-engine entry point; the older menu/engine executables still run the
separate authored experimental host.

GE6's graphics restart was observed to lose quotes around a profile path with
spaces, producing a different profile. Native launch now creates a checked
directory junction under the temporary directory with a path without spaces.
The junction must resolve exactly to the workspace profile; an existing alias
pointing elsewhere is rejected. Profile data stays in the workspace. This
alias and its launch command were checked locally; a complete graphics restart
round trip through it remains pending. If the temporary directory itself has
spaces, the launcher reports an error instead of using an unsafe profile path.

## Validated native baseline (2026-10-15)

A normal Steam startup produced a fresh isolated log identifying **GIANTS
Engine Runtime 6.0.2 64bit**, physics **5.9.4**, and successful OpenGL
initialization. The original menu, Bjornholm loading completion and the player
HUD inside the world were observed. The native script loader executed the
recovered-source probe and all **104 comparisons passed**. No Lua error was
found in the captured log. See [aggregate evidence](../analysis/native-engine-validation.json).

These observations establish native engine use and recovered-source execution.
They do not establish complete reconstructed gameplay: original global game
classes still provide most of the playable simulation. The later hybrid check
on Westbridge Hills additionally activated `getTireType`, `getTireFriction` and
`getGroundType`, and recorded all three being called by native vehicles.

Preparation starts with `native_boot_verified=false` and
`recovered_script_execution_verified=false`. After mission loading, record
the evidence with:

```powershell
python scripts/verify_native_engine.py
```

The verifier requires the native startup, renderer initialization and ordered
probe markers, rejects captured Lua errors, and rejects a log older than the
current launch request. It updates the private manifest and writes an aggregate
report containing hashes rather than profile paths or original code. Visual
gameplay checks remain separate from this log verifier.

## Recovered-source probe

The observed FS15 mod loader accepts `descVersion=21` and loads
`extraSourceFiles` through native `source(filename, modName)`. The generated
single-player probe captures the original WheelsUtil table, loads the recovered
WheelsUtil source into the isolated mod environment, and compares 104 pure
tire-friction/ground-classification cases. Original global gameplay classes
are not replaced by the baseline probe. Hybrid mode adds an authored activation
adapter after verification, replacing only three helper methods in that table.
Other native rendering/physics/gameplay services stay original. See the
[hybrid integration limits](UNIFIED_LAUNCHER.md#recovered-helpers-in-native-gameplay).

Start a career in the isolated profile, then start the mission. In the observed
single-player flow the local probe loaded automatically without a separate
mod-selection screen. Descriptor discovery at the main menu is insufficient:
`extraSourceFiles` execute during `MPLoadingScreen.initializeLoading`, before
specialization initialization and mission loading. Require both a fresh native
log containing `GIANTS Engine Runtime 6.0.2` and this script execution marker:

```text
FS15_NATIVE_RECOVERED_LUA_OK comparisons=104
```

That establishes only the exercised recovered helpers inside the real engine.
Further game-code replacement and complete session validation remain pending.

The authored fixture `tests/fixtures/native_engine_probe.lua` verifies source
ordering, mod isolation and those 104 comparisons against original bytecode in
the standalone verification VM. It is explicitly **not a native engine boot**.

## Native analysis supporting the entry point

- `0x14000c9a0` parses `-profile`. `0x14002ce80`, `0x14002cf00`,
  `0x14002c610` and `0x14002c880` resolve the supplied full profile directory.
- Recovered `main.lua` supports a profile-local mods directory and optional
  `gameSettings.modsDirectoryOverride`.
- Native `0x140280060` prefers same-basename `.luc` over `.lua`. The probe stages
  plain source with no neighboring `.luc`, letting the original engine compile it.
- The original startup loads `dataS/scripts/main.lua`. `-script`/`-base` occur in
  generic help strings, but parsing/working startup overrides are unverified.
- Original `main.lua` disables `loadfile`, `load` and `string.dump`. The probe
  uses the supported engine source path instead.

Modern-verifier bytecode matches do not establish compatibility of freshly
compiled `.luc` files with the older FS15 VM. Local source loading is the first
integration route. Original native code, recovered scripts, profile contents
and generated mod packages remain excluded from Git.
