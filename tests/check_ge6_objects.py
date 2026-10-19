"""Actual framebuffer integration check using only authored geometry/resources."""
import argparse
from pathlib import Path
import struct
import subprocess


def check(exe, workdir):
    workdir.mkdir(parents=True, exist_ok=True)
    # One triangle, shared by two objects, with an intermediate transformed parent.
    vertices = b''.join(struct.pack('<8f', x, y, 0, 0, 0, 1, 0, 0)
                        for x, y in ((-.4, -.4), (.4, -.4), (0, .4)))
    (workdir / 'scene.meshes').write_bytes(b'FS15MSH\1' + struct.pack('<7I', 1, 7, 3, 3, 1, 0, 3)
                                        + vertices + struct.pack('<3I', 0, 1, 2))
    (workdir / 'green.rgba').write_bytes(struct.pack('<II4B', 1, 1, 0, 255, 0, 255))
    pack = bytearray(b'GE6OBJ01' + struct.pack('<II5f', 2, 3, 0, 0, 3, 0, 0))
    for mid, color, texture in ((1, (1, 0, 0, 1), b''), (2, (1, 1, 1, 1), b'green.rgba')):
        pack += struct.pack('<I4fII', mid, *color, 0, len(texture)) + texture
    for parent, shape, x, ids in ((-1, 0, -.6, ()), (0, 7, 0, (1,)), (-1, 7, .6, (2,))):
        pack += struct.pack('<iII10fI', parent, shape, 1, x, 0, 0, 0, 0, 0, 1, 1, 1, 100, len(ids))
        pack += struct.pack(f'<{len(ids)}I', *ids)
    scene = workdir / 'scene.ge6scene'
    scene.write_bytes(pack)
    capture = workdir / 'frame.bmp'
    subprocess.run([str(exe.resolve()), '--scene', str(scene), '--capture', str(capture), '--frames', '1'], check=True)
    image = capture.read_bytes()
    offset = struct.unpack_from('<I', image, 10)[0]
    width, height = struct.unpack_from('<ii', image, 18)
    assert struct.unpack_from('<H', image, 28)[0] == 24
    pitch = (width * 3 + 3) & ~3

    def pixel(x, y):
        b, g, r = image[offset + (height - 1 - y) * pitch + x * 3:][:3]
        return r, g, b
    red, green = pixel(515, 370), pixel(765, 370)
    assert red[0] > 100 and red[1] < 10 and red[2] < 10, red
    assert green[1] > 100 and green[0] < 10 and green[2] < 10, green
    assert pixel(640, 370)[2] > 150, 'Background between objects was overwritten'
    # Bounds rejection must fail rather than submit invalid GPU index data.
    broken = bytearray((workdir / 'scene.meshes').read_bytes())
    struct.pack_into('<I', broken, len(broken) - 4, 99)
    (workdir / 'scene.meshes').write_bytes(broken)
    result = subprocess.run([str(exe.resolve()), '--scene', str(scene), '--frames', '1'], capture_output=True, text=True)
    assert result.returncode != 0 and 'Mesh index out of bounds' in result.stderr, result
    print('GE6 object framebuffer: hierarchy placement, shared mesh, diffuse color, original-format texture transport and bounds rejection passed (authored fixtures)')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--exe', required=True, type=Path)
    parser.add_argument('--workdir', required=True, type=Path)
    args = parser.parse_args()
    check(args.exe, args.workdir)
