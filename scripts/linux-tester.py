#!/usr/bin/env python3
"""Local Linux frontend test launcher. Never uploads results or scans saves."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import platform
import subprocess
import sys
import tarfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--smoke', action='store_true', help='Run checks without opening an interactive window')
    parser.add_argument('--root', type=Path, help='Prepared local resource directory')
    args = parser.parse_args()
    package = Path(__file__).resolve().parents[3]
    resources = args.root.resolve() if args.root else package/'resources'
    output = package/'test-results'/datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S-%f')
    output.mkdir(parents=True, exist_ok=False)
    report = {'scope': 'recovered English frontend test', 'native_engine_started': False,
              'playable': False, 'system': platform.system(), 'machine': platform.machine(),
              'kernel': platform.release(), 'libc': platform.libc_ver(),
              'display': {k: os.environ.get(k, '') for k in ('XDG_SESSION_TYPE', 'SDL_VIDEODRIVER')},
              'smoke_exit': None, 'interactive_exit': None, 'status': 'failed'}
    code = 1
    try:
        if platform.system() != 'Linux' or platform.machine().lower() not in ('x86_64', 'amd64'):
            raise RuntimeError('This package requires Linux x86_64.')
        manifest_file = package/'TESTER-MANIFEST.json'
        if not manifest_file.is_file():
            raise RuntimeError('Missing TESTER-MANIFEST.json; extract the complete tester package.')
        manifest = json.loads(manifest_file.read_text(encoding='utf-8'))
        for row in manifest['files']:
            relative = PurePosixPath(row['path'])
            if relative.is_absolute() or '..' in relative.parts:
                raise RuntimeError('Unsafe manifest path')
            path = package.joinpath(*relative.parts)
            if path.is_symlink() or not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != row['sha256']:
                raise RuntimeError('Package verification failed: '+str(relative))
        report['package_verified'] = True
        exe = package/'bin/FS15MenuLinux'
        deps = subprocess.run(['ldd', str(exe)], capture_output=True, text=True, timeout=15)
        missing = [line.split('=>')[0].strip() for line in deps.stdout.splitlines() if 'not found' in line]
        report['missing_libraries'] = missing
        report['loader_errors'] = deps.stderr.strip()
        if deps.returncode or missing:
            raise RuntimeError('Runtime libraries missing or incompatible: '+', '.join(missing)+
                               '. See README-LINUX.txt for dependency packages; nothing was installed automatically.')
        if not (resources/'bootstrap.lua').is_file():
            raise RuntimeError('No prepared resources. Prepare a local package from your game copy; see README-LINUX.txt.')
        base = [str(exe), '--root', str(resources), '--output-dir', str(output), '--empty-saves']
        with (output/'console.log').open('w', encoding='utf-8') as log:
            smoke_env = dict(os.environ, SDL_VIDEODRIVER='dummy')
            smoke = subprocess.run(base+['--smoke', '--test-inputs', '--capture-dir', str(output/'screens'),
                                         '--screenshot', str(output/'MainScreen.png')],
                                   stdout=log, stderr=subprocess.STDOUT, env=smoke_env, timeout=180)
            report['smoke_exit'] = smoke.returncode
            smoke_log = output/'menu-runtime.log'
            if smoke_log.exists():
                smoke_log.rename(output/'smoke-runtime.log')
            if smoke.returncode:
                raise RuntimeError('Frontend self-check failed. Return the generated results archive.')
            if not args.smoke:
                print('Opening the recovered menu. Test Career, difficulty/map selection, Credits/Back and Quit.', flush=True)
                interactive = subprocess.run(base+['--screenshot', str(output/'window-first-frame.png')],
                                             stdout=log, stderr=subprocess.STDOUT)
                report['interactive_exit'] = interactive.returncode
                if interactive.returncode:
                    raise RuntimeError('Interactive menu failed. Return the generated results archive.')
        code = 0
        report['status'] = 'passed'
    except (OSError, RuntimeError, ValueError, KeyError, subprocess.SubprocessError) as error:
        report['error'] = str(error)
        print(str(error), file=sys.stderr)
    finally:
        (output/'report.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
        archive = output.with_suffix('.tar.gz')
        with tarfile.open(archive, 'w:gz') as tar:
            tar.add(output, arcname=output.name)
        print('Results to return: '+str(archive))
        print('This archive contains this run\'s logs, system details and screenshots; no saves or game resource directories.')
    return code


if __name__ == '__main__':
    sys.exit(main())
