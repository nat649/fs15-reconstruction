# Contributing

Start with the [project checklist](PROJECT_CHECKLIST.md),
[reconstruction strategy](docs/RECONSTRUCTION_STRATEGY.md) and
[native recovery scope](docs/NATIVE_RECOVERY.md).

## What belongs in a contribution

- Authored reconstruction code, analysis tools and reproducible synthetic tests.
- Aggregate evidence: version/hash, function address, comparison count, result
  and the limits of what was exercised.
- Documentation that separates recovered scripts/resources, original native
  components, reconstructed behavior, external decoders and authored adapters.
- Rendered screenshots under `docs/screenshots/`, with the map, test scenario
  and temporary approximations identified.

Keep original map geometry, transforms, scale and materials as the reference.
Use recovered behavior and validated native interfaces when extending gameplay.
Do not describe pseudocode coverage or a passing fixture as full engine parity.

## Files that stay local

Do not attach or commit game executables, archives, code snapshots, raw native
exports, recovered game Lua, original models/textures/configurations, mesh
caches, user saves or playable packages containing those files. `inputs/`,
`recovered/`, `build/` and `logs/` are local-only. Downloaded dependency
checkouts also stay local. Share the tool invocation and aggregate results.

Public issue logs should contain only the relevant error and generic resource
names. Remove credentials and personal workstation/profile paths. User saves
and full memory dumps are not needed for a public bug report.

## Checks

Checks that use only repository code and synthetic fixtures:

```powershell
py -3 scripts/check_public_repo.py
py -3 -m unittest discover -s tests -p "test_*.py"
```

Use a full Git checkout for the publication check: it scans tracked files and
all reachable history, not just the latest commit. It checks known asset paths,
binary formats and common credential patterns; review provenance separately.

On Windows, install the documented LuaJIT toolchain before building:

```powershell
.\scripts\setup_lua_tools.ps1
.\scripts\build.ps1 -PythonExe 'C:\path\to\python.exe'
```

The complete local validation suite additionally needs prepared original
resources and pinned native references. Record which suites actually ran;
asset-free CI does not validate game recovery, original-engine parity or a
playable session. Do not upload local packages or inputs as CI artifacts.

## Pull requests

Explain the concrete behavior change, provenance, validation and remaining
limits. Keep GitHub documentation and checklist entries in English. Use small
changes that can be reviewed against their evidence. Preserve dependency
notices; do not add a blanket license over original or third-party material.
The current licensing status is documented in [NOTICE.md](NOTICE.md).
