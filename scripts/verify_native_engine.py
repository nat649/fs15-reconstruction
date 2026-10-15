"""Record bounded evidence from the isolated original-engine log, not a host fixture."""
import argparse
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def inspect_log(text):
    version = re.search(r'^GIANTS Engine Runtime (6\.0\.2) 64bit', text, re.M)
    loaded = text.find('Load mod: FS15_ReconstructionProbe')
    begin = text.find('FS15_NATIVE_PROBE_BEGIN')
    success = text.find('FS15_NATIVE_RECOVERED_LUA_OK comparisons=104')
    native = version is not None and 'OpenGL initialization successful' in text
    probe = native and 0 <= loaded < begin < success
    return dict(native_boot_verified=native, recovered_script_execution_verified=probe,
                engine_version=version.group(1) if version else None,
                probe_comparisons=104 if probe else 0,
                lua_error_lines=[line for line in text.splitlines()
                                 if 'Error: Running LUA method' in line or 'Error: LUA' in line])


def verify(root):
    root = Path(root)
    manifest_path = root / 'build/native-engine/manifest.json'
    manifest = json.loads(manifest_path.read_text())
    profile = Path(manifest['profile']).resolve(strict=True)
    if profile != (root / 'build/native-engine/profile').resolve(strict=True):
        raise ValueError('Only the isolated workspace profile can be verified')
    log = profile / 'log.txt'
    raw = log.read_bytes()
    report = inspect_log(raw.decode('utf-8', errors='replace'))
    report.update(scope='Original GIANTS Engine 6 with an isolated recovered WheelsUtil helper probe; not a complete game rebuild',
                  log_sha256=hashlib.sha256(raw).hexdigest(),
                  original_executable_sha256=manifest['original_executable_sha256'],
                  recovered_wheels_source_sha256=manifest['recovered_wheels_source_sha256'],
                  mission_visual_check='Not inferred from the log; requires separate observation',
                  original_global_gameplay_replaced=False)
    if not report['native_boot_verified'] or not report['recovered_script_execution_verified'] or report['lua_error_lines']:
        raise ValueError('Missing native boot/probe evidence or Lua errors in the isolated log')
    # A launch timestamp prevents a stale log from validating a new launch request.
    if manifest.get('launch_requested_utc'):
        from datetime import datetime
        if log.stat().st_mtime + 2 < datetime.fromisoformat(manifest['launch_requested_utc']).timestamp():
            raise ValueError('Native log predates the current launch request')
    manifest.update({key: report[key] for key in ('native_boot_verified', 'recovered_script_execution_verified')})
    manifest['verified_log_sha256'] = report['log_sha256']
    manifest_path.write_text(json.dumps(manifest, indent=2) + '\n')
    output = root / 'analysis/native-engine-validation.json'
    output.write_text(json.dumps(report, indent=2) + '\n')
    return report


if __name__ == '__main__':
    argparse.ArgumentParser(description=__doc__).parse_args()
    print(json.dumps(verify(ROOT), indent=2))
