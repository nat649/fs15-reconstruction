"""Native differential evidence for attribute/property/TRS and Camera/LOD copies."""
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
from check_scene_lifecycle import PortableServices,Services,DESTROY,P,N,read,put,canonical
from check_scene_attributes import tree
from check_scene_owned import OwnedView,Vector
from check_scene_groups import Result,CREATED
from check_scene_xml import XmlServices,REBUILD,PROPAGATE,INTEGER,REGISTER
from check_scene_lod import Context,Manager,LodServices,BASE,WORLD,MARK,Queue

class CloneServices(C.Structure):
    _fields_=[('scene',XmlServices),('refresh',REBUILD),('constructed',CREATED)]


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--dll',type=Path,required=True);args=parser.parse_args()
    core=C.CDLL(str(args.dll.resolve()));rng=random.Random(0xf51510fd80);counts=Counter();digest=hashlib.sha256()
    signatures={
        'fs15_scene_attribute_value_clone':[P,N,C.POINTER(Services),C.POINTER(P)],'fs15_scene_attributes_copy':[P,N,P,N,C.POINTER(Services)],
        'fs15_scene_attribute_ensure':[P,N,C.c_char_p,N,C.c_uint32,C.POINTER(Services),C.POINTER(P)],
        'fs15_scene_property_clone_owned':[P,P,C.POINTER(Services),C.POINTER(P)],'fs15_scene_property_construct':[C.c_uint,P,N,P],
        'fs15_scene_construct':[P,N,C.c_uint,C.c_char_p,N,C.c_uint32,C.c_uint8,C.POINTER(Services)],'fs15_scene_destruct':[P,N,C.c_uint,C.c_uint32,C.POINTER(Services)],
        'fs15_scene_camera_construct':[P,N,C.c_char_p,N,C.c_float,C.c_float,C.c_float,C.c_uint8,C.c_float,C.POINTER(Services)],'fs15_scene_camera_destruct':[P,N,C.c_uint32,C.POINTER(Services)],
        'fs15_scene_lod_construct':[P,N,C.c_char_p,N,P,C.c_uint32,C.POINTER(Services)],'fs15_scene_lod_destruct':[P,N,C.c_uint32,P,P],
        'fs15_scene_trs_copy':[P,P,C.POINTER(CloneServices)],'fs15_scene_clone_node':[P,C.POINTER(CloneServices),C.POINTER(Result)],
        'fs15_scene_owned_trs_lod':[P,N,C.c_uint,P,N,P,N,P,P,P],
        'fs15_scene_owned_state_lod':[P,N,C.c_uint,C.c_uint32,C.c_int32,P,N,P,P],
        'fs15_scene_owned_link_lod':[P,N,N,C.c_uint32,C.c_int,C.POINTER(Services),P,N,P,P,P],
        'fs15_scene_owned_world':[P,N,P,N],'fs15_scene_owned_mark_branch':[P],
        'fs15_scene_lod_enqueue':[P,P,C.POINTER(Services)],'fs15_scene_lod_update':[P,P,P],
        'fs15_scene_owned_matrix':[P,N,P,N],'fs15_compose_trs':[P,P,P,P,N]}
    for name,signature in signatures.items():getattr(core,name).argtypes=signature;getattr(core,name).restype=C.c_int
    with NativeAnimationXml() as native:
        allocate=lambda size:native.callbacks[0](native.heap,0,size)
        release=lambda p:native.callbacks[1](native.heap,0,p)
        ctor=C.WINFUNCTYPE(P,P,C.c_char_p,C.c_uint32)(native.base+0x10fa90)
        deleting=C.WINFUNCTYPE(P,P,C.c_uint32)(native.base+0x10fb10)
        value_clone=C.WINFUNCTYPE(P,P)(native.base+0x16eea0)
        copy_attributes=C.WINFUNCTYPE(None,P,P)(native.base+0x1366d0)
        ensure=C.WINFUNCTYPE(P,P,C.c_char_p,C.c_uint32)(native.base+0x136870)
        property_clones=[C.WINFUNCTYPE(P,P,P)(native.base+address) for address in (0xb1520,0x1279d0,0x149a60)]
        copy_trs=C.WINFUNCTYPE(None,P,P)(native.base+0x10fd80)
        camera_ctor=C.WINFUNCTYPE(P,P,C.c_char_p,C.c_float,C.c_float,C.c_float,C.c_uint8,C.c_float)(native.base+0xb5870)
        lod_ctor=C.WINFUNCTYPE(P,P,C.c_char_p,P,C.c_uint32)(native.base+0xd7a70)
        clones={1:C.WINFUNCTYPE(P,P)(native.base+0xb5950),2:C.WINFUNCTYPE(P,P)(native.base+0xd7ca0)}
        link=C.WINFUNCTYPE(None,P,P,C.c_uint32)(native.base+0xb17b0)
        state=C.WINFUNCTYPE(None,P,C.c_uint32,C.c_int32)(native.base+0xb1b00)
        world_native=C.WINFUNCTYPE(P,P)(native.base+0xb1a50)
        lod_update=C.WINFUNCTYPE(None,P)(native.base+0xd7ea0)
        property_delete=[C.WINFUNCTYPE(P,P,C.c_uint32)(native.base+address) for address in (0xb15a0,0x1279a0,0x149880)]
        C.WINFUNCTYPE(P)(native.base+0x524b0)()
        def same_value(a,b):
            x,y=bytearray(C.string_at(a,16)),bytearray(C.string_at(b,16))
            if read(a,8,'I') in (3,4):
                if C.string_at(read(a))!=C.string_at(read(b)) or read(a)==read(b):raise AssertionError('Cloned strings differ or alias')
                x[:8]=y[:8]=bytes(8)
            if x!=y:raise AssertionError('Cloned value bytes differ')
            digest.update(x)
        for case in range(1800):
            owner=PortableServices(core);kind=(0,1,2,3,4,42)[case%6];payload=bytearray(rng.randbytes(16));struct.pack_into('<I',payload,8,kind)
            text=C.create_string_buffer(('text-'+str(case)+'x'*(case%70)).encode())
            if kind in (3,4):struct.pack_into('<Q',payload,0,C.addressof(text))
            source=C.create_string_buffer(bytes(payload),16);a=value_clone(source);out=P()
            if not core.fs15_scene_attribute_value_clone(source,16,C.byref(owner.api),C.byref(out)):raise AssertionError('Value clone rejected')
            b=out.value;same_value(a,b)
            if kind in (3,4):release(read(a));owner.release(None,read(b))
            release(a);owner.release(None,b)
            if owner.blocks or len(native.allocations)!=1:raise AssertionError('Standalone value clone leaked')
            counts['complete_value_clones']+=1
        for kind in range(3):
            for case in range(300):
                owner=PortableServices(core);node=C.create_string_buffer(0x170);source=C.create_string_buffer(bytes([0xa5])*(0x10,0x88,0x90)[kind])
                pointer=C.addressof(source)
                if kind==0:put(pointer,0,0x1404c0ca8);put(pointer,8,rng.getrandbits(32),'I')
                else:
                    core.fs15_scene_property_construct(kind-1,source,len(source),node);put(pointer,8,rng.getrandbits(32),'I')
                    offsets=(list(range(0x50,0x84,4)) if kind==1 else list(range(0x34,0x48,4))+list(range(0x4c,0x64,4))+list(range(0x68,0x80,4))+[0x84,0x88])
                    for at in offsets:put(pointer,at,rng.uniform(-3,3),'f')
                    if kind==1:put(pointer,0x80,rng.getrandbits(32),'I')
                    else:
                        for at in (0x48,0x49,0x4a,0x64,0x65,0x66,0x80):put(pointer,at,rng.randrange(256),'B')
                a=property_clones[kind](source,node);out=P()
                if not core.fs15_scene_property_clone_owned(source,node,C.byref(owner.api),C.byref(out)):raise AssertionError('Property clone rejected')
                b=out.value;size=(0x10,0x88,0x90)[kind]
                if C.string_at(a,size)!=C.string_at(b,size):raise AssertionError(f'Property clone differs kind {kind}: {[hex(i) for i in range(size) if C.string_at(a,size)[i]!=C.string_at(b,size)[i]][:12]}')
                normalized=bytearray(C.string_at(a,size))
                if kind:struct.pack_into('<Q',normalized,0x28 if kind==1 else 0x20,1)
                digest.update(normalized);property_delete[kind](a,1)
                # New records have no backend or registered observer ownership.
                owner.release(None,b)
                if owner.blocks or len(native.allocations)!=1:raise AssertionError('Property clone leaked')
                counts['complete_property_clones']+=1
        def fill_attributes(a,b,owner,serial,count):
            for i in rng.sample(list(range(count)),count):
                key=(f'{i:03}-'+('x'*(i%40))).encode();kind=i%6;kind=42 if kind==5 else kind
                p=ensure(a+0x78,key,kind);out=P()
                if not core.fs15_scene_attribute_ensure(b,0x170,key,len(key)+1,kind,C.byref(owner.api),C.byref(out)):raise AssertionError('Clone source attribute ensure failed')
                q=out.value;payload=rng.getrandbits(64)
                if kind in (3,4):
                    text=f'text-{serial}-{i}'.encode()+b'\0';x,y=allocate(len(text)),owner.allocate(None,len(text));C.memmove(x,text,len(text));C.memmove(y,text,len(text));put(p,0,x);put(q,0,y)
                else:put(p,0,payload);put(q,0,payload)
        def retained(pointer):
            head=read(pointer,0x80);pending=[read(head,8)];result=[]
            while pending:
                p=pending.pop()
                if p==head:continue
                value=read(p,0x40)
                if value and read(value,8,'I')==4:result.append(read(value))
                pending.extend([read(p),read(p,16)])
            return result
        def compare_tree(a,b):
            x,y=tree(a),tree(b)
            if x!=y:
                key=next((k for k in x if x[k]!=y.get(k)),None)
                raise AssertionError(f'Cloned attribute tree differs at {key}: {x.get(key)} != {y.get(key)}')
            counts['complete_copied_attribute_trees']+=1
            for key,row in sorted(x.items()):digest.update(key);digest.update(row[0]);digest.update(row[2] or b'')
        for case in range(90):
            owner=PortableServices(core);owner.sequence=case*10;put(native.base+0x1630d60,0,owner.sequence,'I');nodes=[[],[]];orphans=[set(),set()]
            for i in range(2):
                a,b=allocate(0x170),owner.allocate(None,0x170);ctor(a,b'attributes',0)
                if not core.fs15_scene_construct(b,0x170,3,b'attributes',11,0,1,C.byref(owner.api)):raise AssertionError('Map fixture constructor rejected')
                nodes[0].append(a);nodes[1].append(b);fill_attributes(a,b,owner,i,4+(case+i)%35)
            for step in range(3):
                a,b=nodes[0][1],nodes[1][1]
                for side in range(2):orphans[side].update(retained(nodes[side][1]))
                copy_attributes(a+0x78,nodes[0][0]+0x78)
                if not core.fs15_scene_attributes_copy(b,0x170,nodes[1][0],0x170,C.byref(owner.api)):raise AssertionError('Map copy rejected')
                compare_tree(a,b);counts['map_assignments']+=1
            for side in range(2):orphans[side].update(retained(nodes[side][1]))
            copy_attributes(nodes[0][1]+0x78,nodes[0][1]+0x78)
            if not core.fs15_scene_attributes_copy(nodes[1][1],0x170,nodes[1][1],0x170,C.byref(owner.api)):raise AssertionError('Self map copy rejected')
            compare_tree(nodes[0][1],nodes[1][1]);counts['self_map_assignments']+=1
            for a,b in zip(*nodes):
                orphans[0].update(retained(a));orphans[1].update(retained(b));deleting(a,1)
                if not core.fs15_scene_destruct(b,0x170,3,1,C.byref(owner.api)):raise AssertionError('Map fixture teardown failed')
            for p in orphans[0]:
                if p not in native.allocations:raise AssertionError('Native kind-four text retention changed')
                release(p)
            for p in orphans[1]:owner.release(None,p)
            if owner.blocks or owner.registry or native.registry() or len(native.allocations)!=1:raise AssertionError('Attribute clone map lifetime leaked')
            counts['attribute_map_lifetimes']+=1;counts['native_retained_kind_four_texts']+=len(orphans[0])
        manager=native.base+0x162cc78;pending=native.base+0x162ccf8
        for case in range(90):
            owner=PortableServices(core);owner.sequence=case*100;put(native.base+0x1630d60,0,owner.sequence,'I');nodes=[[],[]];kinds=[];sizes=[];lookup={};context=Context();orphans=[set(),set()]
            C.memset(manager,0,0x20);C.memset(pending,0,24);requests=(N*1024)();queue=Queue(requests,len(requests),0);used=N();warning=C.c_int();attached=C.c_uint32()
            def view():
                array=(P*len(nodes[1]))(*nodes[1]);kind=(C.c_uint8*len(kinds))(*kinds);return array,kind,OwnedView(array,len(array),kind,0)
            def enqueue():
                for i in range(queue.count):
                    if not core.fs15_scene_lod_enqueue(C.byref(context),nodes[1][requests[i]],C.byref(owner.api)):raise AssertionError('Clone LOD enqueue failed')
                queue.count=0
            @REBUILD
            def rebuild(_,p):
                array,kind,v=view();ok=core.fs15_scene_owned_trs_lod(C.byref(v),lookup[p],1,None,0,None,0,C.byref(used),C.byref(warning),C.byref(queue));enqueue();return ok
            @REBUILD
            def refresh(_,p):
                array,kind,v=view();return core.fs15_scene_owned_trs_lod(C.byref(v),lookup[p],0,None,0,None,0,C.byref(used),C.byref(warning),C.byref(queue))
            @PROPAGATE
            def propagate(_,p,mask,origin):
                array,kind,v=view();ok=core.fs15_scene_owned_state_lod(C.byref(v),lookup[p],0,mask,origin,None,0,C.byref(used),C.byref(queue));enqueue();return ok
            @CREATED
            def created(_,p,kind,size):lookup[p]=len(nodes[1]);nodes[1].append(p);kinds.append(kind);sizes.append(size);owner.kinds[p]=3;return 1
            @WORLD
            def world(_,p,out):
                array,kind,v=view();return core.fs15_scene_owned_world(C.byref(v),lookup[p],out,3)
            @MARK
            def mark(_,p,token):return core.fs15_scene_owned_mark_branch(p)
            lod_services=LodServices(None,C.pointer(owner.api),BASE(),world,mark)
            @DESTROY
            def destroy(_,p):
                if read(p)==0x1404c1c48:return core.fs15_scene_lod_destruct(p,0x188,1,C.byref(context),C.byref(lod_services))
                if read(p)==0x1404c1078:return core.fs15_scene_camera_destruct(p,0x190,1,C.byref(owner.api))
                return core.fs15_scene_destruct(p,0x170,3,1,C.byref(owner.api))
            owner.api.destroy_child=destroy;services=CloneServices(XmlServices(None,C.pointer(owner.api),rebuild,propagate,INTEGER(),REGISTER()),refresh,created)
            for i in range(4):
                variant=(case+i)%3 if i in (1,2) else 0;size=(0x170,0x190,0x188)[variant];a,b=allocate(size),owner.allocate(None,size);name=f'node-{i}'.encode()
                if variant==1:
                    camera_ctor(a,name,55,0.1,1000,case%2,3);ok=core.fs15_scene_camera_construct(b,size,name,len(name)+1,55,0.1,1000,case%2,3,C.byref(owner.api))
                elif variant==2:
                    distances=(C.c_float*4)(*[(0,10,20,30),(0,100,100,100),(5,0,0,0),(0,10,10,100)][case%4]);lod_ctor(a,name,distances,4);ok=core.fs15_scene_lod_construct(b,size,name,len(name)+1,distances,4,C.byref(owner.api))
                else:ctor(a,name,0);ok=core.fs15_scene_construct(b,size,3,name,len(name)+1,0,1,C.byref(owner.api))
                if not ok:raise AssertionError('Clone scene constructor rejected')
                nodes[0].append(a);created(None,b,2 if variant==2 else 1,size)
            put(native.base+0x162cd18,0,nodes[0][0]);state(nodes[0][0],2,0);propagate(None,nodes[1][0],2,0)
            for parent,child in ((0,1),(0,2),(2,3)):
                link(nodes[0][parent],nodes[0][child],0xffffffff);array,kind,v=view()
                if not core.fs15_scene_owned_link_lod(C.byref(v),parent,child,0xffffffff,1,C.byref(owner.api),None,0,C.byref(used),C.byref(attached),C.byref(queue)):raise AssertionError('Clone fixture linking failed')
                enqueue()
            source=[nodes[0][1],nodes[1][1]];target=[nodes[0][2],nodes[1][2]]
            fill_attributes(*source,owner,case,5+case%15);fill_attributes(*target,owner,case+1,3)
            # Source transform cache is deliberately dirty; the native copy must
            # refresh it before using the nine cached TRS lanes.
            translation=(C.c_float*3)(case%7,-3,5);rotation=(C.c_float*3)(0.1,0.3,-0.2);scale=(C.c_float*3)(2,0.5,1);matrix=(C.c_float*16)();core.fs15_compose_trs(translation,rotation,scale,matrix,16)
            C.WINFUNCTYPE(None,P,P)(native.base+0x10fce0)(source[0],matrix);array,kind,v=view();core.fs15_scene_owned_matrix(C.byref(v),1,matrix,16)
            for p in source:
                put(p,0x50,(read(p,0x50,'I')|0x24)&~1,'I');put(p,0x128,300.0,'f');put(p,0x12c,5.0,'f');put(p,0x130,0x87654321,'I');put(p,0x134,0x12345678,'I')
            property_kind=case%3
            if property_kind:
                for side,p in enumerate(source):
                    old=read(p,0x118);size=(0x88,0x90)[property_kind-1];q=allocate(size) if side==0 else owner.allocate(None,size)
                    core.fs15_scene_property_construct(property_kind-1,q,size,p);put(p,0x118,q)
                    if side==0:release(old)
                    else:owner.release(None,old)
                    if property_kind==1:put(q,0x68,0.2,'f');put(q,8,(read(q,8,'I')|0x8000)&~0x1000,'I');put(q,0x70,1.5,'f')
                    else:put(q,0x4c,-0.5,'f');put(q,0x58,0.5,'f');put(q,0x48,1,'B')
            def compare():
                for i,(a,b) in enumerate(zip(*nodes)):
                    x,y=bytearray(canonical(C.string_at(a,sizes[i]),3,nodes[0])),bytearray(canonical(C.string_at(b,sizes[i]),3,nodes[1]))
                    for at in (0x30,0x38,0x40):x[at:at+8]=y[at:at+8]=bytes(8)
                    for at,length in ((0x98,16),(0xd8,16),(0x148,9)):
                        av,bv=struct.unpack_from('<'+'f'*length,x,at),struct.unpack_from('<'+'f'*length,y,at)
                        if any(not math.isfinite(a) or not math.isfinite(b) or abs(a-b)>1e-5*max(1,abs(a)) for a,b in zip(av,bv)):raise AssertionError('Cloned scene transform differs')
                        x[at:at+length*4]=y[at:at+length*4]=bytes(length*4)
                    if x!=y:raise AssertionError(f'Cloned complete scene record differs {case}/{i}: {[hex(j) for j in range(sizes[i]) if x[j]!=y[j]][:16]}')
                    if C.string_at(read(a,0x10))!=C.string_at(read(b,0x10)):raise AssertionError('Cloned node name differs')
                    def child_vector(p,objects):
                        start,stop,cap=(read(p,o) for o in (0x30,0x38,0x40));mapping={p:i+1 for i,p in enumerate(objects)}
                        return stop-start,cap-start,tuple(mapping.get(read(start,j),read(start,j)) for j in range(0,cap-start,8))
                    if child_vector(a,nodes[0])!=child_vector(b,nodes[1]):raise AssertionError('Cloned child-vector contents/capacity differ')
                    pa,pb=read(a,0x118),read(b,0x118);ps={0x1404c0ca8:0x10,0x1404c7a90:0x88,0x1404c8158:0x90}[read(pa)];av,bv=bytearray(C.string_at(pa,ps)),bytearray(C.string_at(pb,ps))
                    if ps>0x10:at=0x28 if ps==0x88 else 0x20;av[at:at+8]=bv[at:at+8]=bytes(8)
                    if av!=bv:raise AssertionError('Scene copy property bytes differ')
                    compare_tree(a,b);digest.update(x);digest.update(av);counts['complete_copied_scene_records']+=1
                maps=[{p:i+1 for i,p in enumerate(side)} for side in nodes]
                def pending_view(p,mapping):
                    start,stop,cap=(read(p,o) for o in (0,8,16));return stop-start,cap-start,tuple(mapping.get(read(start,j),read(start,j)) for j in range(0,cap-start,8))
                if pending_view(pending,maps[0])!=pending_view(C.addressof(context)+0x20,maps[1]):raise AssertionError('Clone LOD queue differs')
                counts['complete_clone_forest_states']+=1
            compare()
            for step in range(3):
                for side in range(2):orphans[side].update(retained(target[side]))
                copy_trs(target[0],source[0])
                if not core.fs15_scene_trs_copy(target[1],source[1],C.byref(services)):raise AssertionError('TRS copy rejected')
                compare();counts['existing_node_copies']+=1
            variant=1 if read(source[0])==0x1404c1078 else 2 if read(source[0])==0x1404c1c48 else 0
            if variant:
                a=clones[variant](source[0]);result=Result()
                if not core.fs15_scene_clone_node(source[1],C.byref(services),C.byref(result)):raise AssertionError('Derived clone rejected')
                nodes[0].append(a);compare();counts['camera_clone_wrappers' if variant==1 else 'lod_clone_wrappers']+=1
            for side in range(2):orphans[side].update(retained(target[side]))
            copy_trs(target[0],target[0])
            if not core.fs15_scene_trs_copy(target[1],target[1],C.byref(services)):raise AssertionError('Self TRS copy rejected')
            compare();counts['self_node_copies']+=1
            # Apply queued updates and compare all world caches after copying.
            original_pending=Vector.from_address(pending)
            for i in range((original_pending.end-original_pending.begin)//8):
                lod_update(read(original_pending.begin,i*8))
                if not core.fs15_scene_lod_update(read(context.pending.begin,i*8),C.byref(context),C.byref(lod_services)):raise AssertionError('Clone LOD update failed')
            original_pending.end=original_pending.begin;context.pending.end=context.pending.begin
            for a,b in zip(*nodes):
                world_native(a);position=(C.c_float*3)()
                if not world(None,b,position):raise AssertionError('Cloned world cache rejected')
                orphans[0].update(retained(a));orphans[1].update(retained(b))
            compare()
            ma,mb=Manager.from_address(manager),context.manager
            ah,bh=bytearray(C.string_at(manager,0x20)),bytearray(bytes(mb));ah[:8]=bh[:8]=bytes(8)
            if ah!=bh:raise AssertionError('Cloned LOD manager header differs')
            av,bv=bytearray(C.string_at(ma.entries,ma.capacity*0x28) if ma.entries else b''),bytearray(C.string_at(mb.entries,mb.capacity*0x28) if mb.entries else b'')
            for i in range(ma.capacity):
                for payload,objects in ((av,nodes[0]),(bv,nodes[1])):
                    mapping={p:i+1 for i,p in enumerate(objects)};value=struct.unpack_from('<Q',payload,i*0x28)[0];struct.pack_into('<Q',payload,i*0x28,mapping.get(value,value))
                if i<ma.count:
                    x,y=struct.unpack_from('<3f',av,i*0x28+8),struct.unpack_from('<3f',bv,i*0x28+8)
                    if any(abs(a-b)>1e-5*max(1,abs(a)) for a,b in zip(x,y)):raise AssertionError('Cloned LOD manager world position differs')
                    av[i*0x28+8:i*0x28+20]=bv[i*0x28+8:i*0x28+20]=bytes(12)
            if av!=bv:raise AssertionError('Cloned LOD manager complete allocation differs')
            digest.update(ah);digest.update(av);counts['complete_clone_lod_manager_allocations']+=1
            roots=[(a,b) for a,b in zip(*nodes) if not read(a,0x28)]
            for a,b in roots:
                if read(a)==0x1404c1c48:C.WINFUNCTYPE(P,P,C.c_uint32)(native.base+0xd7bb0)(a,1)
                elif read(a)==0x1404c1078:C.WINFUNCTYPE(P,P,C.c_uint32)(native.base+0xb5910)(a,1)
                else:deleting(a,1)
                if not destroy(None,b):raise AssertionError('Clone forest teardown failed')
            for p in orphans[0]:
                if p not in native.allocations:raise AssertionError('Scene kind-four retention changed')
                release(p)
            for p in orphans[1]:owner.release(None,p)
            for p in (read(manager),read(pending)):
                if p:release(p)
            for p in (context.manager.entries,context.pending.begin):
                if p:owner.release(None,p)
            if owner.blocks or owner.registry or native.registry() or len(native.allocations)!=1:raise AssertionError(f'Clone forest lifetime leaked case {case}: portable sizes {[len(b) for b in owner.blocks.values()]}, registry {list(owner.registry)}, native sizes {list(native.allocations.values())}, registry {list(native.registry())}')
            counts['complete_clone_forest_lifetimes']+=1
        put(native.base+0x162cd18,0,0);C.memset(manager,0,0x20);C.memset(pending,0,24)
    paths=sorted(p for p in (ROOT/'src/core').glob('*') if p.suffix in ('.cpp','.h'))
    report=dict(status='passed',reference_sha256=EXPECTED_IMAGE,native_entries=['0x14016eea0','0x1401366d0','0x1401279d0','0x140149a60','0x14010fd80','0x1400b5950','0x1400d7ca0'],rebuilt_source_sha256={p.relative_to(ROOT).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in paths},tested_dll_sha256=hashlib.sha256(args.dll.read_bytes()).hexdigest(),comparisons=dict(counts),original_results_sha256=digest.hexdigest(),
        scope='Known attribute values/map assignment, default/body/joint property metadata cloning, supported TRS copy and Camera/LOD single-record clone wrappers. Original image code unchanged in private heap reference. Native retained kind-four text allocations tracked and released explicitly after comparison. Full maps/properties/records compared, pointers normalized, libm tolerance 1e-5. Authoring covers owned callback transport, finite/allocation guards and failure cleanup. Temporary CRT string allocations, exception unwinding, arbitrary derived clone, original recursive remap tables, live physics actors, shared-animation rebinding and full loader/startup remain open.')
    write_json(ROOT/'analysis/scene-clone-validation.json',report);print(json.dumps(report,indent=2))


if __name__=='__main__':main()
