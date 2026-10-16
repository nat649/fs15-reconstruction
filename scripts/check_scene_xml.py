"""Compare recovered common I3D attributes and detached property paths with GE6.

The original image executes unchanged in a private heap/FLS fixture. XML decoding,
bounded detached-node transport and registration callbacks are authored seams.
Derived mesh/light/camera construction and live physics are not exercised.
"""
import argparse
from collections import Counter
import ctypes as C
import hashlib
import json
from pathlib import Path
import random
import struct
import xml.etree.ElementTree as ET
from inventory import ROOT,write_json
from native_gar import EXPECTED_IMAGE
from native_animation_xml import NativeAnimationXml
from check_animation_xml import Attribute,attributes
from check_scene_lifecycle import PortableServices,Services,P,N,read,put,canonical
from check_scene_state import Arena,NONE

REBUILD=C.CFUNCTYPE(C.c_int,P,P)
PROPAGATE=C.CFUNCTYPE(C.c_int,P,P,C.c_uint32,C.c_int32)
INTEGER=C.CFUNCTYPE(C.c_int,P,C.c_char_p,C.POINTER(C.c_uint32))
REGISTER=C.CFUNCTYPE(C.c_int,P,C.c_uint32,P)
ENTRIES=[0x194460,0x18c100,0x127910,0x127d50,0x1279a0,0x1497e0,0x1498b0,0x149880,0x9e5b0,0x9e5f0,0x129720,0x299930]


class XmlServices(C.Structure):
    _fields_=[('user',P),('memory',C.POINTER(Services)),('rebuild',REBUILD),('propagate',PROPAGATE),('integer',INTEGER),('register_node',REGISTER)]


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--dll',type=Path,required=True);args=parser.parse_args()
    core=C.CDLL(str(args.dll.resolve()));rng=random.Random(0xf515194460);counts=Counter();digest=hashlib.sha256();assets=[]
    signatures={'fs15_scene_construct':[P,N,C.c_uint,C.c_char_p,N,C.c_uint32,C.c_uint8,C.POINTER(Services)],
        'fs15_scene_destruct':[P,N,C.c_uint,C.c_uint32,C.POINTER(Services)],
        'fs15_scene_property_construct':[C.c_uint,P,N,P],'fs15_scene_property_detached':[P],
        'fs15_scene_property_destruct_detached':[P,C.c_uint32,C.POINTER(Services)],
        'fs15_scene_property_flags':[P,C.c_uint32,C.c_int,P],'fs15_scene_property_density':[P,C.c_float,P],
        'fs15_scene_xml_find':[P,N,C.c_char_p,N,C.POINTER(P)],
        'fs15_scene_xml_apply':[P,P,N,C.c_int,C.c_int,C.c_int,C.c_char_p,N,C.POINTER(XmlServices)],
        'fs15_scene_trs_update':[C.POINTER(Arena),N,C.c_uint,P,N,P,N,C.POINTER(N),C.POINTER(C.c_int)],
        'fs15_scene_state_update':[C.POINTER(Arena),N,C.c_uint,C.c_uint32,C.c_int32,P,N,C.POINTER(N)]}
    for name,signature in signatures.items():getattr(core,name).argtypes=signature;getattr(core,name).restype=C.c_int
    with NativeAnimationXml() as native:
        allocate=lambda size:native.callbacks[0](native.heap,0,size)
        release=lambda p:native.callbacks[1](native.heap,0,p)
        ctor=C.WINFUNCTYPE(P,P,C.c_char_p,C.c_uint32)(native.base+0x10fa90)
        dtor=C.WINFUNCTYPE(None,P)(native.base+0x10fbb0)
        common=C.WINFUNCTYPE(None,P,P,P,C.c_uint8,C.c_uint8,C.c_uint8)(native.base+0x18c100)
        scalar_integer=C.WINFUNCTYPE(C.c_int32,C.c_char_p)(native.base+0x3fd784)
        lookup=C.WINFUNCTYPE(P,C.c_char_p,P)(native.base+0x299930)
        ctors=[C.WINFUNCTYPE(P,P,P)(native.base+entry) for entry in (0x127910,0x1497e0)]
        dtors=[C.WINFUNCTYPE(None,P)(native.base+entry) for entry in (0x127d50,0x1498b0)]
        deleting=[C.WINFUNCTYPE(P,P,C.c_uint32)(native.base+entry) for entry in (0x1279a0,0x149880)]
        enable=C.WINFUNCTYPE(None,P,C.c_uint32)(native.base+0x9e5b0);disable=C.WINFUNCTYPE(None,P,C.c_uint32)(native.base+0x9e5f0)
        density=C.WINFUNCTYPE(None,P,C.c_float)(native.base+0x129720)
        owner_node=C.create_string_buffer(0x170)
        def exact(a,b,size,offsets=()):
            aa,bb=bytearray(C.string_at(a,size)),bytearray(C.string_at(b,size))
            for offset in offsets:aa[offset:offset+8]=bb[offset:offset+8]=bytes(8)
            if aa!=bb:raise AssertionError(f'Property record differs: {[hex(i) for i in range(size) if aa[i]!=bb[i]][:20]}')
            digest.update(aa)
        # Leaf constructors, flags, density and both destructor entry shapes.
        for case in range(400):
            for kind,size in enumerate((0x88,0x90)):
                owner=PortableServices(core);a,b=allocate(size),owner.allocate(None,size)
                ctors[kind](a,owner_node)
                if not core.fs15_scene_property_construct(kind,b,size,owner_node):raise AssertionError('Property construction rejected')
                exact(a,b,size);counts['property_constructors']+=1
                if kind==0:
                    for op in range(4):
                        mask=rng.getrandbits(32);active=bool(op%2)
                        (enable if active else disable)(a,mask)
                        if not core.fs15_scene_property_flags(b,mask,active,None):raise AssertionError('Property flag update rejected')
                        exact(a,b,size);counts['property_flag_updates']+=1
                    # Keep backend inactive; an actor token alone exercises dirty
                    # bits without ever invoking manager/backend code.
                    for offset,value in ((8,0x1080c),(0x20,0),(0x30,case%2)):
                        put(a,offset,value,'I' if offset in (8,0x20) else 'Q');put(b,offset,value,'I' if offset in (8,0x20) else 'Q')
                    value=C.c_float((-1,0,0.00001,1,125.5)[case%5]);density(a,value)
                    if not core.fs15_scene_property_density(b,value,None):raise AssertionError('Property density rejected')
                    exact(a,b,size);counts['property_density_updates']+=1
                    for pointer in (a,b):put(pointer,8,0x80c,'I');put(pointer,0x30,0)
                if case%3==0:
                    dtors[kind](a)
                    if not core.fs15_scene_property_destruct_detached(b,0,C.byref(owner.api)):raise AssertionError('Detached destructor rejected')
                    exact(a,b,size);release(a);owner.release(None,b);counts['property_non_deleting_destructors']+=1
                else:
                    deleting[kind](a,1)
                    if not core.fs15_scene_property_destruct_detached(b,1,C.byref(owner.api)):raise AssertionError('Detached deleting destructor rejected')
                    if native.retired[a]!=owner.retired[b]:raise AssertionError('Deleted property bytes differ')
                    counts['property_deleting_destructors']+=1
                if owner.blocks:raise AssertionError('Property test leaked storage')
        cases=[]
        for case in range(640):
            pairs=[('name',('scene-node' if case%2 else 'long-node-name-'*3)),('translation',f'{case%17}  {case%11} -{case%7}'),
                ('rotation',f'{case%361} -90 45'),('scale',f'{case%3} 1.5 -2'),('visibility',('FALSE','true','0','falseX')[case%4]),
                ('oc',('false','true')[case%2]),('clipDistance',str(case-100)),('minClipDistance',str(case%15-2)),
                ('objectMask',('4294967295','-1','123tail','0x20')[case%4]),('lightMask',' 65535'),('nodeId',str(case%19)),('refId','777')]
            if case%5==0:pairs=[p for p in pairs if p[0] not in ('name','translation','rotation','scale','nodeId')]
            if case%5!=1:
                flag_values=rng.getrandbits(14)
                pairs.extend([(key,('true' if (flag_values>>bit)&1 else 'FALSE')) for bit,key in enumerate(('static','dynamic','kinematic','compound','compoundChild','collision','trigger','joint','projection','xAxisDrive','yAxisDrive','zAxisDrive','drivePos','breakableJoint'))])
                pairs.extend((key,str((case%21-3)/7)) for key in ('restitution','staticFriction','dynamicFriction','linearDamping','angularDamping','density','projDistance','projAngle','driveForceLimit','driveSpring','driveDamping','jointBreakForce','jointBreakTorque','rotMinX','rotMaxY','transMinZ','transMaxX'))
                pairs.extend([('collisionMask','-123'),('solverIterationCount',str(case%17))])
            pairs.extend([('name','ignored-duplicate'),('Name','case-sensitive')])
            cases.append((None,pairs,case%8))
        # Exercise the wrapper's empty scratch name and first-null duplicate,
        # rather than letting the usual duplicate-name fixture mask absence.
        cases[0]=(None,[],0)
        cases[1]=(None,[('name',None),('name','ignored'),('nodeId',None),('refId','123')],1)
        inventory=ROOT/'analysis/inventory.json'
        if inventory.is_file():
            game=Path(json.loads(inventory.read_text())['game_root'])
            paths=[game/'data/vehicles/tools/krone/kroneSwadro2000.i3d',ROOT/'recovered/gar/data/maps/map01.i3d',ROOT/'recovered/gar/data/maps/map02.i3d']
            for path in paths:
                if not path.is_file():continue
                scene=ET.parse(path).getroot().find('Scene')
                if scene is None:continue
                nodes=[e for e in scene.iter() if e is not scene and e.tag in ('TransformGroup','Shape','Camera','Light')]
                # Complete attribute list for a deterministic spread, including
                # actual scene geometry nodes as common-base metadata only.
                selected=nodes[::max(1,len(nodes)//450)][:500]
                cases.extend((path,list(e.attrib.items()),7) for e in selected)
                assets.append(dict(name=path.name,sha256=hashlib.sha256(path.read_bytes()).hexdigest(),scene_nodes=len(nodes),tested_common_attribute_records=len(selected)))
        owner=PortableServices(core);node_ids={}
        def arena(pointer):
            kind=C.c_uint8(1);view=Arena(pointer,0x170,0x170,None,0,None,0,read(pointer,0x118),1,C.addressof(kind),1,NONE)
            return kind,view
        @REBUILD
        def rebuild(_,pointer):
            kind,view=arena(pointer);used=N();warning=C.c_int()
            return core.fs15_scene_trs_update(C.byref(view),0,1,None,0,None,0,C.byref(used),C.byref(warning))
        @PROPAGATE
        def propagate(_,pointer,mask,origin):
            kind,view=arena(pointer);used=N()
            return core.fs15_scene_state_update(C.byref(view),0,0,mask,origin,None,0,C.byref(used))
        @INTEGER
        def integer(_,text,out):out[0]=scalar_integer(text)&0xffffffff;return 1
        @REGISTER
        def register(_,key,pointer):node_ids.setdefault(key,pointer);return 1
        services=XmlServices(None,C.pointer(owner.api),rebuild,propagate,integer,register)
        scene=C.create_string_buffer(0x900);loader=C.addressof(scene);head=allocate(0x30)
        for offset in (0,8,16):put(head,offset,head)
        put(head,0x28,1,'B');put(head,0x29,1,'B');put(loader,0x120,head)
        expected_original_ids={};expected_portable_ids={}
        def tree(pointer):
            if read(pointer,0x29,'B'):return {}
            return {**tree(read(pointer)),read(pointer,0x18,'I'):read(pointer,0x20),**tree(read(pointer,16))}
        for index,(path,pairs,switches) in enumerate(cases):
            owner.sequence=index*3;C.c_uint32.from_address(native.base+0x1630d60).value=owner.sequence
            a,b=allocate(0x170),owner.allocate(None,0x170);ctor(a,b'old',0)
            if not core.fs15_scene_construct(b,0x170,3,b'old',4,0,1,C.byref(owner.api)):raise AssertionError('Scene fixture constructor rejected')
            na,pa=attributes(pairs)
            for key in ('name','missing','Name','nodeId'):
                expected=lookup(key.encode(),na);actual=P()
                if not core.fs15_scene_xml_find(pa,len(pa),key.encode(),len(key)+1,C.byref(actual)):raise AssertionError('Attribute lookup rejected')
                if (C.string_at(expected) if expected else None)!=(C.string_at(actual) if actual else None):raise AssertionError('Attribute lookup differs')
                counts['attribute_lookups']+=1
            common(a,loader,na,switches&1,switches&2,switches&4)
            if not core.fs15_scene_xml_apply(b,pa,len(pa),switches&1,switches&2,switches&4,b'',1,C.byref(services)):raise AssertionError(f'Scene XML rejected {index}: {pairs}')
            aa,bb=bytearray(canonical(C.string_at(a,0x170),3)),bytearray(canonical(C.string_at(b,0x170),3))
            for offset,length in ((0x98,16),(0xd8,16),(0x148,9)):
                av,bv=struct.unpack_from('<'+'f'*length,aa,offset),struct.unpack_from('<'+'f'*length,bb,offset)
                if any(abs(x-y)>8e-6*max(1,abs(x)) for x,y in zip(av,bv)):raise AssertionError(f'Scene transform differs {index}: {av}/{bv}')
                aa[offset:offset+length*4]=bb[offset:offset+length*4]=bytes(length*4)
            digest.update(canonical(C.string_at(a,0x170),3))
            if aa!=bb:raise AssertionError(f'Scene non-float bytes differ {index}: {[hex(i) for i in range(0x170) if aa[i]!=bb[i]][:20]}')
            if C.string_at(read(a,0x10))!=C.string_at(read(b,0x10)):raise AssertionError('Scene name differs')
            p,q=read(a,0x118),read(b,0x118);table=read(p);size={0x1404c0ca8:0x10,0x1404c7a90:0x88,0x1404c8158:0x90}[table]
            exact(p,q,size,(0x28,) if size==0x88 else (0x20,) if size==0x90 else ())
            counts['complete_common_node_attribute_records']+=1;counts['property_kind_'+hex(size)]+=1
            if path:counts['local_asset_common_records']+=1
            values=dict(reversed(pairs));source=values.get('nodeId') if values.get('nodeId') is not None else values.get('refId')
            if switches&2 and source is not None:
                key=scalar_integer(source.encode())&0xffffffff
                expected_original_ids.setdefault(key,a);expected_portable_ids.setdefault(key,b)
            if tree(read(head,8))!=expected_original_ids or node_ids!=expected_portable_ids:raise AssertionError('Original first-ID-wins registration differs')
            # Unactivated trigger cleanup is outside the original TRS trigger
            # release branch. Compare metadata first, then disable on both sides.
            if size==0x88 and read(p,8,'I')&0x1000:
                disable(p,0x1000)
                if not core.fs15_scene_property_flags(q,0x1000,0,None):raise AssertionError('Trigger fixture cleanup flag failed')
            dtor(a)
            if not core.fs15_scene_destruct(b,0x170,3,0,C.byref(owner.api)):raise AssertionError('Scene lifetime cleanup rejected')
            release(a);owner.release(None,b);counts['complete_common_node_lifetimes']+=1
        counts['registered_source_ids']=len(node_ids)
        def free_tree(pointer):
            if read(pointer,0x29,'B'):return
            free_tree(read(pointer));free_tree(read(pointer,16));release(pointer)
        free_tree(read(head,8));release(head)
        if native.registry() or owner.registry or len(native.allocations)!=1 or owner.blocks:raise AssertionError('Scene XML leaked owned storage')
    paths=tuple('src/core/'+p for p in ('scene_xml.cpp','scene_xml.h','scene_properties.cpp','scene_properties.h','scene_lifecycle.cpp','scene_lifecycle.h','shared_scene.cpp','shared_scene.h','scene_state.cpp','scene_state.h','scene_trs.cpp','scene_trs.h','scene_math.cpp','scene_math.h','animation_xml.cpp','animation_xml.h'))
    report=dict(status='passed',reference_sha256=EXPECTED_IMAGE,native_entries=[f'0x140{entry:06x}' for entry in ENTRIES],
        rebuilt_source_sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in paths},tested_dll_sha256=hashlib.sha256(args.dll.read_bytes()).hexdigest(),
        comparisons=dict(counts),original_results_sha256=digest.hexdigest(),local_assets=assets,
        scope='Common I3D node attributes on detached constructed TRS nodes, pending rigid-body/joint metadata, leaf flag/density updates and detached destructor/deleting paths. Full byte comparisons except normalized pointers and bounded libm transform tolerance 8e-6. XML decoding, CRT conversion and node registration transport are authored; original registration first-ID-wins is checked. Derived render classes, LOD, tokenizer, hierarchy loading, active physics/observers and trigger teardown are open.')
    write_json(ROOT/'analysis/scene-xml-validation.json',report);print(json.dumps(report,indent=2))


if __name__=='__main__':main()
