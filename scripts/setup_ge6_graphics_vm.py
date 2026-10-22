"""Assemble a PRIVATE Ubuntu toolchain/X11 initramfs for local GE6 experiments.

Downloads official Ubuntu packages into ignored build storage. No original game
input is sent to a build service. QEMU networking is restricted to loopback SSH.
Requires paramiko and zstandard in a separate local Python environment.
"""
import argparse
import gzip
import hashlib
import io
import json
import lzma
from pathlib import Path
import re
import struct
import tarfile
import urllib.request
import time
from concurrent.futures import ThreadPoolExecutor

from check_ge6_linux_vm import archive_entries

ROOTS = ('g++', 'make', 'libssl-dev', 'libpng-dev', 'libx11-dev', 'libxrandr-dev',
         'libxcursor-dev', 'libxi-dev', 'libgl-dev', 'libgl1-mesa-dri', 'libglx-mesa0',
         'xvfb', 'xserver-xorg-core', 'xserver-xorg-video-dummy',
         'xauth', 'x11-utils', 'x11-xserver-utils', 'xfonts-base', 'libxtst6',
         'openssh-server', 'python3', 'libpython3-stdlib', 'libatomic1')
BASE = 'https://archive.ubuntu.com/ubuntu/'


def download(url, path, digest=None):
    if path.exists() and (not digest or hashlib.sha256(path.read_bytes()).hexdigest() == digest):
        return path.read_bytes()
    path.parent.mkdir(parents=True, exist_ok=True)
    for attempt in range(4):
        try:
            with urllib.request.urlopen(url, timeout=300) as response:
                data = response.read()
            break
        except (TimeoutError, OSError):
            if attempt == 3:
                raise
            time.sleep(2 * (attempt + 1))
    if digest and hashlib.sha256(data).hexdigest() != digest:
        raise ValueError('Ubuntu package digest differs: ' + path.name)
    path.write_bytes(data)
    return data


def version_parts(value):
    epoch, sep, rest = value.partition(':')
    return (int(epoch) if sep else 0, rest if sep else epoch)


def compare_version(a, b):
    ea, a = version_parts(a); eb, b = version_parts(b)
    if ea != eb:
        return (ea > eb) - (ea < eb)
    # Debian non-digit ordering, including tilde-before-everything.
    order = lambda c: -1 if c == '~' else 0 if not c else ord(c) if c.isalpha() else ord(c) + 256
    while a or b:
        while (a and not a[0].isdigit()) or (b and not b[0].isdigit()):
            x = a[0] if a and not a[0].isdigit() else ''; y = b[0] if b and not b[0].isdigit() else ''
            if order(x) != order(y):
                return (order(x) > order(y)) - (order(x) < order(y))
            a = a[len(x):]; b = b[len(y):]
        da = re.match(r'\d*', a)[0]; db = re.match(r'\d*', b)[0]
        na, nb = int(da or '0'), int(db or '0')
        if na != nb:
            return (na > nb) - (na < nb)
        a = a[len(da):]; b = b[len(db):]
    return 0


def catalog(cache):
    packages = {}; provides = {}
    for suite in ('noble', 'noble-updates', 'noble-security'):
        for component in ('main', 'universe'):
            name = f'{suite}-{component}-amd64.xz'
            raw = download(BASE + f'dists/{suite}/{component}/binary-amd64/Packages.xz', cache / name)
            for block in lzma.decompress(raw).decode().split('\n\n'):
                item = {}; key = None
                for line in block.splitlines():
                    if line.startswith(' ') and key:
                        item[key] += ' ' + line.strip()
                    elif ': ' in line:
                        key, value = line.split(': ', 1); item[key] = value
                if 'Filename' not in item:
                    continue
                name = item['Package']
                if name not in packages or compare_version(item['Version'], packages[name]['Version']) > 0:
                    packages[name] = item
    for name, item in packages.items():
        for provided in item.get('Provides', '').split(','):
            virtual = provided.strip().split(' ')[0]
            if virtual:
                provides.setdefault(virtual, name)
    return packages, provides


def closure(packages, provides):
    selected = {}; pending = list(ROOTS)
    while pending:
        name = pending.pop()
        name = name if name in packages else provides.get(name, name)
        if name in selected:
            continue
        item = packages[name]; selected[name] = item
        for expression in (item.get('Pre-Depends', '') + ',' + item.get('Depends', '')).split(','):
            alternatives = [re.split(r'[ (:]', part.strip())[0] for part in expression.split('|') if part.strip()]
            if not alternatives:
                continue
            target = next((part for part in alternatives if part in selected), None)
            target = target or next((part for part in alternatives if part in packages or part in provides), None)
            if not target:
                raise ValueError('Unavailable Ubuntu dependency: ' + expression)
            pending.append(target)
    return selected


def deb_archive(raw):
    import zstandard
    if raw[:8] != b'!<arch>\n':
        raise ValueError('Invalid Debian ar archive')
    at = 8
    while at + 60 <= len(raw):
        header = raw[at:at + 60]; size = int(header[48:58]); name = header[:16].decode().strip().rstrip('/')
        data = raw[at + 60:at + 60 + size]; at += 60 + size + size % 2
        if name.startswith('data.tar'):
            if name.endswith('.zst'):
                return zstandard.ZstdDecompressor().stream_reader(io.BytesIO(data))
            return io.BytesIO(lzma.decompress(data) if name.endswith('.xz') else gzip.decompress(data) if name.endswith('.gz') else data)
    raise ValueError('Debian archive has no data payload')


def assemble(args):
    import paramiko
    args.output.mkdir(parents=True, exist_ok=True)
    packages, provides = catalog(args.output / 'indexes')
    selected = closure(packages, provides)
    def prefetch(item):
        download(BASE + item['Filename'], args.output / 'debs' / Path(item['Filename']).name, item['SHA256'])
    with ThreadPoolExecutor(max_workers=6) as pool:
        list(pool.map(prefetch, selected.values()))
    (args.output / 'ubuntu-packages.json').write_text(json.dumps({name: {k: item[k] for k in ('Version', 'Filename', 'SHA256')} for name, item in selected.items()}, indent=2) + '\n')
    entries = {}
    for i, (name, item) in enumerate(sorted(selected.items())):
        print(f'Ubuntu {i + 1}/{len(selected)} {name} {item["Version"]}', flush=True)
        raw = download(BASE + item['Filename'], args.output / 'debs' / Path(item['Filename']).name, item['SHA256'])
        with tarfile.open(fileobj=deb_archive(raw), mode='r|') as archive:
            for member in archive:
                path = member.name.removeprefix('./').lstrip('/')
                if not path or '..' in Path(path).parts:
                    continue
                if member.isdir():
                    entries[path] = (0o40000 | member.mode, b'')
                elif member.issym():
                    entries[path] = (0o120777, member.linkname.encode())
                elif member.islnk():
                    entries[path] = ('hardlink', member.linkname.removeprefix('./').lstrip('/'))
                elif member.isfile():
                    entries[path] = (0o100000 | member.mode, archive.extractfile(member).read())
    for path, (mode, data) in list(entries.items()):
        if mode == 'hardlink':
            entries[path] = entries[data]
    alpine = dict(archive_entries(args.alpine_initramfs.read_bytes()))
    entries['usr/bin/busybox'] = (0o100755, alpine['usr/bin/busybox'])
    entries['usr/lib/ld-musl-x86_64.so.1'] = (0o100755, alpine['usr/lib/ld-musl-x86_64.so.1'])
    for name, target in {'bin':'usr/bin','sbin':'usr/sbin','lib':'usr/lib','lib64':'usr/lib64',
                         'usr/bin/sh':'busybox','usr/bin/cc':'gcc','usr/bin/c++':'g++'}.items():
        entries[name] = (0o120777, target.encode())
    for keyname in ('client_rsa', 'host_rsa'):
        path = args.output / keyname
        if not path.exists():
            paramiko.RSAKey.generate(3072).write_private_key_file(str(path))
    client = paramiko.RSAKey.from_private_key_file(str(args.output / 'client_rsa'))
    host = paramiko.RSAKey.from_private_key_file(str(args.output / 'host_rsa'))
    entries['etc/ssh/ssh_host_rsa_key'] = (0o100600, (args.output / 'host_rsa').read_bytes())
    entries['root/.ssh/authorized_keys'] = (0o100600, (client.get_name()+' '+client.get_base64()+'\n').encode())
    entries['etc/passwd'] = (0o100644, b'root:x:0:0:root:/root:/bin/sh\nsshd:x:102:65534:sshd:/run/sshd:/usr/sbin/nologin\n')
    entries['etc/shadow'] = (0o100600, b'root::20000:0:99999:7:::\n')
    entries['etc/group'] = (0o100644, b'root:x:0:\nnogroup:x:65534:\n')
    entries['etc/ssh/sshd_config'] = (0o100600, b'Port 22\nHostKey /etc/ssh/ssh_host_rsa_key\nPermitRootLogin prohibit-password\nPasswordAuthentication no\nUsePAM no\nAuthorizedKeysFile /root/.ssh/authorized_keys\nSubsystem sftp /usr/lib/openssh/sftp-server\n')
    entries['etc/hosts'] = (0o100644, b'127.0.0.1 localhost\n')
    entries['etc/nsswitch.conf'] = (0o100644, b'passwd: files\ngroup: files\nhosts: files\n')
    entries['work/libopenal.a'] = (0o100644, args.openal_archive.read_bytes())
    entries['work/xorg-dummy.conf'] = (0o100644, (Path(__file__).parent/'ge6_xorg_dummy.conf').read_bytes())
    for path in args.openal_include.glob('*.h'):
        entries['work/include/AL/'+path.name] = (0o100644, path.read_bytes())
    entries['init'] = (0o100755, b'''#!/bin/sh
/usr/bin/busybox mount -t proc proc /proc
/usr/bin/busybox mount -t sysfs sysfs /sys
/usr/bin/busybox mount -t devtmpfs devtmpfs /dev
# The kernel's 50% rootfs default cannot hold the private world plus tools.
# This is a storage ceiling, not a reservation; leave RAM for the native engine.
/usr/bin/busybox mount -o remount,size=75% /
/usr/bin/busybox mkdir -p /dev/shm /run/sshd /tmp/.X11-unix
/usr/bin/busybox mkdir -p /usr/local/bin
/usr/bin/busybox --install -s /usr/local/bin
/usr/bin/busybox mount -t tmpfs tmpfs /dev/shm
/usr/bin/busybox ip link set eth0 up
/usr/bin/busybox ip addr add 10.0.2.15/24 dev eth0
/usr/bin/busybox ip route add default via 10.0.2.2
export DISPLAY=:98 LIBGL_ALWAYS_SOFTWARE=1 ALSOFT_DRIVERS=null
/usr/bin/Xvfb :99 -screen 0 1280x720x24 -nolisten tcp -ac -noreset >/work/xvfb.log 2>&1 &
/usr/lib/xorg/Xorg :98 -config /work/xorg-dummy.conf -logfile /work/Xorg.98.log -nolisten tcp -ac -noreset -novtswitch -sharevts >/work/xorg-start.log 2>&1 &
/usr/sbin/sshd -E /work/sshd.log
/usr/bin/busybox cat /work/sshd.log
/usr/bin/busybox cat /work/xvfb.log
/usr/bin/busybox ip addr
/usr/bin/busybox ps
echo GE6_GRAPHICS_VM_READY
/usr/bin/busybox uname -a
while true; do /usr/bin/busybox sleep 60; done
''')
    for directory in ('proc','sys','dev','run','run/sshd','tmp','work','root','root/.ssh','usr/lib64'):
        entries.setdefault(directory, (0o40755, b''))
    entries['tmp'] = (0o41777, b''); entries['root/.ssh'] = (0o40700, b'')
    # Alpine virtio networking is modular: preserve its matching module tree.
    for name, data in alpine.items():
        if data and name.startswith(('lib/modules/', 'usr/lib/modules/')):
            entries[name if name.startswith('usr/') else 'usr/'+name] = (0o100644, data)
    entries['init'] = (0o100755, entries['init'][1].replace(b'/usr/bin/busybox ip link', b'/usr/bin/busybox modprobe virtio_net\n/usr/bin/busybox ip link', 1))
    # Ensure parent directories exist, but do not replace explicit merged-/usr links.
    for path in list(entries):
        for parent in Path(path).parents:
            if str(parent) != '.':
                entries.setdefault(parent.as_posix(), (0o40755, b''))
    output = args.output / 'ubuntu-graphics-initramfs.gz'
    with gzip.open(output, 'wb', compresslevel=1) as stream:
        for inode, (name, (mode, data)) in enumerate(sorted(entries.items(), key=lambda item:(item[0].count('/'),item[0])), 1):
            encoded=name.encode()+b'\0';fields=[inode,mode,0,0,1,0,len(data),0,0,0,0,len(encoded),0]
            header=b'070701'+''.join(f'{x:08x}' for x in fields).encode()+encoded
            stream.write(header);stream.write(b'\0'*(-len(header)%4));stream.write(data);stream.write(b'\0'*(-len(data)%4))
        encoded=b'TRAILER!!!\0';fields=[len(entries)+1,0,0,0,1,0,0,0,0,0,0,len(encoded),0]
        header=b'070701'+''.join(f'{x:08x}' for x in fields).encode()+encoded;stream.write(header);stream.write(b'\0'*(-len(header)%4))
    print(f'PRIVATE Linux graphics/toolchain initramfs: {output} ({output.stat().st_size} bytes)',flush=True)
    (args.output / 'host-key.txt').write_text(host.get_name()+' '+host.get_base64()+'\n')


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--alpine-initramfs',type=Path,required=True)
    parser.add_argument('--openal-archive',type=Path,required=True)
    parser.add_argument('--openal-include',type=Path,required=True)
    assemble(parser.parse_args())
