"""Prepare the original starting fleets and their linked wheel meshes locally.

The runtime I3D is a derived cache: original hierarchy/materials, with the same
dynamic-part links made by Vehicle.loadDynamicallyPartsFromXML. Never publish it.
"""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]


def mesh_records(path):
    data = path.read_bytes()
    if data[:8] != b'FS15MSH\x01':
        raise ValueError('Unexpected mesh cache')
    records, pos = [], 12
    for _ in range(struct.unpack_from('<I', data, 8)[0]):
        ident, nv, ni, ns = struct.unpack_from('<4I', data, pos)
        end = pos + 16 + ns * 8 + nv * 32 + ni * 4
        records.append((ident, data[pos+4:end]))
        pos = end
    if pos != len(data):
        raise ValueError('Mesh cache length mismatch')
    return records


def resolve_node(scene, index, components=False):
    if components:
        comp, _, rest = index.partition('>')
        if not _:
            comp, rest = '0', index
        node = list(scene)[int(comp)]
        index = rest
    else:
        node = scene
    for part in index.split('|'):
        if part:
            node = list(node)[int(part)]
    return node


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game-dir', type=Path, required=True)
    args = parser.parse_args()
    base = args.game_dir.resolve(strict=True)
    dest = ROOT/'recovered/gar'
    bridge = ROOT/'build/shape-bridge/FS15ShapeBridge.dll'
    provenance, staged = {}, set()

    def relative(path, parent=Path()):
        value = str(path)
        resolved = (base / (value[1:] if value.startswith('$data/') else parent / value)).resolve()
        return resolved.relative_to(base)

    def stage(rel):
        source, target = base/rel, dest/rel
        digest = hashlib.sha256(source.read_bytes()).hexdigest()
        if not target.is_file() or hashlib.sha256(target.read_bytes()).hexdigest() != digest:
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, target)
        provenance[rel.as_posix()] = dict(sha256=digest, bytes=source.stat().st_size)
        return target

    stats = []

    def prepare(config_rel):
        if config_rel in staged:
            return
        staged.add(config_rel)
        config = ET.parse(stage(config_rel)).getroot()
        scene_rel = relative(config.findtext('filename'), config_rel.parent) if not config.findtext('filename').startswith('data/') else Path(config.findtext('filename'))
        original = ET.parse(stage(scene_rel)).getroot()
        generated = ET.Element('i3D', original.attrib)
        files, materials, scene = (ET.SubElement(generated, tag) for tag in ('Files', 'Materials', 'Scene'))
        records = []
        part_stats = []

        def import_scene(rel):
            tree = ET.parse(stage(rel)).getroot()
            cache = ROOT/'build/mission-meshes'/rel.with_suffix('.meshes')
            cache.parent.mkdir(parents=True, exist_ok=True)
            source = base/(str(rel)+'.shapes')
            result = subprocess.run(['dotnet', str(bridge), str(source), str(cache)], check=True, capture_output=True, text=True)
            info = json.loads(result.stdout)
            part_stats.append({k: info[k] for k in ('meshes', 'vertices', 'triangles', 'skinned')})
            # Preserve rigid geometry only; refuse to silently flatten skinning.
            if info['skinned']:
                raise ValueError(f'Skeletal vehicle mesh needs skinning support: {rel}')
            shape_ids = {}
            for ident, record in mesh_records(cache):
                shape_ids[str(ident)] = str(len(records)+1)
                records.append((len(records)+1, record))
            file_ids = {}
            for item in tree.find('Files'):
                new = copy.deepcopy(item)
                old = new.get('fileId')
                file_ids[old] = str(len(files)+1)
                name = new.get('filename', '')
                if name:
                    path = relative(name, rel.parent)
                    # GE6 texture references often use .png while the shipped file is DDS.
                    if not (base/path).is_file() and (base/path.with_suffix('.dds')).is_file():
                        path = path.with_suffix('.dds')
                    new.set('filename', '$'+path.as_posix())
                    if (base/path).is_file():
                        stage(path)
                new.set('fileId', file_ids[old])
                files.append(new)
            material_ids = {}
            for item in tree.find('Materials'):
                new = copy.deepcopy(item)
                old = new.get('materialId')
                material_ids[old] = str(len(materials)+1)
                new.set('materialId', material_ids[old])
                for attr in ('customShaderId',):
                    if new.get(attr) in file_ids:
                        new.set(attr, file_ids[new.get(attr)])
                for child in new:
                    if child.get('fileId') in file_ids:
                        child.set('fileId', file_ids[child.get('fileId')])
                materials.append(new)
            imported = copy.deepcopy(tree.find('Scene'))
            for node in imported.iter():
                if node.get('shapeId'):
                    node.set('shapeId', shape_ids[node.get('shapeId')])
                if node.get('materialIds'):
                    node.set('materialIds', ' '.join(material_ids[k] for k in node.get('materialIds').split()))
            return imported

        main_scene = import_scene(scene_rel)
        scene.extend(main_scene)
        # Resolve links against the untouched hierarchy before appending children.
        links = []
        for part in config.findall('dynamicallyLoadedParts/dynamicallyLoadedPart'):
            target = resolve_node(scene, part.get('linkNode', '0>'), True)
            linked_scene = import_scene(relative(part.get('filename'), config_rel.parent))
            selected = resolve_node(linked_scene, part.get('node', '0|0'))
            node = copy.deepcopy(selected)
            for src, dst in (('position', 'translation'), ('rotation', 'rotation')):
                if part.get(src):
                    node.set(dst, part.get(src))
            links.append((target, node))
        for target, node in links:
            target.append(node)
        for ident, node in enumerate(scene.iter(), 1):
            if node is not scene:
                node.set('nodeId', str(ident))
        runtime_rel = scene_rel.with_suffix('.runtime.i3d')
        runtime = dest/runtime_rel
        runtime.parent.mkdir(parents=True, exist_ok=True)
        ET.ElementTree(generated).write(runtime, encoding='utf-8', xml_declaration=True)
        cache = runtime.with_suffix('.meshes')
        with cache.open('wb') as output:
            output.write(b'FS15MSH\x01'+struct.pack('<I', len(records)))
            for ident, record in records:
                output.write(struct.pack('<I', ident)+record)
        stats.append(dict(config=config_rel.as_posix(), runtime=runtime_rel.as_posix(), linkedParts=len(links),
                          meshes=len(records), vertices=sum(x['vertices'] for x in part_stats),
                          triangles=sum(x['triangles'] for x in part_stats), skinned=0))

    for map_name in ('map01', 'map02'):
        for name in ('fruit_density.grle','cultivator_density.grle','infoLayer.grle'):
            stage(Path('data/maps')/map_name/name)
        template = Path('profileTemplate')/f'careerVehicles_{map_name}.xml'
        for vehicle in ET.parse(stage(template)).getroot().findall('vehicle'):
            prepare(Path(vehicle.get('filename')))
    private = dict(files=provenance, vehicles=stats)
    (ROOT/'recovered/mission-provenance.json').write_text(json.dumps(private, indent=2)+'\n')
    summary = dict(scope='Original starting vehicle XML/I3D plus linked original wheels; derived local caches',
                   configurations=len(stats), sourceFiles=len(provenance), vehicles=stats)
    (ROOT/'analysis/mission-recovery.json').write_text(json.dumps(summary, indent=2)+'\n')
    print(json.dumps(summary, indent=2))


if __name__ == '__main__':
    main()
