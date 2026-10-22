"""Control the private, loopback-only GE6 Linux graphics VM.

No private payload is uploaded to GitHub. SSH host identity is pinned to the
local setup-generated key. The VM uses official Ubuntu tools and X11 servers.
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


def upload(client, source, target, sftp=None):
    if sftp is None:
        with client.open_sftp() as transfer:
            return upload(client,source,target,transfer)
    for parent in reversed(PurePosixPath(target).parents):
        if str(parent) != '/':
            try:
                sftp.mkdir(str(parent))
            except OSError:
                pass
    temporary=target+'.ge6-upload-tmp'
    try:
        sftp.put(str(source), temporary)
        metadata=source.stat()
        sftp.utime(temporary,(int(metadata.st_atime),int(metadata.st_mtime)))
        # Publish only complete files, including when a previous run is still
        # mapped. A failed/full-filesystem transfer preserves the old target.
        sftp.posix_rename(temporary,target)
    except BaseException:
        try:
            sftp.remove(temporary)
        except OSError:
            pass
        raise


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action',choices=('start','status','compile','deploy','stage','run','capture','exec','stop'))
    parser.add_argument('--directory',type=Path,default=ROOT/'build/ge6-graphics-vm')
    parser.add_argument('--port',type=int,default=22222)
    parser.add_argument('--qemu',type=Path)
    parser.add_argument('--kernel',type=Path,default=ROOT/'build/linux-play-vm/boot/vmlinuz-virt')
    parser.add_argument('--resources',type=Path,default=ROOT/'build/ge6-local')
    parser.add_argument('--resource-set',choices=('startup','world'),default='startup',
                        help='Private startup inputs, or all available original data/shared/profileTemplate files for map loading')
    parser.add_argument('--command')
    parser.add_argument('--exe',type=Path)
    parser.add_argument('--display',default=':98',help='Xorg dummy at 60 Hz; :99 selects the separate Xvfb zero-Hz diagnostic')
    parser.add_argument('--timeout',type=int,default=90,help='Bounded original-code run duration in seconds')
    parser.add_argument('--capture-name',default='ge6-native-observation.png')
    args=parser.parse_args();directory=args.directory
    if args.timeout<1 or Path(args.capture_name).name!=args.capture_name or not args.capture_name.endswith('.png'):
        parser.error('Timeout must be positive and capture name must be a PNG basename')
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
            code,_=command(client,f'/usr/bin/busybox uname -a; DISPLAY={shlex.quote(args.display)} xdpyinfo | head -25; g++ --version | head -1')
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
            snapshot=args.exe.with_name('FS15GE6X11Snapshot')
            if snapshot.is_file():
                upload(client,snapshot,'/work/FS15GE6X11Snapshot')
                command(client,'chmod 755 /work/FS15GE6X11Snapshot')
            code,_=command(client,'chmod 755 /work/FS15GE6LinuxBoot; sha256sum /work/FS15GE6LinuxBoot')
        elif args.action=='stage':
            names=('x64/FarmingSimulator2015Game.exe','game.xml','FarmingSimulator2015.xml','VERSION','steamdata.dat',
                   'dataS.gar','dataS2.gar','crc_files.xml','data/maps/map01.i3d','data/maps/map01/map01_dem.png',
                   'shared/shader.gar','shared/splash.dds','shared/font/defaultFont.xml',
                   'shared/font/defaultFont.font','shared/font/defaultFont.png',
                   'profileTemplate/gameSettingsTemplate.xml','profileTemplate/inputBindingDefault.xml',
                   'data/maps/sounds/achievementSound.wav',
                   'data/vehicles/store/store_caseIHMagnum380.dds','data/vehicles/store/brand_caseIH.dds',
                   'data/vehicles/store/store_empty.dds','data/maps/map01/map_preview.dds')
            if args.resource_set=='world':
                names=tuple(sorted(set(names)|{path.relative_to(args.resources).as_posix()
                            for folder in ('data','shared','profileTemplate')
                            for path in (args.resources/folder).rglob('*') if path.is_file()}))
            missing=[name for name in names if not (args.resources/name).is_file()]
            if missing:
                parser.error('Missing private startup inputs: '+', '.join(missing))
            with client.open_sftp() as transfer:
                for index,name in enumerate(names,1):
                    if len(names)<50 or index==1 or index%100==0 or index==len(names):
                        print(f'Private VM input {index}/{len(names)}: '+name,flush=True)
                    upload(client,args.resources/name,'/game/'+name,transfer)
            manifest={name:dict(sha256=hashlib.sha256((args.resources/name).read_bytes()).hexdigest(),size=(args.resources/name).stat().st_size) for name in names}
            (directory/'staged-inputs.json').write_text(json.dumps(manifest,indent=2)+'\n')
            code,_=command(client,'/usr/bin/busybox mkdir -p /profile')
        elif args.action=='run':
            for name in ('boot.log','original-log.txt','ge6-native-screen.png','ge6-native-final-screen.png'):
                (directory/name).unlink(missing_ok=True)
            upload(client,ROOT/'scripts/ge6_vm_run.py','/work/ge6_vm_run.py')
            upload(client,ROOT/'scripts/ge6_vm_pointer.py','/work/ge6_vm_pointer.py')
            display=shlex.quote(args.display)
            code,_=command(client,f"cd /game && {{ uname -a; sha256sum /work/FS15GE6LinuxBoot; echo GE6_GRAPHICS_ENV=X11 DISPLAY={display} LIBGL_ALWAYS_SOFTWARE=1 ALSOFT_DRIVERS=null; DISPLAY={display} xrandr --current; }} > /work/boot.log; rm -f /work/ge6-native-screen.png /work/ge6-native-final-screen.png; cd /game && DISPLAY={display} LIBGL_ALWAYS_SOFTWARE=1 ALSOFT_DRIVERS=null GE6_X11_CAPTURE=/work/ge6-native-screen.png /usr/bin/python3 /work/ge6_vm_run.py --timeout {args.timeout} /work/FS15GE6LinuxBoot --image /game/x64/FarmingSimulator2015Game.exe --command-line 'FarmingSimulator2015Game.exe -profile Z:/profile' >> /work/boot.log 2>&1; tail -35 /work/boot.log; cat /profile/log.txt",timeout=args.timeout+60)
            with client.open_sftp() as sftp:
                sftp.get('/work/boot.log',str(directory/'boot.log'))
                try:
                    sftp.get('/work/ge6-native-final-screen.png',str(directory/'ge6-native-final-screen.png'))
                except OSError:
                    pass
                try:
                    sftp.get('/work/ge6-native-screen.png',str(directory/'ge6-native-screen.png'))
                except OSError:
                    pass
                try:
                    sftp.get('/profile/log.txt',str(directory/'original-log.txt'))
                except OSError:
                    pass
        elif args.action=='capture':
            output='/work/'+args.capture_name;display=shlex.quote(args.display)
            code,log=command(client,f'DISPLAY={display} xwininfo -root -tree; DISPLAY={display} /work/FS15GE6X11Snapshot {shlex.quote(output)}')
            with client.open_sftp() as sftp:
                sftp.get(output,str(directory/args.capture_name))
            (directory/(args.capture_name+'.log')).write_text(log)
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
