"""Run private original GE6 boot diagnostics in an isolated local Linux/QEMU VM.

Uses an already available Alpine kernel/initramfs and the Linux diagnostic ELF.
The generated initramfs embeds the user's pinned original image: keep it local.
No network device is attached; no original payload is uploaded anywhere.
"""
import argparse
import gzip
import hashlib
from pathlib import Path
import subprocess
from native_gar import DEFAULT_IMAGE
from ge6_linux_images import image_kind


def archive_entries(raw):
    data = gzip.decompress(raw)
    offset = 0
    while data[offset:offset + 6] == b'070701':
        fields = [int(data[offset + 6 + j * 8:offset + 14 + j * 8], 16) for j in range(13)]
        name = data[offset + 110:offset + 110 + fields[11] - 1].decode()
        start = (offset + 110 + fields[11] + 3) & ~3
        content = data[start:start + fields[6]]
        offset = (start + fields[6] + 3) & ~3
        if name == 'TRAILER!!!':
            break
        yield name.lstrip('./'), content


def build_initramfs(alpine, executable, image, resources=None, archives=False):
    original = image.read_bytes()
    kind = image_kind(original)
    source = dict(archive_entries(alpine.read_bytes()))
    busybox = source['usr/bin/busybox']
    libc = source['usr/lib/ld-musl-x86_64.so.1']
    out = bytearray()
    inode = 0
    def add(name, data=b'', mode=0o100644, device=(0, 0)):
        nonlocal inode
        inode += 1
        encoded = name.encode() + b'\0'
        fields = [inode, mode, 0, 0, 1, 0, len(data), 0, 0, *device, len(encoded), 0]
        out.extend(b'070701' + ''.join(f'{x:08x}' for x in fields).encode() + encoded)
        out.extend(b'\0' * (-len(out) % 4))
        out.extend(data)
        out.extend(b'\0' * (-len(out) % 4))
    for name in ('usr', 'usr/bin', 'usr/lib', 'dev', 'proc', 'tmp', 'game', 'game/x64', 'profile'):
        add(name, mode=0o40755)
    add('bin', b'usr/bin', 0o120777)
    add('lib', b'usr/lib', 0o120777)
    add('usr/bin/sh', b'busybox', 0o120777)
    add('usr/bin/busybox', busybox, 0o100755)
    add('usr/lib/ld-musl-x86_64.so.1', libc, 0o100755)
    add('dev/console', mode=0o20600, device=(5, 1))
    add('dev/null', mode=0o20666, device=(1, 3))
    add('dev/urandom', mode=0o20666, device=(1, 9))
    add('ge6-boot', executable.read_bytes(), 0o100755)
    add('game/x64/FarmingSimulator2015Game.exe', original)
    if resources is not None:
        names = ('game.xml', 'FarmingSimulator2015.xml', 'VERSION', 'steamdata.dat')
        if archives:
            names += ('dataS.gar', 'dataS2.gar')
        for name in names:
            source_file = resources / name
            if source_file.is_symlink() or not source_file.is_file():
                raise ValueError('Missing regular startup resource: ' + name)
            add('game/' + name, source_file.read_bytes())
    init = b'''#!/bin/sh
/usr/bin/busybox mount -t proc proc /proc
echo GE6_LINUX_VM_STARTED
/usr/bin/busybox uname -a
cd /game
/ge6-boot --image /game/x64/FarmingSimulator2015Game.exe --command-line 'FarmingSimulator2015Game.exe -profile Z:/profile'
result=$?
echo GE6_LINUX_VM_RESULT=$result
if [ -f /profile/log.txt ]; then
    echo GE6_ORIGINAL_LOG_BEGIN
    /usr/bin/busybox cat /profile/log.txt
    echo GE6_ORIGINAL_LOG_END
fi
/usr/bin/busybox poweroff -f
'''
    init = init.replace(b'echo GE6_LINUX_VM_STARTED', b'echo GE6_LINUX_VM_STARTED\necho GE6_VM_IMAGE_KIND=' + kind.encode())
    add('init', init, 0o100755)
    add('TRAILER!!!')
    return gzip.compress(out, mtime=0)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qemu', required=True, type=Path)
    parser.add_argument('--kernel', required=True, type=Path)
    parser.add_argument('--alpine-initramfs', required=True, type=Path)
    parser.add_argument('--exe', required=True, type=Path)
    parser.add_argument('--image', type=Path, default=DEFAULT_IMAGE)
    parser.add_argument('--resources', type=Path, help='Optional local GE6 directory; stages only named startup files')
    parser.add_argument('--archives', action='store_true', help='Also stage the two original GAR archives locally')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    initramfs = args.output / 'private-ge6-initramfs.gz'
    initramfs.write_bytes(build_initramfs(args.alpine_initramfs, args.exe, args.image, args.resources, args.archives))
    command = [str(args.qemu), '-machine', 'q35', '-accel', 'tcg', '-m', '768',
               '-kernel', str(args.kernel), '-initrd', str(initramfs),
               '-append', 'console=ttyS0 rdinit=/init panic=-1 quiet',
               '-nographic', '-monitor', 'none', '-nic', 'none', '-no-reboot']
    result = subprocess.run(command, capture_output=True, timeout=60)
    log = result.stdout + result.stderr
    (args.output / 'serial.log').write_bytes(log)
    text = log.decode(errors='replace')
    print('\n'.join(line for line in text.splitlines() if 'GE6_' in line or line.startswith('Linux ')))
    if 'GE6_LINUX_VM_RESULT=' not in text:
        raise RuntimeError('Linux diagnostic did not finish; inspect the private serial log')


if __name__ == '__main__':
    main()
