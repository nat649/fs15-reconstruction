"""Exercise the installed headless host from a separate working directory."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--exe',type=Path,required=True);p.add_argument('--workdir',type=Path,required=True);p.add_argument('--report',type=Path);a=p.parse_args()
    exe=a.exe.resolve();a.workdir.mkdir(parents=True,exist_ok=True);checks=[]
    with tempfile.TemporaryDirectory(prefix='portable-host-',dir=a.workdir.resolve()) as folder:
        cwd=Path(folder);game=cwd/'game with spaces';game.mkdir();data=game/'data/maps';data.mkdir(parents=True)
        files={'dataS.gar':b'authored archive placeholder','data/maps/map01.i3d':b'authored map placeholder'}
        for name,payload in files.items():(game/name).write_bytes(payload)
        def run(*args,code=0):
            r=subprocess.run([str(exe),*map(str,args)],cwd=cwd,capture_output=True,text=True,encoding='utf-8')
            if r.returncode!=code:raise AssertionError(f'{args}: expected exit {code}, got {r.returncode}: {r.stderr}')
            return r
        assert 'headless' in run('--help').stdout;checks.append('help_from_other_working_directory')
        baseline=json.loads(run('--probe').stdout)
        assert baseline['status']=='passed' and baseline['resources'] is None and not baseline['native_engine_started'] and not baseline['renderer_started'] and not baseline['playable'] and not baseline['physics_world_started']
        assert baseline['cpu_probe']==dict(view_constructed_and_destroyed=True,buffers_created=2,buffers_destroyed=2,wheel_registry_force_adapter=True,camera_constructed_and_destroyed=True,view_prepared=True,shader_block_floats=49,map_calls=1,unmap_calls=1,visible_objects=1,outside_objects=1,retained_allocations=0)
        checks.append('recovered_core_probe_and_complete_cleanup')
        report=cwd/'report with spaces.json';result=json.loads(run('--probe','--game-dir',game,'--report',report).stdout)
        assert json.loads(report.read_text())==result
        inventory=result['resources'];assert not inventory['version_verified'] and not inventory['loaded_into_engine'];entries={x['path']:x for x in inventory['files']}
        assert len(entries)==4
        for name in entries:assert entries[name]['present']==(name in files) and entries[name]['bytes']==(len(files[name]) if name in files else None)
        assert all((game/name).read_bytes()==payload for name,payload in files.items());checks.append('read_only_resource_inventory_and_report_with_spaces')
        for args in [('--unknown',),('--report',),('--probe','--probe'),('--game-dir',game),('--probe','--game-dir',cwd/'missing')]:run(*args,code=2)
        checks.append('invalid_arguments_and_missing_directory_reject')
        for name in ('dataS.gar','new-report.json'):run('--probe','--game-dir',game,'--report',game/name,code=2)
        assert not (game/'new-report.json').exists() and (game/'dataS.gar').read_bytes()==files['dataS.gar'];checks.append('report_cannot_modify_game_directory')
        run('--probe','--report',cwd/'missing/report.json',code=1);checks.append('report_write_failure_has_nonzero_exit')
        evidence=dict(status='passed',platform=baseline['platform'],architecture=baseline['architecture'],executable_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),checks=checks,
            native_engine_started=False,renderer_started=False,physics_world_started=False,playable=False,cpu_probe=baseline['cpu_probe'],scope='Installed authored headless host, recovered CPU/lifetime chain and read-only filesystem/CLI contract; no full engine or original resources required.')
    if a.report:a.report.write_text(json.dumps(evidence,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(evidence,indent=2))

if __name__=='__main__':main()
