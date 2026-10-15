"""Detect a local FS15 installation and prepare original assets for the host.

No game assets are downloaded. Existing extraction/mesh/script tools are reused;
their original-code dependencies remain explicit. All generated data stays local.
"""
import argparse
from contextlib import contextmanager
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]
EXPECTED_IMAGE='6d1beaf9d7e3c57dd51cec03385a53bda82b4ac946e622d8d2fa592a56d71f9f'
GAME_FILES=('FarmingSimulator2015.xml','x64/FarmingSimulator2015Game.exe',
            'data/maps/map01.i3d','data/maps/map02.i3d')
HOME_FILES=('data/maps/map01.i3d','data/maps/map02.i3d','data/maps/map01.meshes',
            'data/maps/map02.meshes','data/maps/map01/map01_dem.png',
            'data/maps/map02/americanMap_dem.png','data/sky/environment.curves',
            'profileTemplate/careerVehicles_map01.xml','profileTemplate/careerVehicles_map02.xml')
TERRAIN_FILES=('data/maps/map01.i3d','data/maps/map02.i3d',
               'data/maps/map01/map01_dem.png','data/maps/map02/americanMap_dem.png',
               'data/maps/map01/pda_map_H.dds','data/maps/map02/pda_map_H.dds')


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream,'sha256').hexdigest()


def valid_game(path):
    return all((Path(path)/name).is_file() for name in GAME_FILES)


def library_paths(text):
    # Steam's quoted VDF paths; also accept the older numeric library entries.
    for key,value in re.findall(r'"([^"\n]+)"\s*"((?:\\.|[^"\\])*)"',text):
        if key=='path' or (key.isdigit() and (':' in value or value.startswith('/'))):
            yield Path(value.replace('\\\\','\\').replace('\\"','"'))


def detect_installations(root=ROOT,steam_roots=None):
    root=Path(root)
    candidates=[]
    if os.environ.get('FS15_GAME_DIR'):candidates.append(Path(os.environ['FS15_GAME_DIR']))
    for path,key in ((root/'build/launcher/settings.json','GameDirectory'),
                     (root/'build/native-engine/manifest.json','game_directory'),
                     (root/'build/launcher/assets.json','game_directory')):
        try:
            value=json.loads(path.read_text(encoding='utf-8-sig')).get(key)
            if value:candidates.append(Path(value))
        except (OSError,ValueError,AttributeError):pass
    roots=list(steam_roots or [])
    if steam_roots is None:
        roots.extend(Path(os.environ.get(env,''))/'Steam' for env in ('ProgramFiles(x86)','ProgramFiles') if os.environ.get(env))
        if os.name=='nt':
            import winreg
            for hive,key,name in ((winreg.HKEY_CURRENT_USER,r'Software\Valve\Steam','SteamPath'),
                                  (winreg.HKEY_LOCAL_MACHINE,r'SOFTWARE\WOW6432Node\Valve\Steam','InstallPath')):
                try:
                    with winreg.OpenKey(hive,key) as registry:roots.append(Path(winreg.QueryValueEx(registry,name)[0]))
                except OSError:pass
    for steam in list(roots):
        try:roots.extend(library_paths((steam/'steamapps/libraryfolders.vdf').read_text(encoding='utf-8-sig')))
        except OSError:pass
    for steam in roots:
        apps=steam/'steamapps'
        candidates.append(apps/'common/Farming Simulator 15')
        try:
            text=(apps/'appmanifest_313160.acf').read_text(encoding='utf-8-sig')
            match=re.search(r'"installdir"\s*"([^"\n]+)"',text)
            if match and '/' not in match[1] and '\\' not in match[1] and match[1] not in ('.','..'):
                candidates.append(apps/'common'/match[1])
        except OSError:pass
    unique={str(path.resolve()):path.resolve() for path in candidates if valid_game(path)}
    return list(unique.values())


def resource_status(root=ROOT):
    root=Path(root)
    missing=[name for name in HOME_FILES if not (root/'recovered/gar'/name).is_file()]
    for map_id in ('map01','map02'):
        for name in ('fruit_density.grle','cultivator_density.grle'):
            relative=f'data/maps/{map_id}/{name}'
            if not (root/'recovered/gar'/relative).is_file():missing.append(relative)
    for name in ('gui/MainScreen.lua','vehicles/WheelsUtil.lua'):
        if not (root/'recovered/lua/dataS/scripts'/name).is_file():missing.append('recovered Lua: '+name)
    try:
        vehicles=json.loads((root/'recovered/mission-provenance.json').read_text(encoding='utf-8-sig'))['vehicles']
        if not vehicles:missing.append('prepared starting fleets')
        for vehicle in vehicles:
            for path in (Path(vehicle['runtime']),Path(vehicle['runtime']).with_suffix('.meshes')):
                if not (root/'recovered/gar'/path).is_file():missing.append(path.as_posix())
    except (OSError,ValueError,KeyError,TypeError):missing.append('prepared starting fleets')
    incomplete=False
    try:incomplete=json.loads((root/'build/launcher/assets.json').read_text(encoding='utf-8-sig')).get('status')!='complete'
    except FileNotFoundError:pass
    except (OSError,ValueError,AttributeError):incomplete=True
    executable=next((p for p in (root/'FS15EngineRebuilt.exe',root/'build/Release/FS15EngineRebuilt.exe') if p.is_file()),None)
    return dict(assets_present=not missing,missing=missing,host_built=executable is not None,
                preparation_incomplete=incomplete,home_launch_ready=not missing and executable is not None and not incomplete,
                scope='Required-file preflight only; full resource hash validation and rendered startup are separate')


def checked_output(root,relative):
    root=Path(root).resolve()
    target=root/relative
    if not target.resolve().is_relative_to(root) or target.is_symlink():
        raise ValueError('Generated output escapes the workspace: '+str(relative))
    for parent in (target,*target.parents):
        if parent==root:break
        if parent.is_symlink() or (hasattr(parent,'is_junction') and parent.is_junction()):
            raise ValueError('Linked preparation output: '+str(relative))
    return target


def preflight_outputs(root):
    # Existing preparation scripts write many referenced paths. Reject redirected
    # parent directories before invoking them, not just the top-level directory.
    for relative in ('recovered','inputs/archives','analysis','build/shape-bridge','build/mission-meshes'):
        base=checked_output(root,relative)
        if base.exists():
            for path in base.rglob('*'):
                if path.is_symlink() or (hasattr(path,'is_junction') and path.is_junction()):
                    raise ValueError('Linked preparation output: '+str(path.relative_to(root)))


def copy_verified(source,target):
    if target.is_symlink() or (hasattr(target,'is_junction') and target.is_junction()):
        raise ValueError('Linked import destination: '+str(target))
    digest=sha(source)
    if target.is_file() and sha(target)==digest:return False
    target.parent.mkdir(parents=True,exist_ok=True)
    pending=target.with_name(target.name+'.importing')
    if pending.is_symlink() or (pending.exists() and not pending.is_file()):raise ValueError('Unexpected pending import: '+str(pending))
    try:
        shutil.copyfile(source,pending)
        if sha(pending)!=digest:raise ValueError('Source changed or copy verification failed: '+source.name)
        os.replace(pending,target)
    finally:
        if pending.exists():pending.unlink()
    return True


@contextmanager
def preparation_lock(root):
    path=checked_output(root,'build/launcher/assets.lock')
    path.parent.mkdir(parents=True,exist_ok=True)
    with path.open('a+b') as stream:
        stream.seek(0);stream.write(b'0');stream.flush();stream.seek(0)
        try:
            if os.name=='nt':
                import msvcrt
                msvcrt.locking(stream.fileno(),msvcrt.LK_NBLCK,1)
            else:
                import fcntl
                fcntl.flock(stream.fileno(),fcntl.LOCK_EX|fcntl.LOCK_NB)
        except OSError as error:raise ValueError('Another resource preparation is running') from error
        try:yield
        finally:
            stream.seek(0)
            if os.name=='nt':msvcrt.locking(stream.fileno(),msvcrt.LK_UNLCK,1)
            else:fcntl.flock(stream.fileno(),fcntl.LOCK_UN)


def emit(kind,**values):
    print('LAUNCHER_EVENT '+json.dumps(dict(event=kind,**values)),flush=True)


def prepare_assets(root,game,run=None):
    root,game=Path(root).resolve(),Path(game).resolve(strict=True)
    if not valid_game(game):raise ValueError('Select the installed FS15 directory containing data and x64')
    if game.is_relative_to(root/'recovered') or root.is_relative_to(game):
        raise ValueError('The game installation and workspace outputs must be separate')
    run=run or (lambda command:subprocess.run(command,cwd=root,check=True,env=dict(os.environ,PYTHONUTF8='1')))
    ps=shutil.which('pwsh') or shutil.which('powershell')
    if not ps:raise ValueError('PowerShell is required for the existing Windows preparation tools')
    def python(script,*args):return [sys.executable,'-u',str(root/'scripts'/script),*map(str,args)]
    def powershell(script,*args):return [ps,'-NoProfile','-ExecutionPolicy','Bypass','-File',str(root/'scripts'/script),*map(str,args)]
    with preparation_lock(root):
        preflight_outputs(root)
        manifest_path=checked_output(root,'build/launcher/assets.json')
        report=dict(status='preparing',game_directory=str(game),started_utc=datetime.now(timezone.utc).isoformat(),steps=[],installation_modified=False)
        def save():manifest_path.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
        save()
        def step(number,title,action):
            emit('progress',percent=number,title=title)
            action();report['steps'].append(title);save()
        try:
            step(5,'Checking local Lua and mesh tools',lambda: None)
            if not (root/'tools/LuaJIT/src/luajit.exe').is_file() or not (root/'tools/luajit-decompiler/main.py').is_file():
                run(powershell('setup_lua_tools.ps1'))
            if not (root/'build/shape-bridge/FS15ShapeBridge.dll').is_file():run(powershell('setup_shapes_tools.ps1'))
            # Archive extraction still requires the explicitly pinned original
            # oracle. Do not launch a game or capture a process as a side effect.
            recovery=root/'analysis/lua-recovery.json'
            def scripts_current():
                try:
                    rows=json.loads(recovery.read_text(encoding='utf-8-sig'))['files']
                    return bool(rows) and all(row['status'] in ('exact_match','syntax_only') and
                        sha((root/'recovered/lua/dataS'/row['path']).with_suffix('.lua'))==row['source_sha256'] for row in rows)
                except (OSError,ValueError,KeyError):return False
            def archive_current(name):
                try:
                    manifest=json.loads((root/'analysis'/(name+'-extraction.json')).read_text(encoding='utf-8-sig'))
                    from gar import safe_relative_name
                    return sha(game/(name+'.gar'))==manifest['archive_sha256'] and bool(manifest['files']) and all(
                        sha(root/'recovered/gar'/name/safe_relative_name(row['name']).as_posix())==row['sha256'] for row in manifest['files'])
                except (OSError,ValueError,KeyError,TypeError):return False
            if not scripts_current() or not all(archive_current(name) for name in ('dataS','dataS2')):
                image=root/'inputs/runtime/FarmingSimulator2015Game.runtime.bin'
                if not image.is_file() or sha(image)!=EXPECTED_IMAGE:
                    raise ValueError('GAR extraction needs the verified local FS15 1.4.2.0 native analysis image. See docs/LAUNCHER.md; loose assets alone cannot boot the recovered frontend.')
                for index,name in enumerate(('dataS','dataS2')):
                    source=game/(name+'.gar');target=checked_output(root,'inputs/archives/'+name+'.gar')
                    step(12+index*8,'Extracting local '+name+' archive',lambda source=source,target=target: (
                        copy_verified(source,target),run(python('extract_gar.py',target,'--decode-luajit','--validate-xml'))))
                step(30,'Recovering and validating Lua sources',lambda:run(python('recover_lua.py')))
                if not scripts_current():raise ValueError('Lua recovery did not produce a completely compilable source set')
            def terrain():
                for name in TERRAIN_FILES:copy_verified(game/name,checked_output(root,'recovered/gar/'+name))
                run(powershell('prepare-engine-assets.ps1','-GameDir',game))
            step(45,'Preparing original terrain and layer textures',terrain)
            step(60,'Decoding original map meshes and sky resources',lambda:run(python('prepare_scene.py','--game-dir',game)))
            step(80,'Preparing original starting fleets and field grids',lambda:run(python('prepare_mission.py','--game-dir',game)))
            status=resource_status(root)
            if not status['assets_present']:raise ValueError('Preparation incomplete: '+', '.join(status['missing']))
            # Record content hashes rather than treating an old marker as proof.
            files={}
            for path in (root/'recovered/gar').rglob('*'):
                if path.is_file():files[path.relative_to(root/'recovered/gar').as_posix()]=dict(bytes=path.stat().st_size,sha256=sha(path))
            report.update(status='complete',completed_utc=datetime.now(timezone.utc).isoformat(),files=files)
            save();status=resource_status(root);report['resource_status']=status
            save();emit('complete',percent=100,title='Original resources prepared locally',files=len(files),**status)
        except BaseException as error:
            report.update(status='failed',error=str(error));save();raise
    return report


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action',choices=('detect','status','prepare'))
    parser.add_argument('--game-dir',type=Path)
    args=parser.parse_args()
    try:
        if args.action=='detect':emit('installations',paths=[str(p) for p in detect_installations()])
        elif args.action=='status':emit('resources',**resource_status())
        elif args.game_dir:prepare_assets(ROOT,args.game_dir)
        else:raise ValueError('Select your FS15 installation first')
    except (OSError,ValueError,subprocess.CalledProcessError) as error:
        emit('error',title=str(error));return 1
    return 0


if __name__=='__main__':
    sys.exit(main())
