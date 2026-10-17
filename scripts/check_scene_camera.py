"""Compare recovered Camera creation, XML, pose and follow paths with GE6."""
import argparse
from collections import Counter
import ctypes as C
import hashlib
import json
import math
from pathlib import Path
import random
import struct
import xml.etree.ElementTree as ET
from inventory import ROOT,write_json
from native_gar import EXPECTED_IMAGE
from native_animation_xml import NativeAnimationXml
from check_animation_xml import attributes
from check_scene_xml import XmlServices,REBUILD,PROPAGATE,INTEGER,REGISTER
from check_scene_lifecycle import PortableServices,Services,DESTROY,P,N,read,put,canonical
from check_scene_owned import OwnedView
from check_scene_groups import GroupServices,Result,CREATED,LINK
from local_reference_assets import reference_asset

WORLD=C.CFUNCTYPE(C.c_int,P,P,P)
class CameraServices(C.Structure):
    _fields_=[('user',P),('world',WORLD),('refresh',REBUILD),('rebuild',REBUILD)]


def local_cases():
    paths=[ROOT/'recovered/gar/data/maps/map01.i3d',ROOT/'recovered/gar/data/maps/map02.i3d']
    inventory=ROOT/'analysis/inventory.json'
    if inventory.is_file():
        paths.extend([reference_asset('data/vehicles/steerable/deutz/deutzAgrostar661.i3d'),reference_asset('data/vehicles/steerable/newHolland/newHollandT8320.i3d')])
    cases=[];assets=[]
    for path in paths:
        if not path.is_file():continue
        cameras=list(ET.parse(path).getroot().iter('Camera'))
        cases.extend(list(c.attrib.items()) for c in cameras)
        assets.append(dict(name=path.name,sha256=hashlib.sha256(path.read_bytes()).hexdigest(),camera_attribute_sets=len(cameras)))
    return cases,assets


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--dll',type=Path,required=True);args=parser.parse_args()
    core=C.CDLL(str(args.dll.resolve()));rng=random.Random(0xf515b5870);counts=Counter();digest=hashlib.sha256()
    signatures={
        'fs15_scene_camera_construct':[P,N,C.c_char_p,N,C.c_float,C.c_float,C.c_float,C.c_uint8,C.c_float,C.POINTER(Services)],
        'fs15_scene_camera_destruct':[P,N,C.c_uint32,C.POINTER(Services)],'fs15_scene_camera_begin':[C.POINTER(P),P,N,P,C.POINTER(Result)],
        'fs15_scene_camera_script_create':[P,N,N,C.POINTER(Services),C.POINTER(P)],'fs15_scene_normalize3':[P,N],
        'fs15_scene_camera_pose':[P,P,N,P],'fs15_scene_camera_follow':[P,P,N,C.c_float,P],
        'fs15_scene_camera_smooth':[P,P,N,C.c_float,C.c_float,C.c_float,P],'fs15_scene_camera_script_follow':[P,P,N,P],
        'fs15_scene_construct':[P,N,C.c_uint,C.c_char_p,N,C.c_uint32,C.c_uint8,C.POINTER(Services)],'fs15_scene_destruct':[P,N,C.c_uint,C.c_uint32,C.POINTER(Services)],
        'fs15_scene_owned_state':[P,N,C.c_uint,C.c_uint32,C.c_int32,P,N,P],
        'fs15_scene_owned_trs':[P,N,C.c_uint,P,N,P,N,P,P],
        'fs15_scene_owned_link':[P,N,N,C.c_uint32,C.c_int,C.POINTER(Services),P,N,P,P],
        'fs15_scene_owned_unlink':[P,N,C.c_uint32,C.c_int,P,N,P],
        'fs15_scene_owned_world':[P,N,P,N],'fs15_scene_owned_matrix':[P,N,P,N],'fs15_compose_trs':[P,P,P,P,N]}
    for name,signature in signatures.items():getattr(core,name).argtypes=signature;getattr(core,name).restype=C.c_int
    with NativeAnimationXml() as native:
        allocate=lambda size:native.callbacks[0](native.heap,0,size)
        release=lambda p:native.callbacks[1](native.heap,0,p)
        ctor=C.WINFUNCTYPE(P,P,C.c_char_p,C.c_float,C.c_float,C.c_float,C.c_uint8,C.c_float)(native.base+0xb5870)
        deleting=C.WINFUNCTYPE(P,P,C.c_uint32)(native.base+0xb5910)
        create=C.WINFUNCTYPE(P,P)(native.base+0xe8840)
        normalize=C.WINFUNCTYPE(P,P)(native.base+0x52540)
        begin=C.WINFUNCTYPE(None,P,P,P)(native.base+0x18c430)
        end=C.WINFUNCTYPE(None,P)(native.base+0x18c370)
        node_ctor=C.WINFUNCTYPE(P,P,C.c_char_p,C.c_uint32)(native.base+0x10fa90)
        node_delete=C.WINFUNCTYPE(P,P,C.c_uint32)(native.base+0x10fb10)
        state=C.WINFUNCTYPE(None,P,C.c_uint32,C.c_int32)(native.base+0xb1b00)
        link_native=C.WINFUNCTYPE(None,P,P,C.c_uint32)(native.base+0xb17b0)
        unlink_native=C.WINFUNCTYPE(None,P,C.c_uint32,C.c_int)(native.base+0xb19c0)
        set_matrix=C.WINFUNCTYPE(None,P,P)(native.base+0x10fce0)
        world_native=C.WINFUNCTYPE(P,P)(native.base+0xb1a50)
        pose=C.WINFUNCTYPE(None,P,P)(native.base+0xb59f0)
        follow=C.WINFUNCTYPE(None,P,P,C.c_float)(native.base+0xb5be0)
        smooth=C.WINFUNCTYPE(None,P,P,C.c_float,C.c_float,C.c_float)(native.base+0xb5d00)
        script_follow=C.WINFUNCTYPE(None,P,P)(native.base+0xe8940)
        integer_native=C.WINFUNCTYPE(C.c_int32,C.c_char_p)(native.base+0x3fd784)
        C.WINFUNCTYPE(P)(native.base+0x524b0)()
        for at,expected in ((0x49f374,0.001),(0x4e68b8,0.00065)):
            if C.string_at(native.base+at,4)!=struct.pack('<f',expected):raise AssertionError('Camera smoothing constant changed')
        for case in range(2000):
            values=([0,0,0] if case==0 else [math.ldexp(rng.uniform(-1,1),case%80-40) for _ in range(3)])
            a,b=(C.c_float*3)(*values),(C.c_float*3)(*values);normalize(a)
            if not core.fs15_scene_normalize3(b,3) or bytes(a)!=bytes(b):raise AssertionError('Camera normalization differs')
            digest.update(bytes(a));counts['exact_vector_normalizations']+=1
        for case in range(400):
            owner=PortableServices(core);owner.sequence=case*3;put(native.base+0x1630d60,0,owner.sequence,'I')
            name=[b'',b'camera',b'\xc3\xa9quipe',b'x'*257][case%4];params=[rng.uniform(-200,200),rng.uniform(-1,2),rng.uniform(0,10000),case%256,rng.uniform(-10,100)]
            if case%2:
                frame=C.create_string_buffer(bytes([0xa5])*0x40,0x40);name_buffer=C.create_string_buffer(name);put(C.addressof(frame),0,C.addressof(name_buffer))
                for i,v in enumerate(params[:3]):put(C.addressof(frame),(i+1)*16,v,'f')
                a=create(frame);out=P()
                if not core.fs15_scene_camera_script_create(frame,len(frame),len(name)+1,C.byref(owner.api),C.byref(out)):raise AssertionError('Script camera create rejected')
                b=out.value;counts['script_creations']+=1
            else:
                payload=rng.randbytes(0x190);aa=C.create_string_buffer(payload,len(payload));bb=C.create_string_buffer(payload,len(payload));a,b=C.addressof(aa),C.addressof(bb)
                ctor(a,name,*params)
                if not core.fs15_scene_camera_construct(b,0x190,name,len(name)+1,*params,C.byref(owner.api)):raise AssertionError('Camera constructor rejected')
                counts['direct_constructors']+=1
            av,bv=canonical(C.string_at(a,0x190),3),canonical(C.string_at(b,0x190),3)
            if av!=bv:raise AssertionError('Complete camera constructor bytes differ')
            digest.update(av)
            flags=1 if case%2 else 0;deleting(a,flags)
            if not core.fs15_scene_camera_destruct(b,0x190,flags,C.byref(owner.api)):raise AssertionError('Camera deleting wrapper rejected')
            if not flags and canonical(C.string_at(a,0x190),3)!=canonical(C.string_at(b,0x190),3):raise AssertionError('Nondeleting Camera record differs')
            if owner.blocks or owner.registry or native.registry() or len(native.allocations)!=1:raise AssertionError('Camera lifecycle leaked')
            counts['camera_lifetimes']+=1
        local,assets=local_cases()
        synthetic=[]
        for i in range(120):
            pairs=[('name',f'camera-{i}'),('nodeId',str(i)),('translation','1 2 3'),('rotation',f'0 {i%60} 0'),('scale',('1 1 1' if i%2 else '2 0.5 3')),('dynamic','true'),('joint','true')]
            if i%3:pairs.extend([('fov',('45','1e2','')[i%3]),('nearClip','0.2'),('farClip','5000'),('orthographic',('true','TRUE','1','false')[i%4]),('orthographicHeight','30')])
            if i%2:pairs.reverse()
            pairs.append(('name','ignored'));synthetic.append(pairs)
        for case,pairs in enumerate(synthetic+local+[[('name',None),('name','ignored')],[('Name','ignored')],[('fov',None),('fov','90'),('name','null-fov')]]):
            owner=PortableServices(core);owner.sequence=case*20;put(native.base+0x1630d60,0,owner.sequence,'I');nodes=[[],[]];lookup={}
            used=N();warning=C.c_int();attached=C.c_uint32();source_ids={}
            def view():
                array=(P*len(nodes[1]))(*nodes[1]);kinds=(C.c_uint8*len(array))(*([1]*len(array)));return array,kinds,OwnedView(array,len(array),kinds,0)
            def operate(p,op):
                array,kinds,v=view();return core.fs15_scene_owned_trs(C.byref(v),lookup[p],op,None,0,None,0,C.byref(used),C.byref(warning))
            @REBUILD
            def rebuild(_,p):return operate(p,1)
            @REBUILD
            def refresh(_,p):return operate(p,0)
            @PROPAGATE
            def propagate(_,p,mask,origin):
                array,kinds,v=view();return core.fs15_scene_owned_state(C.byref(v),lookup[p],0,mask,origin,None,0,C.byref(used))
            @WORLD
            def world(_,p,out):
                array,kinds,v=view();position=(C.c_float*3)();ok=core.fs15_scene_owned_world(C.byref(v),lookup[p],position,3)
                if ok:C.memmove(out,p+0xd8,64)
                return ok
            @INTEGER
            def integer(_,text,out):out[0]=integer_native(text)&0xffffffff;return 1
            @REGISTER
            def registered(_,key,p):source_ids.setdefault(key,p);return 1
            @CREATED
            def created(_,p,kind,bytes):lookup[p]=len(nodes[1]);nodes[1].append(p);owner.kinds[p]=3;return 1
            @LINK
            def link(_,parent,child,position):
                array,kinds,v=view();return core.fs15_scene_owned_link(C.byref(v),lookup[parent],lookup[child],position,1,C.byref(owner.api),None,0,C.byref(used),C.byref(attached))
            @DESTROY
            def destroy(_,p):return core.fs15_scene_camera_destruct(p,0x190,1,C.byref(owner.api)) if read(p)==0x1404c1078 else core.fs15_scene_destruct(p,0x170,3,1,C.byref(owner.api))
            owner.api.destroy_child=destroy
            services=GroupServices(XmlServices(None,C.pointer(owner.api),rebuild,propagate,integer,registered),created,link);motion=CameraServices(None,world,refresh,rebuild)
            for i in range(2):
                a,b=allocate(0x170),owner.allocate(None,0x170);node_ctor(a,b'parent',0)
                if not core.fs15_scene_construct(b,0x170,3,b'parent',7,0,1,C.byref(owner.api)):raise AssertionError('Camera forest constructor failed')
                nodes[0].append(a);created(None,b,1,0x170)
            put(native.base+0x162cd18,0,nodes[0][0]);state(nodes[0][0],2,0);propagate(None,nodes[1][0],2,0)
            link_native(nodes[0][0],nodes[0][1],0xffffffff)
            if not link(None,nodes[1][0],nodes[1][1],0xffffffff):raise AssertionError('Camera parent attachment failed')
            translation=(C.c_float*3)(5,-2,1);rotation=(C.c_float*3)(0.1,0.2,-0.15);scale=(C.c_float*3)(1,1,0 if case%11==0 else 1);parent_matrix=(C.c_float*16)()
            core.fs15_compose_trs(translation,rotation,scale,parent_matrix,16);set_matrix(nodes[0][1],parent_matrix);state(nodes[0][1],1,0)
            array,kinds,v=view();core.fs15_scene_owned_matrix(C.byref(v),1,parent_matrix,16);propagate(None,nodes[1][1],1,0)
            loader=C.create_string_buffer(0x900);lp=C.addressof(loader);put(lp,0x580,nodes[0][1]);head=allocate(0x30)
            for offset in (0,8,16):put(head,offset,head)
            put(head,0x28,1,'B');put(head,0x29,1,'B');put(lp,0x120,head)
            current=P(nodes[1][1]);na,pa=attributes(pairs);begin(loader,None,na);result=Result()
            if not core.fs15_scene_camera_begin(C.byref(current),pa,len(pa),C.byref(services),C.byref(result)):raise AssertionError(f'Camera XML rejected {pairs}')
            if result.node:
                nodes[0].append(read(lp,0x580));counts['xml_cameras']+=1
                a,b=allocate(0x170),owner.allocate(None,0x170);node_ctor(a,b'child',0)
                if not core.fs15_scene_construct(b,0x170,3,b'child',6,0,1,C.byref(owner.api)):raise AssertionError('Camera child construction failed')
                nodes[0].append(a);created(None,b,1,0x170);link_native(nodes[0][2],a,0xffffffff)
                if not link(None,nodes[1][2],b,0xffffffff):raise AssertionError('Camera child attachment failed')
            else:counts['ignored_xml_cameras']+=1
            def compare():
                for i,(a,b) in enumerate(zip(*nodes)):
                    size=0x190 if i==2 else 0x170
                    av,bv=bytearray(canonical(C.string_at(a,size),3,nodes[0])),bytearray(canonical(C.string_at(b,size),3,nodes[1]))
                    for offset in (0x30,0x38,0x40):av[offset:offset+8]=bv[offset:offset+8]=bytes(8)
                    for offset,length in ((0x98,16),(0xd8,16),(0x148,9)):
                        x,y=struct.unpack_from('<'+'f'*length,av,offset),struct.unpack_from('<'+'f'*length,bv,offset)
                        if any(not math.isfinite(a) or not math.isfinite(b) or abs(a-b)>2e-5*max(1,abs(a)) for a,b in zip(x,y)):raise AssertionError(f'Camera transforms differ {case}/{i}/{offset:x}: {x} != {y}')
                        av[offset:offset+length*4]=bv[offset:offset+length*4]=bytes(length*4)
                    if av!=bv:raise AssertionError(f'Camera forest record differs {case}/{i}: {[hex(j) for j in range(size) if av[j]!=bv[j]][:16]}')
                    if C.string_at(read(a,0x10))!=C.string_at(read(b,0x10)):raise AssertionError('Camera name differs')
                    def vector(p,objects):
                        start,stop,cap=(read(p,o) for o in (0x30,0x38,0x40));mapping={p:i+1 for i,p in enumerate(objects)}
                        return stop-start,cap-start,tuple(mapping.get(read(start,j),read(start,j)) for j in range(0,cap-start,8))
                    if vector(a,nodes[0])!=vector(b,nodes[1]):raise AssertionError('Camera child vector allocation differs')
                    digest.update(av);counts['complete_forest_node_records']+=1
                if len(nodes[0])!=len(nodes[1]):raise AssertionError('Camera XML allocation count differs')
                counts['forest_states']+=1
            compare()
            if result.node:
                a,b=nodes[0][2],nodes[1][2]
                detached=case%7==0
                if detached:
                    unlink_native(nodes[0][1],0,0);array,kinds,v=view()
                    if not core.fs15_scene_owned_unlink(C.byref(v),1,0,0,None,0,C.byref(used)):raise AssertionError('Detach Camera for parentless pose failed')
                    counts['parentless_camera_forests']+=1
                for step in range(12):
                    target=(C.c_float*3)(*(rng.uniform(-30,30) for _ in range(3)));distance=C.c_float(rng.uniform(-20,20));alpha=C.c_float(0.1);factor=C.c_float(0.3)
                    if step==2:
                        # Exact/below-threshold smoothing must preserve the original
                        # origin point before the distance offset is applied.
                        m=world_native(a)
                        for j in range(3):target[j]=read(m,0x30+j*4,'f')
                        if case%2:target[0]+=0.001
                        counts['below_threshold_smoothing_inputs']+=1
                    if step%4==0:
                        matrix=(C.c_float*16)();angles=(C.c_float*3)(0.2,math.pi/2 if step==4 else step*0.03,-0.1);unit=(C.c_float*3)(1,1,1);core.fs15_compose_trs(target,angles,unit,matrix,16)
                        pose(a,matrix);ok=core.fs15_scene_camera_pose(b,matrix,16,C.byref(motion));counts['world_pose_updates']+=1
                    elif step%4==1:
                        follow(a,target,distance);ok=core.fs15_scene_camera_follow(b,target,3,distance,C.byref(motion));counts['follow_updates']+=1
                    elif step%4==2:
                        smooth(a,target,distance,alpha,factor);ok=core.fs15_scene_camera_smooth(b,target,3,distance,alpha,factor,C.byref(motion));counts['smoothed_updates']+=1
                    else:
                        frame=C.create_string_buffer(bytes([0xa5])*0x60,0x60)
                        for i,value in enumerate([*target,distance.value,alpha.value,factor.value]):put(C.addressof(frame),i*16,value,'f')
                        before=frame.raw;script_follow(a,frame);ok=core.fs15_scene_camera_script_follow(b,frame,len(frame),C.byref(motion))
                        if frame.raw!=before:raise AssertionError('Script input frame mutated')
                        counts['script_follow_updates']+=1
                    if not ok:raise AssertionError(f'Camera motion rejected {case}/{step}')
                    for p in nodes[0]:world_native(p)
                    for p in nodes[1]:
                        out=(C.c_float*16)()
                        if not world(None,p,C.addressof(out)):raise AssertionError('Portable camera world cache rejected')
                    compare()
                end(loader)
                if not core.fs15_scene_group_end(C.byref(current)) or current.value!=(None if detached else nodes[1][1]):raise AssertionError('Camera end parent stack differs')
                if detached:
                    deleting(a,1)
                    if not core.fs15_scene_camera_destruct(b,0x190,1,C.byref(owner.api)):raise AssertionError('Detached Camera teardown failed')
            node_delete(nodes[0][0],1)
            if not core.fs15_scene_destruct(nodes[1][0],0x170,3,1,C.byref(owner.api)):raise AssertionError('Camera forest teardown failed')
            def free_tree(p):
                if read(p,0x29,'B'):return
                free_tree(read(p));free_tree(read(p,16));release(p)
            free_tree(read(head,8));release(head)
            if owner.blocks or owner.registry or native.registry() or len(native.allocations)!=1:raise AssertionError('Camera XML forest ownership leaked')
            counts['forest_lifetimes']+=1
        script_follow(None,None)
        if not core.fs15_scene_camera_script_follow(None,None,0,None):raise AssertionError('Null script camera differs')
        put(native.base+0x162cd18,0,0)
    paths=sorted(p for p in (ROOT/'src/core').glob('*') if p.suffix in ('.cpp','.h'))
    report=dict(status='passed',reference_sha256=EXPECTED_IMAGE,native_entries=['0x140052540','0x1400b5870','0x1400b5910','0x1400b59f0','0x1400b5be0','0x1400b5d00','0x1400e8840','0x1400e8940','0x14018c430'],
        rebuilt_source_sha256={p.relative_to(ROOT).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in paths},tested_dll_sha256=hashlib.sha256(args.dll.read_bytes()).hexdigest(),comparisons=dict(counts),original_results_sha256=digest.hexdigest(),local_assets=assets,
        scope='Camera constructor/deleting wrapper/script creation, decoded Camera XML begin, world pose, Z-axis follow, smoothing and script frame adapter. Private unchanged native code, allocator/registry/dispatch seams authored. Normalization compared bit-exactly; full forest records preserve opaque bytes with 2e-5 transform/libm tolerance. Local original camera attribute sets execute in isolated synthetic parent fixtures, not complete original maps. Projection/rendering/culling, general clone, complete I3D loader and full engine startup remain open.')
    write_json(ROOT/'analysis/scene-camera-validation.json',report);print(json.dumps(report,indent=2))


if __name__=='__main__':main()
