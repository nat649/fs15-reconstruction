#!/usr/bin/env python3
"""Private original-GE6 Linux test launcher. Nothing is uploaded automatically."""
import argparse
import ctypes
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import platform
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parent


MIN_GLIBC = (2, 31)


def distro_info(path=Path('/etc/os-release')):
    result = {}
    try:
        for line in path.read_text(encoding='utf-8').splitlines():
            if '=' in line and not line.startswith('#'):
                name, value = line.split('=', 1)
                result[name] = value.strip().strip('"').strip("'")
    except OSError:
        pass
    return result


def install_hint(distro):
    families = [distro.get('ID', '')] + distro.get('ID_LIKE', '').split()
    if 'arch' in families:
        return 'sudo pacman -Syu --needed python libx11 libxrandr libglvnd xorg-xrandr alsa-lib libpulse'
    if any(name in families for name in ('debian', 'ubuntu')):
        return 'sudo apt-get update && sudo apt-get install python3 libx11-6 libxrandr2 libgl1 x11-xserver-utils'
    return 'Install Python 3, X11/RandR libraries, desktop OpenGL/GLX and the xrandr utility using your package manager.'


def check_platform(system, machine, libc, version):
    if system != 'Linux' or machine.lower() not in ('x86_64', 'amd64'):
        raise RuntimeError('This package requires Linux x86_64; 32-bit and ARM are unsupported.')
    if libc != 'glibc':
        raise RuntimeError('This build needs glibc 2.31+. musl/Alpine is not supported by this ELF.')
    try:
        parts = tuple(int(value) for value in version.split('.'))
    except ValueError:
        raise RuntimeError('Cannot determine the host glibc version.')
    if parts < MIN_GLIBC:
        raise RuntimeError('glibc {} is too old; this build requires 2.31 or newer.'.format(version))


def actual_glibc():
    try:
        query = ctypes.CDLL(None).gnu_get_libc_version
        query.restype = ctypes.c_char_p
        return 'glibc', query().decode('ascii')
    except (AttributeError, OSError):
        return platform.libc_ver()


def display_rates(text):
    rates = [float(value) for value in re.findall(r'(\d+(?:\.\d+)?)\*', text)]
    if not rates or any(rate <= 0 for rate in rates):
        raise RuntimeError('GE6 needs an actual nonzero display refresh rate. Try an Xorg desktop session.')
    return rates


def sha(path):
    value = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            value.update(block)
    return value.hexdigest()


def verify(all_files=False):
    manifest = json.loads((ROOT / 'PACKAGE-MANIFEST.json').read_text(encoding='utf-8'))
    selected = manifest['files'] if all_files else {
        name: manifest['files'][name] for name in (
            'bin/FS15GE6LinuxBoot', 'resources/x64/FarmingSimulator2015Game.exe',
            'resources/dataS.gar', 'resources/dataS2.gar')}
    for index, (name, row) in enumerate(selected.items(), 1):
        relative = PurePosixPath(name)
        if relative.is_absolute() or '..' in relative.parts:
            raise RuntimeError('Unsafe package manifest path.')
        path = ROOT / name
        if not path.resolve().is_relative_to(ROOT) or not path.is_file():
            raise RuntimeError('Missing package file: ' + name)
        if path.stat().st_size != row['size'] or sha(path) != row['sha256']:
            raise RuntimeError('Package file failed verification: ' + name)
        if all_files and index % 500 == 0:
            print('Verified {}/{} files'.format(index, len(selected)), flush=True)
    print('Package verification passed ({} files).'.format(len(selected)), flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--verify', action='store_true', help='Verify every packaged file without starting GE6')
    parser.add_argument('--silent', action='store_true', help='Use the explicit silent audio backend')
    parser.add_argument('--seconds', type=int, help='Stop this diagnostic after a bounded number of seconds')
    args = parser.parse_args()
    if args.seconds is not None and not 1 <= args.seconds <= 7200:
        parser.error('--seconds must be between 1 and 7200')
    verify(args.verify)
    if args.verify:
        return 0
    distro = distro_info()
    libc, libc_version = actual_glibc()
    check_platform(platform.system(), platform.machine(), libc, libc_version)
    hint = install_hint(distro)
    if not os.environ.get('DISPLAY'):
        raise RuntimeError('No X11 display. Use a graphical Xorg session or working XWayland.')
    binary = ROOT / 'bin/FS15GE6LinuxBoot'
    binary.chmod(0o755)
    if shutil.which('ldd'):
        library_command = ['ldd', str(binary)]
    else:
        loader = Path('/lib64/ld-linux-x86-64.so.2')
        if not loader.is_file():
            raise RuntimeError('Missing glibc dynamic loader. This package needs a glibc/FHS desktop.')
        library_command = [str(loader), '--list', str(binary)]
    libraries = subprocess.run(library_command, capture_output=True, text=True)
    if libraries.returncode or 'not found' in libraries.stdout + libraries.stderr:
        raise RuntimeError('Missing/incompatible libraries.\n' + hint + '\n' + libraries.stdout + libraries.stderr)
    if not shutil.which('xrandr'):
        raise RuntimeError('Missing xrandr.\n' + hint)
    display = subprocess.run(['xrandr', '--current'], capture_output=True, text=True)
    if display.returncode:
        raise RuntimeError('Cannot query the actual X11 display.\n' + display.stderr)
    display_rates(display.stdout)
    profile = ROOT / 'profile'
    if profile.is_symlink():
        raise RuntimeError('Refusing a linked test profile.')
    profile.mkdir(exist_ok=True)
    (profile / 'mods').mkdir(exist_ok=True)
    if not (profile / 'game.xml').exists():
        shutil.copy2(ROOT / 'profile-template/game.xml', profile / 'game.xml')
    stamp = datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S-%f')
    results = ROOT / 'test-results' / stamp
    results.mkdir(parents=True)
    environment = os.environ.copy()
    if args.silent:
        environment['ALSOFT_DRIVERS'] = 'null'
    command_line = subprocess.list2cmdline([
        'FarmingSimulator2015Game.exe', '-profile', 'Z:' + profile.as_posix()])
    command = [str(binary), '--image', str(ROOT / 'resources/x64/FarmingSimulator2015Game.exe'),
               '--command-line', command_line]
    report = dict(scope='Original GE6 Linux test; complete gameplay unverified',
                  system=platform.system(), machine=platform.machine(), kernel=platform.release(),
                  libc=[libc, libc_version], distro=distro, session=os.environ.get('XDG_SESSION_TYPE'),
                  display_mode=display.stdout, silent_audio=args.silent, result=None, stopped_by=None)
    print('Starting original GE6. Click through the three introductions, then Career.', flush=True)
    print('Logs: ' + str(results), flush=True)
    print('Press Ctrl+C here to stop the test if quitting the game fails.', flush=True)
    child = None
    try:
        with (results / 'startup.log').open('wb') as log:
            child = subprocess.Popen(command, cwd=ROOT / 'resources', env=environment,
                                     stdout=log, stderr=subprocess.STDOUT)
            try:
                report['result'] = child.wait(timeout=args.seconds)
            except (subprocess.TimeoutExpired, KeyboardInterrupt) as error:
                report['stopped_by'] = 'time-limit' if isinstance(error, subprocess.TimeoutExpired) else 'operator'
                child.terminate()
                try:
                    report['result'] = child.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    child.kill()
                    report['result'] = child.wait()
    finally:
        if child is not None and child.poll() is None:
            child.kill()
            child.wait()
        journal = profile / 'log.txt'
        if journal.is_file():
            shutil.copy2(journal, results / 'game-log.txt')
        (results / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('Test finished. Return this log folder after reviewing it: ' + str(results), flush=True)
    if report['result'] != 0 and report['stopped_by'] is None:
        print('GE6 stopped with code {}. See startup.log for the unsupported call or fault.'.format(report['result']))
        return 1
    return 0


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (OSError, ValueError, RuntimeError) as error:
        print(str(error), file=sys.stderr)
        raise SystemExit(1)
