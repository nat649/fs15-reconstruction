# Actual GIANTS Engine 6 runtime path

The target is to run recovered/reconstructed game code on **the actual GIANTS
Engine 6**, retaining native rendering, physics, scene loading and lifecycle.
The authored Direct3D alpha is an experimental viewer/test host, not the final
engine architecture. Launching the original game alone does not make it a
reconstructed game.

## Prepared local entry point

```powershell
& 'C:\path\to\python.exe' scripts/prepare_native_engine.py --game-dir $env:FS15_GAME_DIR
& 'C:\path\to\python.exe' scripts/prepare_native_engine.py --game-dir $env:FS15_GAME_DIR --launch
```

Preparation creates `build/native-engine/profile` and a local script probe under
its `mods/FS15_ReconstructionProbe` directory. The existing reconstructed launcher
starts the installed original x64 executable with `-profile` pointing to that
absolute directory, without a trailing slash. Original executables/assets are
used in place; preparation does not modify the installation or normal saves.
The installed game's normal launch/ownership requirements still apply.

The manifest explicitly starts with `native_boot_verified=false` and
`recovered_script_execution_verified=false`. These are preparation results.
No actual GE6 launch has been validated for this new entry point yet.

## Recovered-source probe

The observed FS15 mod loader accepts `descVersion=21` and loads
`extraSourceFiles` through native `source(filename, modName)`. The generated
single-player probe captures the original WheelsUtil table, loads the recovered
WheelsUtil source into the isolated mod environment, and compares 104 pure
tire-friction/ground-classification cases. Original global gameplay classes
are not replaced by this initial validation probe.

Select the probe when starting a new career in the isolated profile, then
start the mission. Descriptor discovery at the main menu is insufficient:
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
