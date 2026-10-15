"""Prepare an isolated actual-GE6 entry point from the user's installed FS15.

Original executables/assets are used in place. Only recovered Lua source and
authored probes are staged locally. Preparation is not proof of a native boot.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
from xml.etree import ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
MOD_NAME = 'FS15_ReconstructionProbe'
WHEELS = 'scripts/vehicles/WheelsUtil.luc'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def prepare(root, game):
    root, game = Path(root).resolve(), Path(game).resolve(strict=True)
    if not (game / 'FarmingSimulator2015.xml').is_file() or not (game / 'x64/FarmingSimulator2015Game.exe').is_file():
        raise ValueError('Expected the installed FS15 game directory')
    runtime = root / 'build/native-engine'
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
    for name in ('bootstrap.lua', 'verify.lua'):
        if (mod / name).is_symlink():
            raise ValueError('Refusing a linked native probe output')
    desc_path = mod / 'modDesc.xml'
    if desc_path.is_symlink():
        raise ValueError('Refusing a linked native descriptor output')
    mod.mkdir(parents=True, exist_ok=True)
    (mod / 'recovered').mkdir(exist_ok=True)
    for name in ('bootstrap.lua', 'verify.lua'):
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
    for name in ('bootstrap.lua', 'recovered/WheelsUtil.lua', 'verify.lua'):
        ET.SubElement(sources, 'sourceFile', filename=name)
    ET.indent(descriptor)
    ET.ElementTree(descriptor).write(desc_path, encoding='utf-8', xml_declaration=True)
    launcher = root / 'FS15LauncherRebuilt.exe'
    command = [str(launcher), '--game-dir', str(game), '--arch', 'x64', '--', '-profile', profile.as_posix()]
    manifest = dict(scope='Actual GIANTS Engine 6 entry point with an isolated profile and recovered-source probe',
                    native_boot_verified=False, recovered_script_execution_verified=False,
                    original_executable_sha256=digest(game / 'x64/FarmingSimulator2015Game.exe'),
                    recovered_wheels_source_sha256=digest(source), profile=str(profile), command=command,
                    mod=MOD_NAME, expected_probe_comparisons=104,
                    evidence_required=['fresh GIANTS Engine Runtime 6.0.2 log', 'FS15_NATIVE_RECOVERED_LUA_OK comparisons=104 during mission loading'])
    (runtime / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game-dir', type=Path, required=True)
    parser.add_argument('--launch', action='store_true', help='Start the installed original engine, using the isolated profile')
    args = parser.parse_args()
    manifest = prepare(ROOT, args.game_dir)
    print(json.dumps(manifest, indent=2))
    if args.launch:
        if not Path(manifest['command'][0]).is_file():
            parser.error('Build FS15LauncherRebuilt before launching the original engine')
        subprocess.run(manifest['command'], check=True)


if __name__ == '__main__':
    main()
