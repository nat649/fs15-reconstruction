"""Local-only original I3D adapter for the GE6 recovered-draw integration host.

No geometry replacements: mesh bytes come from the existing pinned decoder cache.
This is authored staging, not the recovered GE6 resource loader or shader system.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import shutil
import struct
import xml.etree.ElementTree as ET


def vector(element, key, default):
    values = tuple(map(float, element.get(key, default).split()))
    if len(values) != 3 or not all(map(math.isfinite, values)):
        raise ValueError(f'Invalid {key}')
    return values


def stage(scene, output):
    from PIL import Image
    scene, output = scene.resolve(), output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    root = ET.parse(scene).getroot()
    files = {e.get('fileId'): e.get('filename') for e in root.find('Files')}
    # Resolve $data relative to the enclosing data directory, never a network path.
    data = next((p for p in scene.parents if p.name.lower() == 'data'), None)

    def resource(name):
        if not name:
            return None
        if name.startswith('$data/'):
            return data / name[6:] if data else None
        return scene.parent / name

    textures, missing, materials = {}, set(), []
    for element in root.find('Materials'):
        color = tuple(map(float, element.get('diffuseColor', '1 1 1 1').split()))
        if len(color) == 3:
            color = (*color, 1.)
        if len(color) != 4 or not all(map(math.isfinite, color)):
            raise ValueError('Invalid diffuseColor')
        texture = element.find('Texture')
        name = files.get(texture.get('fileId')) if texture is not None else None
        source = resource(name)
        if source:
            source = source.with_suffix('.dds').resolve()
        texture_name = ''
        if source and source.is_file():
            if source not in textures:
                texture_name = f'texture-{len(textures)}.rgba'
                with Image.open(source) as image:
                    image = image.convert('RGBA')
                    (output / texture_name).write_bytes(struct.pack('<II', *image.size) + image.tobytes())
                textures[source] = texture_name
            texture_name = textures[source]
        elif source:
            missing.add(name)
        # GE6 shader-driven foliage cutouts need temporary explicit GL binding.
        foliage = element.get('name', '').lower().startswith(('treebranch', 'treelod', 'treescape'))
        alpha = element.get('alphaTest') == 'true' or element.get('alphaBlending') == 'true' or foliage
        materials.append((int(element.get('materialId')), color, int(alpha), texture_name))

    nodes = []
    camera = (273.242, 163.39, 187.108, 48.104, -2.1156)
    tags = {'TransformGroup', 'TerrainTransformGroup', 'Shape', 'Camera', 'Light', 'ReferenceNode'}

    def walk(element, parent=-1):
        nonlocal camera
        for child in element:
            if child.tag not in tags:
                continue
            index = len(nodes)
            t = vector(child, 'translation', '0 0 0')
            r = tuple(math.radians(x) for x in vector(child, 'rotation', '0 0 0'))
            s = vector(child, 'scale', '1 1 1')
            # Temporary fixed LOD0 policy; retains all nodes and original transforms.
            hidden = child.get('visibility') == 'false' or bool(re.search(r'lod[1-9]', child.get('name', '').lower()))
            render = child.tag == 'Shape' and child.get('nonRenderable') != 'true' and child.get('trigger') != 'true'
            ids = tuple(map(int, child.get('materialIds', '').split())) if render else ()
            clip = float(child.get('clipDistance', '10000'))
            if not math.isfinite(clip) or clip < 0:
                raise ValueError('Invalid clip distance')
            nodes.append((parent, int(child.get('shapeId', '0')), int(not hidden), (*t, *r, *s, clip), ids))
            if child.tag == 'Camera' and child.get('name') == 'persp' and parent == -1:
                camera = (*t, math.degrees(r[1]), math.degrees(r[0]))
            walk(child, index)
    walk(root.find('Scene'))
    mesh_source = scene.with_suffix('.meshes')
    shutil.copyfile(mesh_source, output / 'scene.meshes')
    terrain = root.find('.//Scene/TerrainTransformGroup')
    terrain_samples = 0
    if terrain is not None:
        import numpy as np
        if (vector(terrain, 'translation', '0 0 0') != (0., 0., 0.) or
                vector(terrain, 'rotation', '0 0 0') != (0., 0., 0.) or
                vector(terrain, 'scale', '1 1 1') != (1., 1., 1.)):
            raise ValueError('Only the untransformed base-map terrain is supported')
        # Full DEM resolution and original spacing/height scale; no invented relief.
        with Image.open(resource(files[terrain.get('heightMapId')])) as image:
            pixels = np.asarray(image)
            if image.mode == 'RGB':
                if np.any(pixels[:, :, 2]):
                    raise ValueError('Unsupported packed DEM: B must be zero')
                pixels = pixels[:, :, 0].astype(np.uint16) * 256 + pixels[:, :, 1]
            elif image.mode not in ('I;16', 'I;16B', 'I'):
                raise ValueError('Unsupported DEM: expected RG16 or Gray16')
            heights = pixels.astype(np.float32) * (float(terrain.get('heightScale', '255')) / 65535.)
        nz, nx = heights.shape
        spacing = float(terrain.get('unitsPerPixel', '1'))
        if min(nx, nz) < 3 or max(nx, nz) > 4097 or not math.isfinite(spacing) or spacing <= 0 or not np.isfinite(heights).all():
            raise ValueError('Invalid terrain dimensions, spacing or heights')
        dz, dx = np.gradient(heights, spacing)
        length = np.sqrt(dx * dx + dz * dz + 1.)
        vertices = np.zeros((nz, nx, 8), dtype='<f4')
        vertices[:, :, 0] = (np.arange(nx) - (nx - 1) / 2.) * spacing
        vertices[:, :, 1] = heights
        vertices[:, :, 2] = ((np.arange(nz) - (nz - 1) / 2.) * spacing)[:, None]
        vertices[:, :, 3] = -dx / length
        vertices[:, :, 4] = 1. / length
        vertices[:, :, 5] = -dz / length
        vertices[:, :, 6] = np.arange(nx) / (nx - 1)
        vertices[:, :, 7] = (np.arange(nz) / (nz - 1))[:, None]
        a = (np.arange(nz - 1)[:, None] * nx + np.arange(nx - 1)).astype('<u4')
        indices = np.stack((a, a + nx, a + 1, a + 1, a + nx, a + nx + 1), axis=-1).astype('<u4')
        terrain_mesh = 0xfffffffe
        terrain_material = 0xfffffffe
        with (output / 'scene.meshes').open('r+b') as stream:
            stream.seek(8)
            count = struct.unpack('<I', stream.read(4))[0]
            stream.seek(8)
            stream.write(struct.pack('<I', count + 1))
            stream.seek(0, 2)
            stream.write(struct.pack('<6I', terrain_mesh, nx * nz, indices.size, 1, 0, indices.size))
            stream.write(vertices.tobytes())
            stream.write(indices.tobytes())
        materials.append((terrain_material, (.26, .34, .16, 1.), 0, ''))
        nodes.append((-1, terrain_mesh, 1, (0., 0., 0., 0., 0., 0., 1., 1., 1., 10000.), (terrain_material,)))
        terrain_samples = nx * nz
    with (output / 'scene.ge6scene').open('wb') as stream:
        stream.write(b'GE6OBJ01')
        stream.write(struct.pack('<II5f', len(materials), len(nodes), *camera))
        for mid, color, alpha, name in materials:
            encoded = name.encode('utf-8')
            stream.write(struct.pack('<I4fII', mid, *color, alpha, len(encoded)) + encoded)
        for parent, mesh, flags, trs, ids in nodes:
            stream.write(struct.pack('<iII10fI', parent, mesh, flags, *trs, len(ids)))
            stream.write(struct.pack(f'<{len(ids)}I', *ids))
    report = dict(scope='Original object geometry and hierarchy; authored local adapter; fixed LOD0 and temporary fixed-function materials',
                  nodes=len(nodes), materials=len(materials), textures=len(textures), missingTextures=sorted(missing),
                  meshBytes=mesh_source.stat().st_size, camera=camera, terrainSamples=terrain_samples,
                  sourceSceneSha256=hashlib.sha256(scene.read_bytes()).hexdigest(),
                  sourceMeshCacheSha256=hashlib.sha256(mesh_source.read_bytes()).hexdigest(),
                  terrainMaterial='Temporary constant color; original terrain layer shaders are not connected')
    (output / 'staging-report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--scene', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True, help='Local ignored directory, e.g. build/ge6-objects/map01')
    args = parser.parse_args()
    stage(args.scene, args.output)
