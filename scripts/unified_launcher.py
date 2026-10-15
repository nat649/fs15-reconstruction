"""Shared launcher backend: ReFarm or actual GE6 with recovered helpers.

These are separate renderer/physics processes, not an in-process engine merger.
Direct mode uses only normal installed-executable startup; no ownership bypass.
"""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import time
from prepare_native_engine import ROOT, prepare, configure_launch
from launcher_assets import resource_status


def home_plan(root, map_id, difficulty):
    root = Path(root).resolve()
    if map_id not in ('Map01', 'Map02') or difficulty not in (1, 2, 3):
        raise ValueError('Invalid home-runtime map or difficulty')
    executable = root / 'FS15EngineRebuilt.exe'
    if not executable.is_file():
        executable = root / 'build/Release/FS15EngineRebuilt.exe'
    if not executable.is_file():
        raise ValueError('Build ReFarm before launching it')
    status=resource_status(root)
    if not status['home_launch_ready']:
        details=', '.join(status['missing'][:5]) or 'unfinished or interrupted asset preparation'
        raise ValueError('Open Resources > Prepare / repair assets in the launcher: '+details)
    return dict(mode='home', command=[str(executable), '--root', str(root), '--map', map_id,
                                     '--play', '--difficulty', str(difficulty)],
                scope='Authored Direct3D runtime with recovered Lua; partial physics and gameplay')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mode', choices=('refarm', 'home', 'ge6', 'hybrid'), required=True)
    parser.add_argument('--transport', choices=('auto', 'direct', 'steam'), default='auto')
    parser.add_argument('--game-dir', type=Path)
    parser.add_argument('--steam', type=Path)
    parser.add_argument('--map', choices=('Map01', 'Map02'), default='Map01')
    parser.add_argument('--difficulty', type=int, choices=(1, 2, 3), default=1)
    parser.add_argument('--dry-run', action='store_true')
    args = parser.parse_args()
    try:
        if args.mode in ('refarm', 'home'):
            plan = home_plan(ROOT, args.map, args.difficulty)
            print('Runtime: ReFarm (experimental). Map: ' + args.map + '. Difficulty: ' + str(args.difficulty), flush=True)
            if args.dry_run:
                print('Command: ' + subprocess.list2cmdline(plan['command']), flush=True)
            if not args.dry_run:
                child = subprocess.Popen(plan['command'], cwd=ROOT)
                try:
                    result = child.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    print('HOME_PROCESS_RUNNING pid=' + str(child.pid), flush=True)
                else:
                    raise ValueError('ReFarm exited during startup: ' + str(result))
            return
        if not args.game_dir:
            raise ValueError('Select your installed Farming Simulator 15 directory')
        if sys.platform == 'win32' and not args.dry_run:
            from runtime_snapshot import processes
            if list(processes()):
                raise ValueError('FS15 is already running. Close that session before starting another native session.')
        manifest = prepare(ROOT, args.game_dir, activate_recovered=args.mode == 'hybrid')
        steam = args.steam
        if args.transport == 'steam' and steam is None:
            import os
            steam = Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / 'Steam/steam.exe'
        configure_launch(manifest, steam, args.transport == 'direct', args.dry_run)
        (ROOT / 'build/native-engine/manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
        print('Runtime: actual GIANTS Engine 6' + (' + recovered wheel helpers' if args.mode == 'hybrid' else ' + source comparison probe'), flush=True)
        print('Profile: isolated workspace profile. Startup: ' + args.transport, flush=True)
        result = subprocess.run(manifest['command'], cwd=ROOT)
        if result.returncode:
            raise ValueError('Native startup exited with code ' + str(result.returncode) +
                             '. Direct mode cannot remove a Steam installation requirement; try normal Steam startup or a supported non-Steam copy.')
        if not args.dry_run:
            from datetime import datetime
            launch_time = datetime.fromisoformat(manifest['launch_requested_utc']).timestamp()
            log = Path(manifest['profile']) / 'log.txt'
            for _ in range(60):
                if log.exists() and log.stat().st_mtime >= launch_time:
                    text = log.read_text(encoding='utf-8', errors='replace')
                    if 'GIANTS Engine Runtime 6.0.2' in text and 'OpenGL initialization successful' in text:
                        print('NATIVE_BOOT_LOG_OK. Start Career to load recovered scripts.', flush=True)
                        return
                time.sleep(0.5)
            raise ValueError('Launch requested, but no fresh native renderer log was observed within 30 seconds. Check the game window and isolated log.')
    except (OSError, ValueError, KeyError) as error:
        print('LAUNCH_ERROR: ' + str(error), file=sys.stderr, flush=True)
        sys.exit(1)


if __name__ == '__main__':
    main()
