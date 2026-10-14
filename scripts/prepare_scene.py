"""Stage original GIANTS map meshes/textures with local provenance, never replacements."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--game-dir', type=Path, default=os.environ.get('FS15_GAME_DIR'))
    args = parser.parse_args()
    if args.game_dir is None:
        parser.error('Pass --game-dir with the installed FS15 directory')
    base = Path(args.game_dir).resolve()
    bridge = ROOT/'build/shape-bridge/FS15ShapeBridge.dll'
    if not bridge.is_file():
        raise RuntimeError('Build the pinned decoder first: scripts/setup_shapes_tools.ps1')
    dest = ROOT/'recovered/gar'
    files, maps, missing = {}, [], set()

    def copy(relative):
        source = (base/relative).resolve()
        relative = source.relative_to(base)
        target = dest/relative
        if not source.is_file():
            missing.add(relative.as_posix())
            return
        source_hash = sha(source)
        if not target.is_file() or sha(target) != source_hash:
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source,target)
        files[relative.as_posix()] = dict(sha256=source_hash,bytes=source.stat().st_size)

    for map_name in ('map01','map02'):
        scene_rel = Path('data/maps')/(map_name+'.i3d')
        copy(scene_rel)
        original = base/(str(scene_rel)+'.shapes')
        cached = dest/scene_rel.with_suffix('.meshes')
        result = subprocess.run(['dotnet',str(bridge),str(original),str(cached)],check=True,capture_output=True,text=True)
        stats = json.loads(result.stdout)
        tree = ET.parse(base/scene_rel)
        references = {f.get('fileId'):f.get('filename') for f in tree.getroot().find('Files')}
        names = set()
        for material in tree.getroot().find('Materials'):
            for texture in material.findall('Texture'):
                name = references[texture.get('fileId')]
                if name:
                    rel = Path(name[1:]) if name.startswith('$data/') else scene_rel.parent/name
                    names.add(rel.with_suffix('.dds'))
        for layer in tree.findall('.//TerrainTransformGroup/Layers/Layer'):
            names.add((scene_rel.parent/references[layer.get('detailMapId')]).with_suffix('.dds'))
            names.add(scene_rel.parent/references[layer.get('weightMapId')])
        for name in names:
            copy(name)
        stats.update(map=map_name,originalShapesSha256=sha(original),cacheSha256=sha(cached),cacheBytes=cached.stat().st_size)
        maps.append(stats)
    provenance = dict(upstream='https://github.com/Donkie/I3DShapesTool',
                      upstreamCommit='1a2ecf3125d6754d9100c342419003bf048dbec5',
                      license='MIT',scope='Original binary meshes, no authored replacement objects',
                      maps=maps,files=files,unusedMissingTextureReferences=sorted(missing))
    (ROOT/'recovered/scene-provenance.json').write_text(json.dumps(provenance,indent=2)+'\n')
    summary = {k:v for k,v in provenance.items() if k not in ('files',)}
    summary['copiedResourceCount']=len(files)
    # Names and vertices from original meshes stay local; publish aggregate evidence.
    for stats in summary['maps']:
        stats.pop('samples',None)
    (ROOT/'analysis/scene-recovery.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(json.dumps(summary,indent=2))


if __name__ == '__main__':
    main()
