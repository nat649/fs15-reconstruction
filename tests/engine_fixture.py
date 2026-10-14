"""Authored I3D/RG16/DDS fixture exercises the real native D3D11 WARP path."""
from __future__ import annotations
import argparse
import json
import math
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import zlib


def chunk(kind: bytes, data: bytes) -> bytes:
    return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('--exe', required=True, type=Path)
    parser.add_argument('--fixtures', required=True, type=Path)
    args = parser.parse_args()
    parent = Path(os.path.abspath(args.fixtures))
    parent.mkdir(parents=True, exist_ok=True)
    temporary = tempfile.TemporaryDirectory(prefix='engine-authored-', dir=parent)
    root = Path(os.path.abspath(temporary.name))
    if root.parent != parent or not root.name.startswith('engine-authored-'):
        raise RuntimeError('Fixture must remain within its verified build directory')
    try:
        maps = root / 'assets/data/maps'
        textures = maps / 'map01'
        textures.mkdir(parents=True)
        values = [round(32768 + 10000 * math.sin(x / 10) * math.sin(z / 10))
                  for z in range(33) for x in range(33)]
        rows = []
        for z in range(33):
            rows.append(b'\0' + b''.join(bytes((v >> 8, v & 255, 0)) for v in values[z*33:(z+1)*33]))
        png = (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 33, 33, 8, 2, 0, 0, 0))
               + chunk(b'IDAT', zlib.compress(b''.join(rows))) + chunk(b'IEND', b''))
        (textures / 'dem.png').write_bytes(png)
        header = bytearray(128)
        header[:4] = b'DDS '
        for offset, value in [(4,124), (8,0x81007), (12,4), (16,4), (20,8),
                              (28,1), (76,32), (80,4), (84,0x31545844), (108,0x1000)]:
            struct.pack_into('<I', header, offset, value)
        (textures / 'pda_map_H.dds').write_bytes(header + struct.pack('<HHI', 0x07e0, 0x001f, 0x44332211))
        scene = ('<i3D><Files><File fileId="1" filename="map01/dem.png"/></Files><Scene>'
                 '<TerrainTransformGroup nodeId="1" heightMapId="1" heightScale="255" unitsPerPixel="2"/>'
                 '<TransformGroup nodeId="2" name="deferred"><Shape nodeId="3" shapeId="1"/></TransformGroup>'
                 '</Scene></i3D>')
        (maps / 'map01.i3d').write_text(scene, encoding='utf-8')
        report, image = root / 'result.json', root / 'result.png'
        command = [str(args.exe), '--root', str(root), '--map', 'Map01', '--smoke', '--warp',
                   '--report', str(report), '--capture', str(image)]
        subprocess.run(command, check=True, timeout=30)
        data = json.loads(report.read_text())
        assert data['ready'] and data['driver'] == 'WARP' and not data['original_engine_loaded']
        assert data['scene_nodes'] == 3 and data['source_shape_nodes'] == 1
        assert data['source_width'] == 33 and data['world_width'] == 64
        assert data['vertices'] == 289 and data['triangles'] == 512
        assert abs(data['min_height'] - min(values) * 255 / 65535) < 0.001
        assert abs(data['max_height'] - max(values) * 255 / 65535) < 0.001
        assert data['visible_gpu_samples'] > 0 and data['camera_math_check']
        assert image.read_bytes().startswith(b'\x89PNG')
        # A real mesh above the terrain exercises hierarchy/rotation/scale,
        # subset upload, instancing, material color and independent GPU samples.
        vertices = [(-10,0,-10,0,1,0,0,0), (10,0,-10,0,1,0,1,0), (0,10,10,0,1,0,0.5,1)]
        mesh = (b'FS15MSH\x01' + struct.pack('<I',1) + struct.pack('<4I',7,3,3,1)
                + struct.pack('<2I',0,3) + b''.join(struct.pack('<8f',*v) for v in vertices)
                + struct.pack('<3I',0,1,2))
        (maps / 'map01.meshes').write_bytes(mesh)
        object_scene = scene.replace('<Scene>', '<Materials><Material materialId="1" diffuseColor="0.7 0.3 0.2"/></Materials><Scene>')
        object_scene = object_scene.replace('</Scene>',
            '<TransformGroup name="fixture" translation="4 180 -4" rotation="0 90 0" scale="2 2 2" nodeId="4">'
            '<Shape shapeId="7" materialIds="1" translation="1 0 2" nodeId="5"/>'
            '<Shape shapeId="7" materialIds="1" nonRenderable="true" nodeId="6"/>'
            '<Shape name="fixture_lod1" shapeId="7" materialIds="1" nodeId="7"/>'
            '</TransformGroup></Scene>')
        (maps / 'map01.i3d').write_text(object_scene, encoding='utf-8')
        subprocess.run(command, check=True, timeout=30)
        data = json.loads(report.read_text())
        assert data['decoded_meshes'] == 1 and data['rendered_instances'] == 1 and data['object_triangles'] == 1
        assert data['visible_object_gpu_samples'] > 0
        matrix = data['first_instance_world']
        assert all(abs(matrix[12+i] - expected) < 0.001 for i,expected in enumerate([8,180,-6]))
        assert abs(matrix[2]+2) < 0.001 and abs(matrix[8]-2) < 0.001
        (maps / 'map01.meshes').write_bytes(mesh[:-1])
        assert subprocess.run(command, timeout=30).returncode != 0
        (maps / 'map01.meshes').write_bytes(mesh)
        # Both malformed XML and missing terrain resources must fail explicitly.
        for invalid in ['<!DOCTYPE i3D [<!ENTITY x SYSTEM "file:///invalid">]><i3D>&x;</i3D>',
                        scene.replace('map01/dem.png', 'map01/missing.png')]:
            (maps / 'map01.i3d').write_text(invalid, encoding='utf-8')
            assert subprocess.run(command, timeout=30).returncode != 0
        print('Authored heights, transformed/filtered mesh instances, WARP terrain/object visibility, camera and failure fixtures passed')
    finally:
        if Path(os.path.abspath(temporary.name)).parent != parent:
            raise RuntimeError('Unsafe fixture cleanup target')
        temporary.cleanup()


if __name__ == '__main__':
    main()
