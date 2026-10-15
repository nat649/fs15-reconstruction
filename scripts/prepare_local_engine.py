"""Prepare an experimental original-GE6 runtime using a local platform adapter.

Requires the user's pinned native analysis image and installed game. Original
code/resources stay local. This is a modified original engine, not recovered C++
GE6. Steam services and licensed DLC are unavailable through the adapter.
"""
import argparse
from contextlib import contextmanager
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
from inventory import PE, ROOT
from launcher_assets import checked_output, copy_verified, preflight_outputs, preparation_lock, emit
from native_gar import EXPECTED_IMAGE
from prepare_native_engine import prepare, configure_launch

EXPECTED_GAME='21ce6fa0f776ad69f5a707a8c391cba6d2b80bdb612ffdf47f109372478b5488'
LOCAL_IMPORT=b'fs15_local.dll\0'
EXPORTS={'SteamUtils','SteamAPI_RegisterCallback','SteamUserStats','SteamAPI_WriteMiniDump',
         'SteamAPI_SetMiniDumpComment','SteamAPI_Shutdown','SteamClient','SteamAPI_UnregisterCallback',
         'SteamAPI_Init','SteamApps','SteamAPI_RestartAppIfNecessary','SteamAPI_RunCallbacks','SteamUser'}


def sha(path):
    with Path(path).open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()


def local_image(raw,reference_sha256=EXPECTED_IMAGE):
    if hashlib.sha256(raw).hexdigest()!=reference_sha256:
        raise ValueError('Local GE6 needs the pinned FS15 1.4.2.0 x64 analysis image')
    pe=PE(raw)
    if pe.bits!=64 or pe.base!=0x140000000 or pe.entry_rva!=0x1694310 or pe.directories[9]!=(0,0):
        raise ValueError('Unexpected native bootstrap/ABI/TLS layout')
    steam=[row for row in pe.imports() if row['dll'].lower()=='steam_api64.dll']
    if len(steam)!=1 or not steam[0]['functions'] or any(row['name'] not in EXPORTS for row in steam[0]['functions']):
        raise ValueError('Unexpected Steam import interface')
    expected=bytes.fromhex('48896c2418');offset=pe.offset(0xc810)
    if raw[offset:offset+len(expected)]!=expected:
        raise ValueError('Native platform bootstrap signature differs')
    # Confirm the recovered CRT wrapper is file-backed before selecting it.
    pe.offset(0x3fc96c)
    name=b'steam_api64.dll\0'
    if raw.count(name)!=1:raise ValueError('Native platform DLL name is ambiguous')
    out=bytearray(raw)
    optional=struct.unpack_from('<I',out,0x3c)[0]+24
    struct.pack_into('<I',out,optional+16,0x3fc96c)
    # Retire the Steam-only startup wrapper in the local copy. Other native
    # imports resolve to the bounded authored adapter, not the installed SDK DLL.
    out[offset]=0xc3
    name_offset=raw.index(name)
    out[name_offset:name_offset+len(name)]=LOCAL_IMPORT.ljust(len(name),b'\0')
    return bytes(out)


def adapter(root):
    root=Path(root)
    source=root/'src/local_platform'
    build=checked_output(root,'build/local-platform')
    dll=checked_output(root,'build/local-platform/Release/fs15_local.dll')
    stamp=checked_output(root,'build/local-platform/source.json')
    pins={name:sha(source/name) for name in ('CMakeLists.txt','local_platform.cpp')}
    try:current=json.loads(stamp.read_text())
    except (OSError,ValueError):current={}
    if not dll.is_file() or current.get('sources')!=pins or current.get('dll_sha256')!=sha(dll):
        subprocess.run(['cmake','-S',str(source),'-B',str(build),'-A','x64'],check=True)
        subprocess.run(['cmake','--build',str(build),'--config','Release'],check=True)
        stamp.write_text(json.dumps(dict(sources=pins,dll_sha256=sha(dll)),indent=2)+'\n')
    return dll


@contextmanager
def local_state(root):
    path=checked_output(root,'build/ge6-local/preparation.json');path.parent.mkdir(parents=True,exist_ok=True)
    path.write_text('{"status":"preparing"}\n')
    try:yield
    except BaseException as error:
        path.write_text(json.dumps(dict(status='failed',error=str(error)))+'\n');raise
    else:path.write_text('{"status":"complete"}\n')


def prepare_local(root,game,activate_recovered=False,dry_run=False):
    root,game=Path(root).resolve(),Path(game).resolve(strict=True)
    if sha(game/'x64/FarmingSimulator2015Game.exe')!=EXPECTED_GAME:
        raise ValueError('Local GE6 currently supports only the pinned installed FS15 1.4.2.0 x64 executable')
    base=checked_output(root,'build/ge6-local')
    image=root/'inputs/runtime/FarmingSimulator2015Game.runtime.bin'
    rebuilt=local_image(image.read_bytes())
    exe=checked_output(root,'build/ge6-local/x64/FarmingSimulator2015Game.exe')
    if dry_run:
        if json.loads((base/'preparation.json').read_text()).get('status')!='complete':
            raise ValueError('Complete or repair local GE6 preparation before checking its launch plan')
        if not exe.is_file() or sha(exe)!=hashlib.sha256(rebuilt).hexdigest() or not (base/'x64/fs15_local.dll').is_file():
            raise ValueError('Prepare the local GE6 runtime first: scripts/prepare_local_engine.py --game-dir PATH')
        manifest=json.loads((base/'manifest.json').read_text())
        manifest['command']=[str(root/'FS15LauncherRebuilt.exe'),'--game-dir',str(base),'--arch','x64','--','-profile',manifest['profile']]
        return configure_launch(manifest,direct=True,dry_run=True)
    with preparation_lock(root),local_state(root):
        preflight_outputs(root)
        # Check every generated runtime path, including existing child junctions.
        if base.exists():
            for path in base.rglob('*'):checked_output(root,path.relative_to(root))
        emit('progress',percent=5,title='Building the local GE6 platform adapter')
        dll=adapter(root)
        emit('progress',percent=15,title='Preparing the isolated original GE6 copy')
        exe.parent.mkdir(parents=True,exist_ok=True);exe.write_bytes(rebuilt)
        index_path=checked_output(root,'build/ge6-local/asset-index.json')
        try:previous=json.loads(index_path.read_text())
        except (OSError,ValueError):previous={}
        index={}
        def metadata(path):
            stat=path.stat();return [stat.st_size,stat.st_mtime_ns]
        def copy_asset(source,target):
            key=target.relative_to(base).as_posix()
            state=dict(source=metadata(source),target=metadata(target) if target.is_file() else None)
            updated=False
            if previous.get(key)!=state:
                updated=copy_verified(source,target);state['target']=metadata(target)
            index[key]=state;return updated
        for name in ('OpenAL32.dll','wrap_oal.dll'):copy_verified(game/'x64'/name,checked_output(root,'build/ge6-local/x64/'+name))
        copy_verified(dll,checked_output(root,'build/ge6-local/x64/fs15_local.dll'))
        for name in ('FarmingSimulator2015.xml','game.xml','VERSION','steamdata.dat','dataS.gar','dataS2.gar'):
            copy_asset(game/name,checked_output(root,'build/ge6-local/'+name))
        copied=0
        for directory in ('data','profileTemplate','shared','pdlc'):
            source=game/directory
            if not source.is_dir():continue
            for path in source.rglob('*'):
                if path.is_symlink() or (hasattr(path,'is_junction') and path.is_junction()):raise ValueError('Linked installed resource: '+str(path))
                if not path.is_file():continue
                if directory=='pdlc' and path.suffix.lower()=='.dlc':continue
                target=checked_output(root,'build/ge6-local/'+path.relative_to(game).as_posix())
                # All copied data remains separate from the installed game.
                updated=copy_asset(path,target);copied+=int(updated)
                if updated and copied%150==0:emit('progress',percent=55,title=f'Copying local GE6 resources ({copied} updated files)')
        if any((base/'pdlc').glob('*.dlc')) or any((base/'profile/pdlc').glob('*.dlc')):
            raise ValueError('The local GE6 adapter does not support licensed DLC; keep .dlc files outside this isolated runtime')
        manifest=prepare(root,game,activate_recovered,runtime_name='ge6-local')
        manifest['command']=[str(root/'FS15LauncherRebuilt.exe'),'--game-dir',str(base),'--arch','x64','--','-profile',manifest['profile']]
        manifest.update(runtime_transport='local',local_executable_sha256=sha(exe),local_adapter_sha256=sha(base/'x64/fs15_local.dll'),
            local_platform_scope='Authored offline service adapter; no Steam login, user identity, ownership, achievements or licensed DLC services',
            original_image_sha256=EXPECTED_IMAGE,original_installation_modified=False,full_native_rebuild_verified=False)
        manifest=configure_launch(manifest,direct=True)
        (base/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
        index_path.write_text(json.dumps(index,indent=2)+'\n')
        emit('progress',percent=100,title='Local GE6 runtime prepared; Steam services and licensed DLC unavailable')
    return manifest


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game-dir',type=Path,required=True)
    parser.add_argument('--activate-recovered',action='store_true')
    parser.add_argument('--dry-run',action='store_true')
    args=parser.parse_args()
    try:
        if sys.platform=='win32' and not args.dry_run:
            from runtime_snapshot import processes
            if list(processes()):raise ValueError('Close the running FS15 session before preparing its local copy')
        manifest=prepare_local(ROOT,args.game_dir,args.activate_recovered,args.dry_run)
        print(json.dumps({key:manifest[key] for key in ('runtime_transport','profile','command','local_platform_scope')},indent=2),flush=True)
    except (OSError,ValueError,subprocess.CalledProcessError) as error:parser.exit(1,str(error)+'\n')


if __name__=='__main__':main()
