"""SDL frontend transport checks with entirely authored resources.

This verifies drawing pixels and failures, not the recovered original menu.
Original-resource testing is deliberately private and separate.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import zlib


def dds(width, height, pixels):
    header = [0] * 31
    header[0:5] = [124, 0x100F, height, width, width * 4]
    header[18:26] = [32, 0x41, 0, 32, 0xFF0000, 0xFF00, 0xFF, 0xFF000000]
    header[26] = 0x1000
    return b'DDS ' + struct.pack('<31I', *header) + struct.pack('<'+'I'*len(pixels), *pixels)


def read_png(path):
    data = path.read_bytes()
    assert data[:8] == b'\x89PNG\r\n\x1a\n'
    pos = 8
    packed = b''
    while pos < len(data):
        size = struct.unpack_from('>I', data, pos)[0]
        kind, body = data[pos+4:pos+8], data[pos+8:pos+8+size]
        if kind == b'IHDR':
            width, height, depth, color, _, _, interlace = struct.unpack('>IIBBBBB', body)
            assert depth == 8 and color in (2, 6) and interlace == 0
        if kind == b'IDAT':
            packed += body
        pos += size + 12
    bpp = 4 if color == 6 else 3
    raw = zlib.decompress(packed)
    rows, previous = [], bytearray(width*bpp)
    for y in range(height):
        offset = y*(width*bpp+1)
        method = raw[offset]
        row = bytearray(raw[offset+1:offset+1+width*bpp])
        for x in range(len(row)):
            a = row[x-bpp] if x >= bpp else 0
            b, c = previous[x], previous[x-bpp] if x >= bpp else 0
            if method == 0:
                prediction = 0
            elif method == 1:
                prediction = a
            elif method == 2:
                prediction = b
            elif method == 3:
                prediction = (a+b)//2
            elif method == 4:
                p = a+b-c
                distances = [abs(p-v) for v in (a, b, c)]
                prediction = (a, b, c)[distances.index(min(distances))]
            else:
                raise AssertionError('Unknown PNG filter')
            row[x] = (row[x]+prediction) & 255
        rows.append(row)
        previous = row
    return width, height, lambda x, y: tuple(rows[y][x*bpp:x*bpp+3])


BOOTSTRAP = '''-- Authored transport fixture. No original game logic or assets.
source('dataS/scripts/probe.lua')
local image
function fs15_menu_selfcheck()
    assert(probeLoaded)
    image=createImageOverlay('probe.dds')
end
function fs15_menu_show(name) assert(name=='MainScreen') end
function fs15_menu_update(dt) assert(dt==16) end
function fs15_menu_draw()
    setOverlayColor(image,1,0,0,1)
    renderOverlay(image,0.1,0.1,0.8,0.8)
    set2DClipArea(0.25,0.25,0.5,0.5)
    setOverlayColor(image,0,0,1,1)
    renderOverlay(image,0.1,0.1,0.8,0.8)
    reset2DClipArea()
    setTextColor(0,1,0,1)
    renderText(0.8,0.8,0.05,'ABC')
end
function fs15_menu_test_inputs()
    assert(not menuLaunchWorld('Map01',2,1))
    menuUnavailable('authored fixture boundary')
    doExit()
end
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, required=True)
    parser.add_argument('--workdir', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    exe = args.exe.resolve()
    args.workdir.mkdir(parents=True, exist_ok=True)
    checks = []
    with tempfile.TemporaryDirectory(dir=args.workdir.resolve(), prefix='sdl-') as tmp:
        tmp = Path(tmp)
        root = tmp/'resource directory'
        font = root/'assets/shared/font'
        font.mkdir(parents=True)
        scripts = root/'lua/dataS/scripts'
        scripts.mkdir(parents=True)
        (scripts/'probe.lua').write_text('probeLoaded=true\n')
        (root/'bootstrap.lua').write_text(BOOTSTRAP)
        (root/'assets/probe.dds').write_bytes(dds(2, 2, [0xFFFFFFFF]*4))
        (font/'defaultFont.dds').write_bytes(dds(8, 8, [0xFFFFFFFF]*64))
        metrics = struct.pack('>fII', 40, 0, 95)
        for char in range(32, 127):
            metrics += struct.pack('>I', char)
            for bold in (False, True):
                metrics += struct.pack('>HHBBbbf', 0, 0, 8, 8, 0, 8, 10 if bold else 8)
        (font/'defaultFont.font').write_bytes(metrics)
        before = {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest()
                  for p in root.rglob('*') if p.is_file()}
        env = dict(os.environ, SDL_VIDEODRIVER='dummy')

        def run(*options, expected=0):
            proc = subprocess.run([str(exe), *map(str, options)], cwd=tmp, env=env,
                                  capture_output=True, text=True, timeout=60)
            assert proc.returncode == expected, (proc.returncode, proc.stdout, proc.stderr)
            return proc

        run('--help')
        checks.append('help_without_assets')
        image = tmp/'frame.png'
        output = tmp/'result'
        run('--root', root, '--output-dir', output, '--smoke', '--test-inputs', '--screenshot', image)
        log = (output/'menu-runtime.log').read_text()
        assert all(marker in log for marker in ('BOOT_OK', 'SMOKE_OK', 'INPUT_CHECK_OK', 'WORLD_UNAVAILABLE', 'SHUTDOWN exit=0'))
        checks.append('bootstrap_source_input_callbacks_and_world_boundary')
        w, h, pixel = read_png(image)
        assert (w, h) == (1280, 720)
        for xy, rgb in [((10, 10), (0, 0, 0)), ((200, 360), (255, 0, 0)), ((640, 360), (0, 0, 255)), ((640, 600), (255, 0, 0))]:
            assert pixel(*xy) == rgb, (xy, pixel(*xy), rgb)
        assert any(pixel(x, y)[1] > 200 for y in range(130, 150) for x in range(1020, 1050))
        checks.append('png_dimensions_overlay_tint_uv_origin_clip_and_font_pixels')
        run('--root', root, '--output-dir', output, '--smoke', '--screen', 'UnknownScreen', expected=1)
        checks.append('unknown_screen_failure')
        after = {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest()
                 for p in root.rglob('*') if p.is_file()}
        assert before == after
        checks.append('resources_unchanged')
        (scripts/'probe.lua').write_bytes(b'\x1bLJfake')
        run('--root', root, '--output-dir', output, '--smoke', expected=1)
        assert 'Refusing compiled/original bytecode' in (output/'menu-runtime.log').read_text()
        checks.append('bytecode_refused')
        (scripts/'probe.lua').write_text('probeLoaded=true\n')
        (root/'assets/probe.dds').unlink()
        run('--root', root, '--output-dir', output, '--smoke', expected=1)
        assert 'Missing recovered asset' in (output/'menu-runtime.log').read_text()
        checks.append('missing_texture_failure')
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps({'platform': 'linux-x64', 'original_resources_executed': False,
                                     'fixture_origin': 'authored', 'checks': checks, 'passed': True}, indent=2)+'\n')
    print(f'{len(checks)} Linux menu transport checks passed; original resources not exercised')


if __name__ == '__main__':
    main()
