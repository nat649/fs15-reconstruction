# Unified engine launcher

`build/FS15Launcher/FS15Reconstruction.exe` is an authored Windows Forms launcher,
built with .NET 10. `Launch FS15.cmd` opens this graphical application. Settings
are stored only in `build/launcher/settings.json`.

Build it after the native launcher and local script/resource recovery:

```powershell
scripts/build-launcher.ps1
```

The application needs the .NET 10 Windows Desktop runtime and Python 3.11+.
It discovers the workspace from its installation location. Keep the published
files together in `build/FS15Launcher`; an explicit `--root PATH` is also supported.
Choose the installed FS15 folder and Python executable in the interface.
The interface has **Farming Simulator 15**, **Game resources** and **Settings & activity** pages. Game resources
detects the local installation and runs the existing asset/recovery preparation
pipeline with progress and cancellation. See the [preparation guide](LAUNCHER.md)
for prerequisites, repair behavior and the native GAR-oracle limitation.

## Modes and component boundaries

| Mode | Renderer and physics | Recovered code | Remaining limits |
|---|---|---|---|
| GE6 + recovered gameplay | Installed original GE6 | After 104 comparisons, three recovered wheel helpers replace the corresponding methods in the original WheelsUtil table | Most game code remains original; full recovered-game replacement is incomplete |
| ReFarm (experimental) | Authored Direct3D host and partial simulation | Existing recovered gameplay scripts, bounded recovered native components and original local resources | Approximate physics, farming subset, no persistent session saves |
| GE6 original baseline | Installed original GE6 | Isolated comparison probe only | Original global gameplay methods stay intact |

This combines launch configuration and recovered logic. It does not merge the
two native renderers or physics solvers inside one process. Native modes choose
Career/map/difficulty in the original GE6 frontend. The home-runtime map and
difficulty selectors are disabled in native modes so selections are not silently
discarded. ReFarm does not require Steam at runtime. The existing `--mode home`
alias and executable filenames remain compatible; `--mode refarm` is preferred.

## Direct and Steam startup

Automatic startup uses the normal Steam path for a Steam installation. Direct
startup invokes the installed game executable without requesting a Steam client
launch. An explicit direct request never silently falls back to Steam.

The local FS15 1.4.2.0 Steam executable exited with **code 53** during the direct
test from this GUI. Normal Steam startup reached the native menu and Westbridge
Hills. This proves that the tested direct path did not work; it does not prove
support for a different non-Steam distribution. The separate **Local GE6
(experimental)** option now uses a modified local runtime copy and an authored
platform adapter. It reached the original menu and Bjornholm without loading a
Steam library; the Steam client was already running and was not closed for this
test. Steam services/licensed DLC are unavailable, and full-session validation
remains open. See [the local runtime scope and instructions](LOCAL_ENGINE_RUNTIME.md).

## Recovered helpers in native gameplay

Hybrid startup stages plain recovered WheelsUtil source, verifies the same
104 cases as the baseline probe, then activates only:

- `getTireType`
- `getTireFriction`
- `getGroundType`

The authored activation adapter keeps the original class table and its tire
registrations, including custom tires. It installs an unload listener that
restores only its own wrappers, preserving later changes by other mods. Since
FS15 loads mod scripts once, the map-load listener reactivates the helpers for
subsequent maps. Repeated-map activation is covered by the authored fixture;
native unload/reload sessions remain an outstanding check.
Rendering, wheel contacts, suspension, rigid bodies and mission lifecycle remain
native GE6 services. The standalone fixture checks activation rejection before
parity, 64 friction samples, custom registration and unload behavior; it is not
proof of a native session.

Actual GE6 validation on Westbridge Hills recorded all three helper-use markers
after activation. The map and tractor HUD were observed. This is bounded helper
integration, not a validation of every farming operation or multiplayer session.
Use **Verify native session** after entering a vehicle to record the native log
evidence. Saved evidence is explicitly distinguished from the current process
state in the interface.

**Check launch plan** stages/checks the selected native configuration without
starting a game process. **Open local logs** opens the selected runtime's logs.
The older command-line native script remains available for automation.

See [native evidence](../analysis/native-engine-validation.json),
[launcher validation](../analysis/unified-launcher-validation.json) and the
[project checklist](../PROJECT_CHECKLIST.md).
