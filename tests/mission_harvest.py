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
    result=dict(status='passed',scope='Original owned-field harvest scenario, original combine/cutter; host native services remain approximate',maps=maps)
    (args.output/'summary.json').write_text(json.dumps(result,indent=2)+'\n')
    if (args.root/'analysis').is_dir():
        (args.root/'analysis/mission-validation.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))

if __name__=='__main__': main()
