"""Compare native frustum/clip predicates and ordered spatial visibility queries."""
import argparse
from collections import Counter
import ctypes as C
import hashlib
import json
import math
from pathlib import Path
import random
import struct
from inventory import ROOT,write_json
from native_gar import EXPECTED_IMAGE
from native_animation_xml import NativeAnimationXml
from check_scene_lifecycle import PortableServices,Services,P,N,read,put
from check_scene_spatial import Grid,Cell,Entry,Sphere,Update,WORLD,configure as configure_spatial
from check_scene_lod import Manager,Entry as LodEntry
from check_scene_light import Fixture,local_cases

F=C.c_float;U=C.c_uint32
class Frustum(C.Structure):_fields_=[('vtable',N),('planes',(F*4)*6)]
class Clip(C.Structure):_fields_=[('vtable',N),('planes',P),('count',U),('unknown14',U)]

def configure(core):
    configure_spatial(core)
    for name,signature in {'plane_normalize':[P,N],'frustum_matrix':[P,P,N],
        'frustum_point':[P,P,P],'frustum_sphere':[P,P,P],'frustum_box':[P,P,P,P],
        'clip_point':[P,P,P],'clip_sphere':[P,P,P],'clip_box':[P,P,P,P],
        'spatial_entry_visible':[P,P,F,P,P,P],'spatial_volume_query':[P,P,P,F,P,P,U,P],
        'spatial_point_query':[P,P,P,P,U,P]}.items():
        f=getattr(core,'fs15_scene_'+name);f.argtypes=signature;f.restype=C.c_int

def matrix(center,half):
    values=[0.0]*16
    for i in range(3):values[i*5]=1.0/half[i];values[12+i]=-center[i]/half[i]
    values[15]=1;return (F*16)(*values)

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--dll',type=Path,required=True);args=parser.parse_args()
    core=C.CDLL(str(args.dll.resolve()));configure(core);rng=random.Random(0xf515d7250);counts=Counter();digest=hashlib.sha256()
    with NativeAnimationXml() as native:
        nf=lambda address,result,*types:C.WINFUNCTYPE(result,*types)(native.base+address)
        nf(0x524b0,P)();normalize=nf(0x6d6c0,None,P);extract=nf(0x289cc0,None,P,P)
        fixed=[nf(address,C.c_uint8,*signature) for address,signature in [(0x289e90,[P,P]),(0x289f00,[P,P,F]),(0x289f80,[P,P,P])]]
        dynamic=[nf(address,C.c_uint8,*signature) for address,signature in [(0x1af460,[P,P]),(0x1af4e0,[P,P,F]),(0x1af560,[P,P,P])]]
        entry_test=nf(0xd6580,C.c_uint8,P,P,F,P,P);volume_query=nf(0xd7250,U,P,P,P,F,P,P,U);point_query=nf(0xd7460,U,P,P,P,P,U)
        for case in range(4000):
            values=[math.ldexp(rng.uniform(-1,1),case%80-40) for _ in range(4)] if case else [0,0,0,1]
            a,b=(F*4)(*values),(F*4)(*values);normalize(a)
            if not core.fs15_scene_plane_normalize(b,4) or bytes(a)!=bytes(b):raise AssertionError('Plane normalization differs')
            digest.update(bytes(a));counts['exact_plane_normalizations']+=1
        for case in range(1200):
            values=[rng.uniform(-5,5) for _ in range(16)] if case%3 else list(matrix([rng.uniform(-100,100) for _ in range(3)],[rng.uniform(1,200) for _ in range(3)]))
            if case==0:values=[0]*16
            if case==1:values=list(matrix([0,0,0],[1,1,1]))
            m=(F*16)(*values);a,b=Frustum(),Frustum();a.vtable=b.vtable=0x1404c7cc0;extract(C.byref(a),m)
            if not core.fs15_scene_frustum_matrix(C.byref(b),m,16) or bytes(a)!=bytes(b):raise AssertionError(f'Frustum plane extraction differs {case}')
            digest.update(bytes(a));counts['exact_frustum_extractions']+=1
            # Real original virtual targets are called directly; no fabricated
            # callback substitutes for the sphere/box predicates.
            extra=(F*4*3)()
            for row in extra:
                for i in range(4):row[i]=rng.uniform(-1,1)
            plane_rows=list(a.planes)+list(extra);storage=((F*4)*len(plane_rows))(*plane_rows)
            clip=Clip(0x1404c7da8,C.addressof(storage),[0,1,3,6,9][case%5],0xa5a5a5a5)
            for step in range(8):
                x=(F*4)(*[rng.uniform(-200,200) for _ in range(3)],rng.uniform(-3,50));half=(F*3)(*[rng.uniform(-3,20) for _ in range(3)])
                if step==0:x[:]=[0,0,0,0]
                for prefix,record,original in [('frustum',a,fixed),('clip',clip,dynamic)]:
                    for kind in range(3):
                        out=C.c_int();expected=original[kind](C.byref(record),x,*([x[3]] if kind==1 else [half] if kind==2 else []))
                        params=[C.byref(record),x]+([half] if kind==2 else [])+[C.byref(out)]
                        if not getattr(core,'fs15_scene_'+prefix+'_'+['point','sphere','box'][kind])(*params) or out.value!=bool(expected):raise AssertionError(f'{prefix} predicate differs {case}/{step}/{kind}')
                        counts[prefix+'_'+['point','sphere','box'][kind]+'_predicates']+=1;digest.update(bytes([out.value]))
        # Native strictness at exact tangency and the first/last clipping planes.
        a=Frustum();a.vtable=0x1404c7cc0;extract(C.byref(a),matrix([0,0,0],[1,1,1]))
        for axis in range(3):
            for sign in (-1,1):
                for distance in [0.9999999403953552,1.0,1.0000001192092896,2.0]:
                    x=(F*4)(0,0,0,abs(distance)-1);x[axis]=sign*distance
                    for kind in (0,1):
                        out=C.c_int();expected=fixed[kind](C.byref(a),x,*([x[3]] if kind else []));fn=core.fs15_scene_frustum_sphere if kind else core.fs15_scene_frustum_point
                        if not fn(C.byref(a),x,C.byref(out)) or out.value!=bool(expected):raise AssertionError('Native tangency boundary differs')
                        counts['exact_tangency_predicates']+=1
        def managers():
            arrays=[(LodEntry*4)(),(LodEntry*4)()];result=[]
            for rows in arrays:
                for i,e in enumerate(rows):e.node=0x1000+i;e.position[:]=[i*30,0,i*-10];e.squared[:]=[0,100,2500,10000];e.levels=4;e.selected=0;e.stamp=0
                result.append(Manager(C.addressof(rows),(F*3)(0,0,0),1,4,4,1))
            return arrays,result
        def compare_managers(a,b,amap=None,bmap=None):
            x,y=bytearray(bytes(a)),bytearray(bytes(b));x[:8]=y[:8]=bytes(8)
            if x!=y:raise AssertionError('Query LOD manager header differs')
            x,y=bytearray(C.string_at(a.entries,a.capacity*40) if a.entries else b''),bytearray(C.string_at(b.entries,b.capacity*40) if b.entries else b'')
            for data,mapping in [(x,amap or {}),(y,bmap or {})]:
                for i in range(a.capacity):p=struct.unpack_from('<Q',data,i*40)[0];struct.pack_into('<Q',data,i*40,mapping.get(p,p))
            if x!=y:raise AssertionError('Query complete LOD allocation/cache differs')
            digest.update(x);counts['complete_query_lod_cache_states']+=1
        arrays,ms=managers()
        for case in range(4000):
            e=Entry();e.center[:]=[rng.uniform(-100,100) for _ in range(3)];e.radius=rng.uniform(0,40);e.clip[:]=[rng.uniform(-1,300),rng.uniform(0,40)];e.lod_token=0xffffffff if case%3==0 else ((case%4)<<30)|((case//4)%4)
            viewer=(F*3)(*[rng.uniform(-200,200) for _ in range(3)]);factor=F([0,0.5,1,2,-1][case%5]);out=C.c_int()
            for m in ms:m.stamp=[0,1,65535,65536,65537,0xffffffff][case%6];m.viewer[:]=viewer;m.factor=[0,0.25,1,4][case%4]
            if case%13==0:e.center[:]=viewer;e.radius=0;e.clip[:]=[0,0]
            expected=entry_test(C.byref(a),viewer,factor,C.byref(e),C.byref(ms[0]))
            if not core.fs15_scene_spatial_entry_visible(C.byref(a),viewer,factor,C.byref(e),C.byref(ms[1]),C.byref(out)) or out.value!=bool(expected):raise AssertionError('Complete entry filter differs')
            compare_managers(*ms);counts['entry_visibility_filters']+=1
        def query(g0,g1,v,viewer,factor,m0,m1,maps,capacity,point=False):
            sent=0x6a6a6a6a6a6a6a6a;left=(N*(capacity+3))(*([sent]*(capacity+3)));right=(N*(capacity+3))(*([sent]*(capacity+3)));total=U()
            outputs=[left if capacity else None,right if capacity else None]
            if point:
                expected=point_query(C.byref(g0),viewer,C.byref(m0),outputs[0],capacity);ok=core.fs15_scene_spatial_point_query(C.byref(g1),viewer,C.byref(m1),outputs[1],capacity,C.byref(total));name='point_queries'
            else:
                expected=volume_query(C.byref(g0),C.byref(v),viewer,factor,C.byref(m0),outputs[0],capacity);ok=core.fs15_scene_spatial_volume_query(C.byref(g1),C.byref(v),viewer,factor,C.byref(m1),outputs[1],capacity,C.byref(total));name='volume_queries'
            if not ok or total.value!=expected:raise AssertionError(f'{name} total differs: {total.value} != {expected}')
            left=[maps[0].get(p,p) for p in left];right=[maps[1].get(p,p) for p in right]
            if left!=right:raise AssertionError(f'{name} output prefix/spare bytes/order differs')
            if expected>capacity:counts['queries_with_truncated_output']+=1
            counts[name]+=1;counts['query_total_matches']+=expected;digest.update(struct.pack('<I',expected));digest.update(struct.pack('<'+'Q'*len(left),*left));return expected
        initialize=nf(0xd6660,None,P,F,F,F,F,F,F,C.c_int32);add=nf(0xd6f00,None,P,P,P,F,F,U);free=nf(0x3fabc8,None,P)
        def free_grid(g):
            for i in range(g.count):
                row=Cell.from_address(g.cells+i*32)
                if row.entries:free(row.entries)
            free(g.cells);g.cells=0
        # Synthetic scenes include root sentinels, dense cells, cached LOD tokens
        # and volumes whose rejection must skip descendants without reordering.
        for case in range(32):
            owner=PortableServices(core);grids=[Grid(),Grid()];lo=(F*3)(-256,-500,-256);hi=(F*3)(256,500,256);dimension=[1,4,16,32][case%4]
            initialize(C.byref(grids[0]),*lo,*hi,dimension)
            if not core.fs15_scene_spatial_init(C.byref(grids[1]),lo,hi,dimension,C.byref(owner.api)):raise AssertionError('Visibility grid init rejected')
            keys=[(U*160)(),(U*160)()];maps=[{C.addressof(k)+i*4:i+1 for i in range(len(k))} for k in keys];arrays,ms=managers()
            for i in range(len(keys[0])):
                s=Sphere();s.center[:]=[rng.uniform(-220,220) for _ in range(3)];s.radius=rng.uniform(0,50)
                if i%8==0:s.center[:]=[0,0,0];s.radius=2
                pointer=None if i%17==0 else C.byref(s);clip0,clip1=rng.uniform(100,1000),rng.uniform(0,20);token=0xffffffff if i%3 else ((i%4)<<30)|(i//4%4)
                add(C.byref(grids[0]),C.addressof(keys[0])+i*4,pointer,clip0,clip1,token)
                if not core.fs15_scene_spatial_add(C.byref(grids[1]),C.addressof(keys[1])+i*4,pointer,clip0,clip1,token,C.byref(owner.api)):raise AssertionError('Visibility fixture object rejected')
            frozen=[C.string_at(g.cells,g.count*32) for g in grids];row_bytes=[[C.string_at(Cell.from_address(g.cells+i*32).entries,Cell.from_address(g.cells+i*32).capacity*40) if Cell.from_address(g.cells+i*32).entries else b'' for i in range(g.count)] for g in grids]
            for step in range(64):
                viewer=(F*3)(*[rng.uniform(-250,250) for _ in range(3)]) if step%4 else (F*3)(0,0,0);volume=Frustum();volume.vtable=0x1404c7cc0;extract(C.byref(volume),matrix(viewer,[rng.uniform(10,300) for _ in range(3)]))
                dynamic_volume=Clip(0x1404c7da8,C.addressof(volume.planes),[0,1,4,6][step%4],0);v=dynamic_volume if step%2 else volume
                factor=F([0,0.5,1,2][step%4]);capacity=[0,1,5,16,160][step%5]
                for m in ms:m.stamp=step//2;m.viewer[:]=viewer;m.factor=float(factor.value*factor.value)
                query(*grids,v,viewer,factor,*ms,maps,capacity);compare_managers(*ms)
                query(*grids,v,viewer,factor,*ms,maps,capacity,True);compare_managers(*ms)
            for side,g in enumerate(grids):
                if C.string_at(g.cells,g.count*32)!=frozen[side]:raise AssertionError('Query changed cell headers')
                for i in range(g.count):
                    c=Cell.from_address(g.cells+i*32)
                    if (C.string_at(c.entries,c.capacity*40) if c.entries else b'')!=row_bytes[side][i]:raise AssertionError('Query changed spatial entry/spare storage')
            free_grid(grids[0]);core.fs15_scene_spatial_release(C.byref(grids[1]),C.byref(owner.api))
            if owner.blocks:raise AssertionError('Visibility fixture leaked')
            counts['synthetic_query_grid_lifetimes']+=1
        # Loaded original Light records and mixed LOD trees use actual GE6 bounds
        # and its unchanged manager consumer before issuing visibility queries.
        local,assets=local_cases();cases=[(None,[('TransformGroup',[('name','lod'),('lodDistance','0 40 120')]),('Light',[('name','point'),('range','30')]),('end',[]),('Light',[('name','spot'),('type','spot'),('range','60')]),('end',[]),('end',[])]) for _ in range(12)]+local
        for case,(path,events) in enumerate(cases):
            C.memset(native.base+0x162cbd0,0,0x140);f=Fixture(core,native,case*1000);g=Grid();ng=Grid.from_address(native.base+0x162cc40);lo=(F*3)(-1024,-500,-1024);hi=(F*3)(1024,500,1024)
            initialize(C.byref(ng),*lo,*hi,16);core.fs15_scene_spatial_init(C.byref(g),lo,hi,16,C.byref(f.owner.api))
            @WORLD
            def world(_,p,out):
                array,kinds,view=f.view();position=(F*3)();ok=core.fs15_scene_owned_world(C.byref(view),f.lookup[p],position,3)
                if ok:C.memmove(out,p+0xd8,64)
                return ok
            services=Update(None,C.pointer(f.owner.api),world);f.load(events,counts);nf(0x87f00,None,P)(native.base+0x162cbd0)
            if not core.fs15_scene_lights_flush(C.byref(f.lod),C.byref(f.lod_services),C.byref(f.lights),C.byref(g),C.byref(services)):raise AssertionError('Loaded query scene update rejected')
            lights=[i for i,k in enumerate(f.kinds) if k==3];maps=[{p+0x170:i+1 for i,p in enumerate(side) if f.kinds[i]==3} for side in f.nodes];m0=Manager.from_address(f.manager);m1=f.lod.manager
            for step in range(36):
                center=struct.unpack('<3f',C.string_at(f.nodes[0][lights[step%len(lights)]]+0x1e4,12));viewer=(F*3)(*[value+(rng.uniform(-100,100) if step%3 else 0) for value in center])
                v=Frustum();v.vtable=0x1404c7cc0;extract(C.byref(v),matrix(viewer,[150,150,150]));factor=F([0.5,1,2][step%3]);capacity=[0,1,16][step%3]
                for m in (m0,m1):m.stamp=step;m.viewer[:]=viewer;m.factor=float(factor.value*factor.value)
                query(ng,g,v,viewer,factor,m0,m1,maps,capacity);query(ng,g,v,viewer,factor,m0,m1,maps,capacity,True);f.compare(counts,digest)
            for i in lights:
                for p in (f.nodes[0][i],f.nodes[1][i]):put(p,0x50,(read(p,0x50,'I')|0x400)&~2,'I')
                nf(0xda450,None,P)(f.nodes[0][i]);core.fs15_scene_light_update(f.nodes[1][i],C.byref(g),C.byref(services))
            free_grid(ng);core.fs15_scene_spatial_release(C.byref(g),C.byref(f.owner.api));f.close();counts['loaded_query_forest_lifetimes']+=1
            if path:counts['complete_original_light_query_subtrees']+=1
    paths=sorted(p for p in (ROOT/'src/core').glob('*') if p.suffix in ('.cpp','.h'))
    report=dict(status='passed',reference_sha256=EXPECTED_IMAGE,native_entries=['0x14006d6c0','0x140289cc0','0x140289e90','0x140289f00','0x140289f80','0x1401af460','0x1401af4e0','0x1401af560','0x1400d6580','0x1400d7250','0x1400d7460'],rebuilt_source_sha256={p.relative_to(ROOT).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in paths},tested_dll_sha256=hashlib.sha256(args.dll.read_bytes()).hexdigest(),comparisons=dict(counts),original_results_sha256=digest.hexdigest(),local_assets=assets,scope='Plane normalization/extraction, fixed-six and dynamic-plane predicates, LOD/clip-distance entry filtering, ordered grid volume/point queries and bounded output counts. Actual original volume functions and query routines execute unchanged; synthetic matrices/planes, owned world/allocator transport and guards are authored. Full output prefixes/spares and LOD cache allocations compared; spatial storage remains unchanged. Full camera projection/view preparation, dynamic shadow-volume construction, render masks/Shape/audio consumers, GPU resources and engine startup remain open.')
    write_json(ROOT/'analysis/scene-visibility-validation.json',report);print(json.dumps(report,indent=2))

if __name__=='__main__':main()
