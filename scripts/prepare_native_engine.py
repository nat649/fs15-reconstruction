"""Prepare an isolated actual-GE6 entry point from the user's installed FS15.

Original executables/assets are used in place. Only recovered Lua source and
authored probes are staged locally. Preparation is not proof of a native boot.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
from xml.etree import ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
MOD_NAME = 'FS15_ReconstructionProbe'
WHEELS = 'scripts/vehicles/WheelsUtil.luc'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def restart_safe_profile(profile, alias_root):
    """Use a checked junction alias: GE6 restartApplication loses argv quotes."""
    profile = Path(profile).resolve(strict=True)
    if ' ' not in str(profile):
        return profile
    alias = Path(alias_root) / ('FS15Native-' + hashlib.sha256(str(profile).encode()).hexdigest()[:16])
    if ' ' in str(alias):
        raise ValueError('The native restart-safe profile alias must have a path without spaces')
    if not alias.exists() and not alias.is_symlink():
        if os.name != 'nt':
            raise ValueError('Native profile junctions require Windows')
        import _winapi
        _winapi.CreateJunction(str(profile), str(alias))
    if alias.resolve(strict=True) != profile:
        raise ValueError('Native profile alias already points to a different directory')
    return alias


def prepare(root, game, activate_recovered=False, runtime_name='native-engine'):
    root, game = Path(root).resolve(), Path(game).resolve(strict=True)
    if not (game / 'FarmingSimulator2015.xml').is_file() or not (game / 'x64/FarmingSimulator2015Game.exe').is_file():
        raise ValueError('Expected the installed FS15 game directory')
    if runtime_name not in ('native-engine','ge6-local'):
        raise ValueError('Unsupported native runtime directory')
    runtime = root / 'build' / runtime_name
    profile = runtime / 'profile'
    mod = profile / 'mods' / MOD_NAME
    # Refuse links that could redirect a generated profile/mod into the installed
    # game or the normal personal profile, including existing parent junctions.
    for path in (runtime, profile, mod, mod / 'recovered'):
        if not path.resolve().is_relative_to(root / 'build'):
            raise ValueError('Native runtime output must stay inside the workspace build directory')
    if (runtime / 'manifest.json').is_symlink():
        raise ValueError('Refusing a linked native manifest output')
    recovery = json.loads((root / 'analysis/lua-recovery.json').read_text())
    row = next(x for x in recovery['files'] if x['path'] == WHEELS)
    source = root / 'recovered/lua/dataS/scripts/vehicles/WheelsUtil.lua'
    if row['status'] != 'exact_match' or digest(source) != row['source_sha256']:
        raise ValueError('Recovered WheelsUtil source must have a verified bytecode match and unchanged hash')
    target = mod / 'recovered/WheelsUtil.lua'
    if target.is_symlink() or target.with_suffix('.luc').exists():
        raise ValueError('The native probe needs plain source without a preferred neighboring .luc file')
    authored_sources = ('bootstrap.lua', 'verify.lua', 'activate.lua') if activate_recovered else ('bootstrap.lua', 'verify.lua')
    for name in authored_sources:
        if (mod / name).is_symlink() or (mod / name).with_suffix('.luc').exists():
            raise ValueError('Refusing a linked native probe output or preferred neighboring .luc')
    desc_path = mod / 'modDesc.xml'
    if desc_path.is_symlink():
        raise ValueError('Refusing a linked native descriptor output')
    mod.mkdir(parents=True, exist_ok=True)
    (mod / 'recovered').mkdir(exist_ok=True)
    for name in authored_sources:
        target = mod / name
        shutil.copyfile(root / 'src/native_engine' / name, target)
    target = mod / 'recovered/WheelsUtil.lua'
    shutil.copyfile(source, target)
    descriptor = ET.Element('modDesc', descVersion='21')
    ET.SubElement(descriptor, 'author').text = 'FS15 reconstruction contributors'
    ET.SubElement(descriptor, 'version').text = '0.1.0.0'
    ET.SubElement(ET.SubElement(descriptor, 'title'), 'en').text = 'FS15 recovered-script native probe'
    ET.SubElement(ET.SubElement(descriptor, 'description'), 'en').text = 'Compare recovered wheel helpers inside the actual GIANTS Engine 6.'
    ET.SubElement(descriptor, 'multiplayer', supported='false')
    sources = ET.SubElement(descriptor, 'extraSourceFiles')
    source_order = ['bootstrap.lua', 'recovered/WheelsUtil.lua', 'verify.lua']
    if activate_recovered:
        source_order.append('activate.lua')
    for name in source_order:
        ET.SubElement(sources, 'sourceFile', filename=name)
    ET.indent(descriptor)
    ET.ElementTree(descriptor).write(desc_path, encoding='utf-8', xml_declaration=True)
    launcher = root / 'FS15LauncherRebuilt.exe'
    command = [str(launcher), '--game-dir', str(game), '--arch', 'x64', '--', '-profile', profile.as_posix()]
    manifest = dict(scope='Actual GIANTS Engine 6 entry point with an isolated profile and recovered-source probe',
                    game_directory=str(game),
                    recovered_gameplay_enabled=activate_recovered,
                    native_boot_verified=False, recovered_script_execution_verified=False,
                    original_executable_sha256=digest(game / 'x64/FarmingSimulator2015Game.exe'),
                    recovered_wheels_source_sha256=digest(source), profile=str(profile), command=command,
                    mod=MOD_NAME, expected_probe_comparisons=104,
                    evidence_required=['fresh GIANTS Engine Runtime 6.0.2 log', 'FS15_NATIVE_RECOVERED_LUA_OK comparisons=104 during mission loading'])
    if activate_recovered:
        manifest['evidence_required'] += ['FS15_HYBRID_RECOVERED_ACTIVE helpers=3', 'Native vehicle calls to getTireType, getTireFriction and getGroundType']
    (runtime / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    return manifest


def configure_launch(manifest, steam=None, direct=False, dry_run=False):
    profile_arg = restart_safe_profile(manifest['profile'], tempfile.gettempdir())
    manifest['command'][-1] = profile_arg.as_posix()
    game = Path(manifest['game_directory'])
    if not direct and steam is None and game.parent.name.lower() == 'common':
        steam = Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / 'Steam/steam.exe'
    if steam is not None and not direct:
        steam = Path(steam).resolve(strict=True)
        manifest['command'][1:1] = ['--steam', str(steam)]
    if dry_run:
        manifest['command'].insert(1, '--dry-run')
    else:
        from datetime import datetime, timezone
        manifest['launch_requested_utc'] = datetime.now(timezone.utc).isoformat()
    manifest['profile_argument'] = str(profile_arg)
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game-dir', type=Path, required=True)
    parser.add_argument('--launch', action='store_true', help='Start the installed original engine, using the isolated profile')
    parser.add_argument('--dry-run', action='store_true', help='With --launch, print/check the launch command without starting a process')
    parser.add_argument('--steam', type=Path, help='Installed Steam client; defaults to the standard Steam path for Steam installations')
    parser.add_argument('--direct', action='store_true', help='Launch the installed executable directly (for non-Steam copies)')
    parser.add_argument('--activate-recovered', action='store_true', help='After parity checks, use recovered wheel helpers in native gameplay')
    args = parser.parse_args()
    if args.launch and not args.dry_run and os.name == 'nt':
        from runtime_snapshot import processes
        if list(processes()):
            parser.error('FS15 is already running. Close that session before launching the isolated runtime again.')
    manifest = prepare(ROOT, args.game_dir, args.activate_recovered)
    if args.launch:
        if not Path(manifest['command'][0]).is_file():
            parser.error('Build FS15LauncherRebuilt before launching the original engine')
        configure_launch(manifest, args.steam, args.direct, args.dry_run)
        (ROOT / 'build/native-engine/manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(json.dumps(manifest, indent=2), flush=True)
    if args.launch:
        subprocess.run(manifest['command'], check=True)


if __name__ == '__main__':
    main()
