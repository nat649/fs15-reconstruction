"""Actual GE6 sky/injection/shadow GPU comparisons, with stdlib PNG decoding."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import zlib


def png_rgb(path):
    source = path.read_bytes()
    assert source[:8] == b'\x89PNG\r\n\x1a\n'
    at, compressed = 8, bytearray()
    while at < len(source):
        length, = struct.unpack_from('>I',source,at)
        kind, data = source[at+4:at+8], source[at+8:at+8+length]
        if kind == b'IHDR':
            width,height,bits,color,compression,filtering,interlace = struct.unpack('>IIBBBBB',data)
            assert bits == 8 and color in (2,6) and compression == filtering == interlace == 0
        if kind == b'IDAT':
            compressed.extend(data)
        at += length+12
    bpp = 3 if color == 2 else 4
    decoded = zlib.decompress(compressed)
    stride, at, previous, pixels = width*bpp, 0, bytearray(width*bpp), bytearray()
    assert len(decoded) == height*(stride+1)
    for _ in range(height):
        method = decoded[at]
        row = bytearray(decoded[at+1:at+1+stride]);at += stride+1
        for i in range(stride):
            left = row[i-bpp] if i >= bpp else 0
            up = previous[i]
            corner = previous[i-bpp] if i >= bpp else 0
            if method == 0: predictor = 0
            elif method == 1: predictor = left
            elif method == 2: predictor = up
            elif method == 3: predictor = (left+up)//2
            elif method == 4:
                p = left+up-corner
                a,b,c = abs(p-left),abs(p-up),abs(p-corner)
                predictor = left if a <= b and a <= c else up if b <= c else corner
            else: raise AssertionError('Unsupported PNG row filter')
            row[i] = (row[i]+predictor)&255
        if bpp == 3: pixels.extend(row)
        else:
            for i in range(0,stride,4):pixels.extend(row[i:i+3])
        previous = row
    return width,height,pixels


def changed(first,second):
    assert first[:2] == second[:2]
    a,b=first[2],second[2]
    return sum(max(abs(a[i]-b[i]),abs(a[i+1]-b[i+1]),abs(a[i+2]-b[i+2]))>2 for i in range(0,len(a),3))


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--exe',type=Path,required=True)
    parser.add_argument('--root',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    args.output.mkdir(parents=True,exist_ok=True)
    reports,images={},{}
    close=['--camera','230','110','245','--look-at','262','105','255']
    variants={'day':[],'motion':['--time','1'],'night':['--hour','22'],
              'shadow':close,'unshadowed':close+['--no-shadows']}
    for name,extra in variants.items():
        report,image=args.output/(name+'.json'),args.output/(name+'.png')
        subprocess.run([str(args.exe),'--root',str(args.root),'--map','Map02','--smoke',
                        '--report',str(report),'--capture',str(image)]+extra,check=True,timeout=60)
        data=json.loads(report.read_text())
        assert data['ready'] and data['environment_ready'] and data['original_shader_injections']==5
        assert data['animated_draws']>0 and data['visible_object_gpu_samples']>0
        assert data['shadow_depth_pixels']>0 if name!='unshadowed' else data['shadow_depth_pixels']==0
        reports[name]=data;images[name]=png_rgb(image)
    motion=changed(images['day'],images['motion'])
    night=changed(images['day'],images['night'])
    shadow=changed(images['shadow'],images['unshadowed'])
    assert motion>100, f'Original shader animation not visible: {motion}'
    assert shadow>10000, f'Shadow depth is populated but casting is not visible: {shadow}'
    assert night>images['day'][0]*images['day'][1]//2
    assert sum(images['night'][2]) < sum(images['day'][2])*0.4
    summary=dict(scope='Original sky mesh/textures, recovered Lua curves and five compiled GE6 shader injections; authored single-map shadow host',
                 status='passed',map='Map02',shaderInjections=5,originalCurveSamples=1441,
                 animatedDraws=reports['day']['animated_draws'],motionChangedPixels=motion,
                 nightChangedPixels=night,shadowChangedPixels=shadow,
                 shadowDepthPixels=reports['shadow']['shadow_depth_pixels'],shadowResolution=reports['shadow']['shadow_resolution'],
                 dayImageSha256=hashlib.sha256((args.output/'day.png').read_bytes()).hexdigest(),
                 nightImageSha256=hashlib.sha256((args.output/'night.png').read_bytes()).hexdigest())
    (args.root/'analysis/environment-validation.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(json.dumps(summary,indent=2))


if __name__=='__main__':
    main()
