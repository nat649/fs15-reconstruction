"""Control the private, loopback-only GE6 Linux graphics VM.

No private payload is uploaded to GitHub. SSH host identity is pinned to the
local setup-generated key. The VM uses official Ubuntu tools and Xvfb.
"""
import argparse
import base64
import hashlib
import json
from pathlib import Path, PurePosixPath
import shlex
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]


def connect(directory, port):
    import paramiko
    host = (directory / 'host-key.txt').read_text().split()
    client = paramiko.SSHClient()
    client.get_host_keys().add(f'[127.0.0.1]:{port}', host[0], paramiko.RSAKey(data=base64.b64decode(host[1])))
    client.connect('127.0.0.1', port=port, username='root', pkey=paramiko.RSAKey.from_private_key_file(str(directory / 'client_rsa')),
                   allow_agent=False, look_for_keys=False, timeout=8, banner_timeout=15)
    return client


def command(client, text, timeout=180):
    stdin, stdout, stderr = client.exec_command(text, timeout=timeout)
    stdin.close()
    # Merge stderr at the channel so a long compiler diagnostic cannot deadlock.
    stdout.channel.set_combine_stderr(True)
    result = stdout.read().decode(errors='replace')
    code = stdout.channel.recv_exit_status()
    print(result, end='', flush=True)
    return code, result


def upload(client, source, target):
    with client.open_sftp() as sftp:
        for parent in reversed(PurePosixPath(target).parents):
            if str(parent) != '/':
                try:
                    sftp.mkdir(str(parent))
                except OSError:
                    pass
        sftp.put(str(source), target)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action',choices=('start','status','compile','deploy','stage','run','capture','exec','stop'))
    parser.add_argument('--directory',type=Path,default=ROOT/'build/ge6-graphics-vm')
    parser.add_argument('--port',type=int,default=22222)
    parser.add_argument('--qemu',type=Path)
    parser.add_argument('--kernel',type=Path,default=ROOT/'build/linux-play-vm/boot/vmlinuz-virt')
    parser.add_argument('--resources',type=Path,default=ROOT/'build/ge6-local')
    parser.add_argument('--command')
    parser.add_argument('--exe',type=Path)
    args=parser.parse_args();directory=args.directory
    if args.action=='start':
        if not args.qemu:
            parser.error('start requires --qemu')
        argv=[str(args.qemu),'-machine','q35','-accel','tcg,thread=multi','-cpu','max','-smp','2','-m','6144',
              '-kernel',str(args.kernel),'-initrd',str(directory/'ubuntu-graphics-initramfs.gz'),
              '-append','console=ttyS0 rdinit=/init panic=-1 quiet',
              '-display','none','-serial','file:'+str(directory/'serial.log'),'-monitor','none',
              '-netdev',f'user,id=local,restrict=on,hostfwd=tcp:127.0.0.1:{args.port}-:22',
              '-device','virtio-net-pci,netdev=local','-no-reboot']
        flags=getattr(subprocess,'CREATE_NO_WINDOW',0)
        with (directory/'qemu.log').open('wb') as log:
            process=subprocess.Popen(argv,stdout=log,stderr=subprocess.STDOUT,creationflags=flags)
        (directory/'process.json').write_text(json.dumps(dict(pid=process.pid,port=args.port)))
        print(f'Private graphics VM process {process.pid}; SSH restricted to 127.0.0.1:{args.port}')
        return
    with connect(directory,args.port) as client:
        if args.action=='status':
            code,_=command(client,'/usr/bin/busybox uname -a; DISPLAY=:99 xdpyinfo | head -25; g++ --version | head -1')
        elif args.action=='compile':
            for source in (ROOT/'src/ge6_linux').iterdir():
                if source.suffix in ('.cpp','.h'):
                    upload(client,source,'/work/src/'+source.name)
            code,text=command(client,'cd /work/src && g++ -std=c++17 -O0 -Wall -Wextra -Wpedantic -Werror -maccumulate-outgoing-args -DGE6_X11=1 -I/work/include main.cpp /work/libopenal.a -lcrypto -lpng -lX11 -lXrandr -lXcursor -lXi -lGL -latomic -ldl -lpthread -lm -o /work/FS15GE6LinuxBoot',timeout=600)
            (directory/'compile.log').write_text(text)
        elif args.action=='deploy':
            if not args.exe:
                parser.error('deploy requires --exe')
            upload(client,args.exe,'/work/FS15GE6LinuxBoot')
            code,_=command(client,'chmod 755 /work/FS15GE6LinuxBoot; sha256sum /work/FS15GE6LinuxBoot')
        elif args.action=='stage':
            names=('x64/FarmingSimulator2015Game.exe','game.xml','FarmingSimulator2015.xml','VERSION','steamdata.dat',
                   'dataS.gar','dataS2.gar','crc_files.xml','data/maps/map01.i3d','data/maps/map01/map01_dem.png')
            for name in names:
                print('Private VM input: '+name,flush=True)
                upload(client,args.resources/name,'/game/'+name)
            manifest={name:dict(sha256=hashlib.sha256((args.resources/name).read_bytes()).hexdigest(),size=(args.resources/name).stat().st_size) for name in names}
            (directory/'staged-inputs.json').write_text(json.dumps(manifest,indent=2)+'\n')
            code,_=command(client,'/usr/bin/busybox mkdir -p /profile')
        elif args.action=='run':
            code,_=command(client,"cd /game && { uname -a; sha256sum /work/FS15GE6LinuxBoot; echo GE6_GRAPHICS_ENV=X11 DISPLAY=:99 LIBGL_ALWAYS_SOFTWARE=1 ALSOFT_DRIVERS=null; } > /work/boot.log; rm -f /work/ge6-native-screen.png; cd /game && DISPLAY=:99 LIBGL_ALWAYS_SOFTWARE=1 ALSOFT_DRIVERS=null GE6_X11_CAPTURE=/work/ge6-native-screen.png /usr/bin/busybox timeout 90 /work/FS15GE6LinuxBoot --image /game/x64/FarmingSimulator2015Game.exe --command-line 'FarmingSimulator2015Game.exe -profile Z:/profile' >> /work/boot.log 2>&1; result=$?; echo GE6_GRAPHICS_RESULT=$result >> /work/boot.log; tail -35 /work/boot.log; cat /profile/log.txt",timeout=150)
            with client.open_sftp() as sftp:
                sftp.get('/work/boot.log',str(directory/'boot.log'))
                try:
                    sftp.get('/work/ge6-native-screen.png',str(directory/'ge6-native-screen.png'))
                except OSError:
                    pass
                try:
                    sftp.get('/profile/log.txt',str(directory/'original-log.txt'))
                except OSError:
                    pass
        elif args.action=='capture':
            code,_=command(client,'DISPLAY=:99 xwininfo -root -tree; DISPLAY=:99 xwd -root -silent -out /work/screen.xwd')
            with client.open_sftp() as sftp:
                sftp.get('/work/screen.xwd',str(directory/'screen.xwd'))
        elif args.action=='exec':
            if not args.command:
                parser.error('exec requires --command')
            code,_=command(client,args.command,timeout=300)
        else:
            client.exec_command('/usr/bin/busybox poweroff -f');return
        if code:
            raise SystemExit(code)


if __name__=='__main__':
    main()
