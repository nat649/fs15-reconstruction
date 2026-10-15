"""Run actual original-map/cutter/harvest integration; never load user saves."""
import argparse
import json
from pathlib import Path
import subprocess

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--exe',type=Path,required=True)
    parser.add_argument('--root',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    args.output.mkdir(parents=True,exist_ok=True)
    maps=[]
    for name in ('Map01','Map02'):
        report=args.output/(name+'.json')
        capture=args.output/(name+'.png')
        subprocess.run([str(args.exe.resolve()),'--root',str(args.root.resolve()),'--map',name,'--scenario','harvest','--smoke',
                        '--report',str(report.resolve()),'--capture',str(capture.resolve())],check=True,timeout=180)
        data=json.loads(report.read_text())
        mission=data['mission']
        assert mission['harvested_l']>1, (name,'Original cutter did not harvest')
        assert mission['density_revision']>0 and mission['foliage_instances']>0
        assert data['visible_vehicle_gpu_samples']>0 and not data['original_engine_loaded']
        assert mission['fields']==(41 if name=='Map01' else 21)
        # The test scenario relocates original equipment to an originally owned,
        # already mature field. It does not claim normal career spawn positions.
        maps.append(dict(map=name,vehicles=mission['vehicles'],fields=mission['fields'],
                         harvestedLiters=mission['harvested_l'],distanceMetres=mission['distance_m'],
                         foliageInstances=mission['foliage_instances'],collisionTriangles=mission['collision_triangles'],
                         visibleVehicleSamples=data['visible_vehicle_gpu_samples']))
    tipping=[]
    for name in ('Map01','Map02'):
        report=args.output/(name+'-tip.json')
        capture=args.output/(name+'-tip.png')
        subprocess.run([str(args.exe.resolve()),'--root',str(args.root.resolve()),'--map',name,'--scenario','tip','--smoke',
                        '--report',str(report.resolve()),'--capture',str(capture.resolve())],check=True,timeout=180)
        data=json.loads(report.read_text());mission=data['mission']
        assert mission['unloaded_l']>0 and mission['implement_tip_state'] in (1,2)
        assert mission['active_animations']>0 and mission['recovered_animations']>0
        assert abs(mission['implement_fill_l']+mission['unloaded_l']-mission['implement_capacity_l'])<0.001
        assert abs(mission['farm_wheat_l']-mission['unloaded_l']-5000)<0.001
        assert data['visible_vehicle_gpu_samples']>0 and not data['original_engine_loaded']
        tipping.append(dict(map=name,unloadedLiters=mission['unloaded_l'],trailerLiters=mission['implement_fill_l'],capacityLiters=mission['implement_capacity_l'],
                            farmWheatLiters=mission['farm_wheat_l'],recoveredAnimations=mission['recovered_animations'],
                            activeAnimations=mission['active_animations'],visibleVehicleSamples=data['visible_vehicle_gpu_samples']))
    result=dict(status='passed',scope='Original harvest and animated farm-discharge scenarios; host physics/native services remain approximate',maps=maps,tipScenarios=tipping)
    (args.output/'summary.json').write_text(json.dumps(result,indent=2)+'\n')
    if (args.root/'analysis').is_dir():
        (args.root/'analysis/mission-validation.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))

if __name__=='__main__': main()
