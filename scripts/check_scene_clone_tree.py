"""Compare native pointer remap maps and recursive supported scene cloning."""
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
from check_scene_groups import GroupServices,Result,CREATED,LINK,asset_cases
from check_scene_clone import CloneServices
from check_scene_xml import XmlServices,REBUILD,PROPAGATE,INTEGER,REGISTER
from check_animation_xml import attributes
from check_scene_lod import Context,Manager,LodServices,BASE,WORLD,MARK,Queue

class Remap(C.Structure):
    _fields_=[('unknown',N),('head',N),('count',N)]
class TreeServices(C.Structure):
    _fields_=[('clone',CloneServices),('link',LINK)]


def entries(mapping):
    result={};pending=[read(mapping.head,8)]
    while pending:
        p=pending.pop()
        if p==mapping.head:continue
        key=read(p,0x18)
        if key in result:raise AssertionError('Duplicate/cyclic remap entry')
        result[key]=p;pending.extend([read(p),read(p,16)])
    if len(result)!=mapping.count:raise AssertionError('Remap count differs')
    return result


def snapshot(mapping):
    values=entries(mapping);roles={p:key for key,p in values.items()};roles[mapping.head]='#sentinel';result={}
    for p,key in roles.items():
        payload=bytearray(C.string_at(p,0x30));links=tuple(roles[read(p,o)] for o in (0,8,16));payload[:24]=bytes(24);result[key]=(bytes(payload),links)
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--dll',type=Path,required=True);args=parser.parse_args()
    core=C.CDLL(str(args.dll.resolve()));rng=random.Random(0xf5151b9d70);counts=Counter();digest=hashlib.sha256()
    signatures={
        'fs15_scene_clone_map_init':[P,C.POINTER(Services)],'fs15_scene_clone_map_clear':[P,C.c_int,C.POINTER(Services)],
        'fs15_scene_clone_map_find':[P,N,C.POINTER(P)],'fs15_scene_clone_map_node':[P,N,N,C.POINTER(Services),C.POINTER(P)],
        'fs15_scene_clone_map_insert':[P,P,C.POINTER(Services),C.POINTER(P),P],
        'fs15_scene_clone_vector_reserve':[P,N,C.POINTER(Services)],'fs15_scene_clone_vector_grow':[P,C.POINTER(Services)],
        'fs15_scene_clone_tree':[P,P,P,P,C.POINTER(Result),C.POINTER(Result)],
        'fs15_scene_construct':[P,N,C.c_uint,C.c_char_p,N,C.c_uint32,C.c_uint8,C.POINTER(Services)],'fs15_scene_destruct':[P,N,C.c_uint,C.c_uint32,C.POINTER(Services)],
        'fs15_scene_group_begin':[P,P,N,P,P],'fs15_scene_group_end':[P],'fs15_scene_camera_begin':[P,P,N,P,P],
        'fs15_scene_owned_trs_lod':[P,N,C.c_uint,P,N,P,N,P,P,P],
        'fs15_scene_owned_state_lod':[P,N,C.c_uint,C.c_uint32,C.c_int32,P,N,P,P],
        'fs15_scene_owned_link_lod':[P,N,N,C.c_uint32,C.c_int,C.POINTER(Services),P,N,P,P,P],
        'fs15_scene_owned_world':[P,N,P,N],'fs15_scene_owned_mark_branch':[P],
        'fs15_scene_lod_enqueue':[P,P,C.POINTER(Services)],'fs15_scene_lod_update':[P,P,P],'fs15_scene_lod_destruct':[P,N,C.c_uint32,P,P],
        'fs15_scene_camera_destruct':[P,N,C.c_uint32,C.POINTER(Services)],'fs15_scene_property_flags':[P,C.c_uint32,C.c_int,P],
        'fs15_scene_attribute_ensure':[P,N,C.c_char_p,N,C.c_uint32,C.POINTER(Services),C.POINTER(P)]}
    for name,signature in signatures.items():getattr(core,name).argtypes=signature;getattr(core,name).restype=C.c_int
    with NativeAnimationXml() as native:
        allocate=lambda size:native.callbacks[0](native.heap,0,size)
        release=lambda p:native.callbacks[1](native.heap,0,p)
        factory=C.WINFUNCTYPE(P,P,P)(native.base+0x1ba050);insert=C.WINFUNCTYPE(P,P,P,P)(native.base+0x16ea00)
        free_nodes=C.WINFUNCTYPE(None,P,P)(native.base+0x96190)
        reserve=C.WINFUNCTYPE(None,P,N)(native.base+0x88360);grow=C.WINFUNCTYPE(None,P)(native.base+0x320740)
        recursive=C.WINFUNCTYPE(P,P,P,P,P,P)(native.base+0x1b9d70)
        ctor=C.WINFUNCTYPE(P,P,C.c_char_p,C.c_uint32)(native.base+0x10fa90);deleting=C.WINFUNCTYPE(P,P,C.c_uint32)(native.base+0x10fb10)
        begin_group=C.WINFUNCTYPE(None,P,P,P)(native.base+0x18c1a0);begin_camera=C.WINFUNCTYPE(None,P,P,P)(native.base+0x18c430);end=C.WINFUNCTYPE(None,P)(native.base+0x18c370)
        state=C.WINFUNCTYPE(None,P,C.c_uint32,C.c_int32)(native.base+0xb1b00);world_native=C.WINFUNCTYPE(P,P)(native.base+0xb1a50)
        integer_native=C.WINFUNCTYPE(C.c_int32,C.c_char_p)(native.base+0x3fd784);lod_update=C.WINFUNCTYPE(None,P)(native.base+0xd7ea0)
        ensure=C.WINFUNCTYPE(P,P,C.c_char_p,C.c_uint32)(native.base+0x136870);disable=C.WINFUNCTYPE(None,P,C.c_uint32)(native.base+0x9e5f0)
        C.WINFUNCTYPE(P)(native.base+0x524b0)()
        def init(owner):
            a=Remap(0x123456789abcdef0,allocate(0x30),0);b=Remap(0x123456789abcdef0,0,0)
            for at in (0,8,16):put(a.head,at,a.head)
            put(a.head,0x28,1,'B');put(a.head,0x29,1,'B')
            if not core.fs15_scene_clone_map_init(C.byref(b),C.byref(owner.api)):raise AssertionError('Remap init rejected')
            return a,b
        def clear(a,b,owner):
            roles=[{p:key for key,p in entries(m).items()} for m in (a,b)];start=[len(native.freed),len(owner.release_order)]
            free_nodes(C.byref(a),read(a.head,8))
            if not core.fs15_scene_clone_map_clear(C.byref(b),0,C.byref(owner.api)):raise AssertionError('Remap clear rejected')
            x=[roles[0][p] for p in native.freed[start[0]:]];y=[roles[1][p] for p in owner.release_order[start[1]:]]
            # Actual source pointer ordering can differ across the two heaps.
            # Each native tree frees in descending unsigned-key order.
            if x!=sorted(roles[0].values(),reverse=True) or y!=sorted(roles[1].values(),reverse=True):raise AssertionError('Remap subtree free order differs')
            for at in (0,8,16):put(a.head,at,a.head)
            a.count=0
            if snapshot(a)!=snapshot(b):raise AssertionError('Remap cleared sentinel differs')
            release(a.head)
            if not core.fs15_scene_clone_map_clear(C.byref(b),1,C.byref(owner.api)):raise AssertionError('Remap sentinel release rejected')
        for case in range(120):
            owner=PortableServices(core);a,b=init(owner);keys=[0,1,0xffffffffffffffff,0x8000000000000000,*[rng.getrandbits(64) for _ in range(24+case%35)]];rng.shuffle(keys)
            for step,key in enumerate(keys+[keys[case%len(keys)],keys[0]]):
                value=rng.getrandbits(64);pair=(N*2)(key,value);p=factory(C.byref(a),pair);q=P()
                if not core.fs15_scene_clone_map_node(C.byref(b),key,value,C.byref(owner.api),C.byref(q)):raise AssertionError('Remap node factory rejected')
                x,y=bytearray(C.string_at(p,0x30)),bytearray(C.string_at(q.value,0x30));x[:24]=y[:24]=bytes(24)
                if x!=y:raise AssertionError('Remap factory complete record differs')
                counts['remap_factory_records']+=1;out=C.create_string_buffer(bytes([0xa5])*16,16);insert(C.byref(a),out,p);entry=P();added=C.c_int()
                if not core.fs15_scene_clone_map_insert(C.byref(b),q,C.byref(owner.api),C.byref(entry),C.byref(added)):raise AssertionError('Remap insertion rejected')
                if read(C.addressof(out),8,'B')!=added.value or read(read(C.addressof(out)),0x18)!=read(entry.value,0x18):raise AssertionError('Remap insert result differs')
                if snapshot(a)!=snapshot(b):raise AssertionError('Remap RB topology/opaque bytes differ')
                for query in (key,key^0x55aa55aa55aa55aa):
                    found=P()
                    if not core.fs15_scene_clone_map_find(C.byref(b),query,C.byref(found)) or (read(found.value,0x20) if found.value else None)!=(read(entries(a)[query],0x20) if query in entries(a) else None):raise AssertionError('Remap lookup differs')
                counts['complete_remap_states']+=1;counts['remap_insertions' if added.value else 'duplicate_remap_insertions']+=1;digest.update(x)
            clear(a,b,owner)
            if owner.blocks or len(native.allocations)!=1:raise AssertionError('Remap lifetime leaked')
            counts['complete_remap_lifetimes']+=1
        for case in range(100):
            owner=PortableServices(core);a,b=Vector(),Vector()
            for step in range(30):
                if step%3==0:
                    wanted=(a.end-a.begin)//8+step%7;reserve(C.byref(a),wanted);ok=core.fs15_scene_clone_vector_reserve(C.byref(b),wanted,C.byref(owner.api));counts['clone_vector_reserves']+=1
                else:
                    grow(C.byref(a));ok=core.fs15_scene_clone_vector_grow(C.byref(b),C.byref(owner.api));counts['clone_vector_grows']+=1
                    if ok:
                        value=rng.getrandbits(64);put(a.end,0,value);put(b.end,0,value);a.end+=8;b.end+=8
                if not ok or (a.end-a.begin,a.capacity-a.begin,C.string_at(a.begin,a.capacity-a.begin) if a.begin else b'')!=(b.end-b.begin,b.capacity-b.begin,C.string_at(b.begin,b.capacity-b.begin) if b.begin else b''):raise AssertionError('Clone vector allocation bytes differ')
                digest.update(C.string_at(a.begin,a.capacity-a.begin) if a.begin else b'');counts['complete_clone_vector_states']+=1
            if a.begin:release(a.begin)
            if b.begin:owner.release(None,b.begin)
        local,assets=asset_cases();cases=[]
        for case in range(36):
            events=[]
            def group(depth,serial):
                tag='Camera' if depth>0 and serial%4==0 else 'TransformGroup'
                pairs=[('name',f'node-{serial}'),('nodeId',str(serial)),('translation',f'{serial%9} {depth} {-serial%5}'),('rotation',f'0 {serial*7%75} 0'),('scale','1 1 1'),('clipDistance',str(100+serial%30))]
                if tag=='Camera':pairs.extend([('fov','55'),('nearClip','0.2'),('farClip','1000')])
                elif serial%3==0:pairs.append(('lodDistance','0 10 20 30'))
                if serial%5==0 and tag!='Camera':pairs.extend([('dynamic','true'),('density','0.7')])
                if serial%2:pairs.reverse()
                events.append((tag,pairs))
                if depth<3:
                    for i in range(1+serial%2):group(depth+1,serial*2+i+1)
                events.append(('end',[]))
            group(0,case+1);cases.append((None,events))
        cases.extend((path,[('TransformGroup' if op==0 else 'end',pairs) for op,pairs in events]) for path,events in local)
        manager=native.base+0x162cc78;pending=native.base+0x162ccf8
        for case,(path,events) in enumerate(cases):
            owner=PortableServices(core);owner.sequence=case*1000;put(native.base+0x1630d60,0,owner.sequence,'I');nodes=[[],[]];kinds=[];sizes=[];lookup={};context=Context();C.memset(manager,0,0x20);C.memset(pending,0,24)
            requests=(N*4096)();queue=Queue(requests,len(requests),0);used=N();warning=C.c_int();attached=C.c_uint32()
            def view():
                array=(P*len(nodes[1]))(*nodes[1]);kind=(C.c_uint8*len(kinds))(*kinds);return array,kind,OwnedView(array,len(array),kind,0)
            def enqueue():
                for i in range(queue.count):
                    if not core.fs15_scene_lod_enqueue(C.byref(context),nodes[1][requests[i]],C.byref(owner.api)):raise AssertionError('Recursive clone LOD enqueue failed')
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
            @LINK
            def link(_,parent,child,position):
                array,kind,v=view();ok=core.fs15_scene_owned_link_lod(C.byref(v),lookup[parent],lookup[child],position,1,C.byref(owner.api),None,0,C.byref(used),C.byref(attached),C.byref(queue));enqueue();return ok
            @INTEGER
            def integer(_,text,out):out[0]=integer_native(text)&0xffffffff;return 1
            @REGISTER
            def registered(_,key,p):return 1
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
            owner.api.destroy_child=destroy;common=XmlServices(None,C.pointer(owner.api),rebuild,propagate,integer,registered);group_services=GroupServices(common,created,link);services=TreeServices(CloneServices(common,refresh,created),link)
            a,b=allocate(0x170),owner.allocate(None,0x170);ctor(a,b'root',0)
            if not core.fs15_scene_construct(b,0x170,3,b'root',5,0,1,C.byref(owner.api)):raise AssertionError('Recursive clone root rejected')
            nodes[0].append(a);created(None,b,1,0x170);put(native.base+0x162cd18,0,a);state(a,2,0);propagate(None,b,2,0)
            loader=C.create_string_buffer(0x900);lp=C.addressof(loader);put(lp,0x580,a);head=allocate(0x30)
            for at in (0,8,16):put(head,at,head)
            put(head,0x28,1,'B');put(head,0x29,1,'B');put(lp,0x120,head);current=P(b)
            for tag,pairs in events:
                if tag=='end':
                    end(loader)
                    if not core.fs15_scene_group_end(C.byref(current)):raise AssertionError('Source XML end rejected')
                else:
                    na,pa=attributes(pairs);(begin_camera if tag=='Camera' else begin_group)(loader,None,na);nodes[0].append(read(lp,0x580));result=Result()
                    fn=core.fs15_scene_camera_begin if tag=='Camera' else core.fs15_scene_group_begin
                    if not fn(C.byref(current),pa,len(pa),C.byref(group_services),C.byref(result)) or not result.node:raise AssertionError('Source XML begin rejected')
                    counts['original_source_xml_begins']+=1
            source_count=len(nodes[0]);skipped=set()
            if path is None:
                for i in range(1,source_count):
                    if i>2 and (i+case)%7==0:
                        for side in range(2):put(nodes[side][i],0x1c,read(nodes[side][i],0x1c,'I')&~0x40,'I')
                        skipped.add(i)
                    for key,kind in ((b'copied-value',1),(b'copied-text',3)):
                        p=ensure(nodes[0][i]+0x78,key,kind);q=P();core.fs15_scene_attribute_ensure(nodes[1][i],0x170,key,len(key)+1,kind,C.byref(owner.api),C.byref(q))
                        if kind==1:put(p,0,i*17,'I');put(q.value,0,i*17,'I')
                        else:
                            text=f'text-{case}-{i}'.encode()+b'\0';x,y=allocate(len(text)),owner.allocate(None,len(text));C.memmove(x,text,len(text));C.memmove(y,text,len(text));put(p,0,x);put(q.value,0,y)
            remap_a,remap_b=init(owner);list_a,list_b=Vector(),Vector()
            original=recursive(nodes[0][1],C.byref(remap_a),C.byref(list_a),None,None);result=Result();unfinished=Result()
            if not core.fs15_scene_clone_tree(nodes[1][1],C.byref(remap_b),C.byref(list_b),C.byref(services),C.byref(result),C.byref(unfinished)) or not result.node or unfinished.node:raise AssertionError('Recursive clone rejected')
            new_a=[read(list_a.begin,i) for i in range(0,list_a.end-list_a.begin,8)];nodes[0].extend(new_a)
            if original!=new_a[0] or len(nodes[0])!=len(nodes[1]):raise AssertionError('Recursive clone count/order differs')
            counts['recursively_cloned_nodes']+=len(new_a)
            def compare():
                maps=[{p:i+1 for i,p in enumerate(side)} for side in nodes]
                for i,(a,b) in enumerate(zip(*nodes)):
                    x,y=bytearray(canonical(C.string_at(a,sizes[i]),3,nodes[0])),bytearray(canonical(C.string_at(b,sizes[i]),3,nodes[1]))
                    for at in (0x30,0x38,0x40):x[at:at+8]=y[at:at+8]=bytes(8)
                    for at,length in ((0x98,16),(0xd8,16),(0x148,9)):
                        av,bv=struct.unpack_from('<'+'f'*length,x,at),struct.unpack_from('<'+'f'*length,y,at)
                        if any(not math.isfinite(a) or not math.isfinite(b) or abs(a-b)>1e-5*max(1,abs(a)) for a,b in zip(av,bv)):raise AssertionError('Recursive clone transforms differ')
                        x[at:at+length*4]=y[at:at+length*4]=bytes(length*4)
                    if x!=y:raise AssertionError(f'Recursive clone record differs {case}/{i}: {[hex(j) for j in range(sizes[i]) if x[j]!=y[j]][:12]}')
                    def children(p,mapping):
                        start,stop,cap=(read(p,o) for o in (0x30,0x38,0x40));return stop-start,cap-start,tuple(mapping.get(read(start,j),read(start,j)) for j in range(0,cap-start,8))
                    if children(a,maps[0])!=children(b,maps[1]) or C.string_at(read(a,0x10))!=C.string_at(read(b,0x10)):raise AssertionError('Recursive clone name/child allocation differs')
                    if tree(a)!=tree(b):raise AssertionError('Recursive clone attribute trees differ')
                    pa,pb=read(a,0x118),read(b,0x118);ps={0x1404c0ca8:0x10,0x1404c7a90:0x88,0x1404c8158:0x90}[read(pa)];av,bv=bytearray(C.string_at(pa,ps)),bytearray(C.string_at(pb,ps))
                    if ps>0x10:at=0x28 if ps==0x88 else 0x20;av[at:at+8]=bv[at:at+8]=bytes(8)
                    if av!=bv:raise AssertionError('Recursive clone properties differ')
                    digest.update(x);digest.update(av);counts['complete_recursive_node_records']+=1
                def remapped(m,mapping):return {mapping[k]:mapping[read(p,0x20)] for k,p in entries(m).items()}
                if remapped(remap_a,maps[0])!=remapped(remap_b,maps[1]):raise AssertionError('Recursive original-to-clone remap differs')
                def vector(v,mapping):return v.end-v.begin,v.capacity-v.begin,tuple(mapping.get(read(v.begin,j),read(v.begin,j)) for j in range(0,v.capacity-v.begin,8))
                if vector(list_a,maps[0])!=vector(list_b,maps[1]):raise AssertionError('Recursive clone list allocation/order differs')
                if vector(Vector.from_address(pending),maps[0])!=vector(context.pending,maps[1]):raise AssertionError('Recursive clone LOD request queue differs')
                counts['complete_recursive_forest_states']+=1
            compare()
            before=(len(native.allocations),len(owner.blocks),list_a.end,list_b.end)
            if recursive(nodes[0][1],C.byref(remap_a),C.byref(list_a),None,None):raise AssertionError('Native duplicate root unexpectedly clones')
            if not core.fs15_scene_clone_tree(nodes[1][1],C.byref(remap_b),C.byref(list_b),C.byref(services),C.byref(result),C.byref(unfinished)) or result.node or before!=(len(native.allocations),len(owner.blocks),list_a.end,list_b.end):raise AssertionError('Duplicate root no-op differs')
            # The original lookup branch consumes the actual reconstructed map
            # layout; all roots are cached, so no mixed allocator writes occur.
            for source in entries(remap_b):
                if recursive(source,C.byref(remap_b),C.byref(list_b),None,None):raise AssertionError('Original code cannot read reconstructed remap')
                counts['original_lookups_on_reconstructed_remap']+=1
            compare();counts['duplicate_root_calls']+=1
            va=Vector.from_address(pending)
            for i in range((va.end-va.begin)//8):
                lod_update(read(va.begin,i*8))
                if not core.fs15_scene_lod_update(read(context.pending.begin,i*8),C.byref(context),C.byref(lod_services)):raise AssertionError('Recursive cloned LOD update rejected')
                counts['recursive_lod_updates']+=1
            va.end=va.begin;context.pending.end=context.pending.begin
            for a,b in zip(*nodes):
                world_native(a);position=(C.c_float*3)()
                if not world(None,b,position):raise AssertionError('Recursive cloned world cache rejected')
            compare()
            ma,mb=Manager.from_address(manager),context.manager;ah,bh=bytearray(C.string_at(manager,0x20)),bytearray(bytes(mb));ah[:8]=bh[:8]=bytes(8)
            if ah!=bh:raise AssertionError('Recursive clone LOD manager header differs')
            av,bv=bytearray(C.string_at(ma.entries,ma.capacity*0x28) if ma.entries else b''),bytearray(C.string_at(mb.entries,mb.capacity*0x28) if mb.entries else b'')
            for i in range(ma.capacity):
                for payload,objects in ((av,nodes[0]),(bv,nodes[1])):
                    mapping={p:i+1 for i,p in enumerate(objects)};value=struct.unpack_from('<Q',payload,i*0x28)[0];struct.pack_into('<Q',payload,i*0x28,mapping.get(value,value))
                if i<ma.count:
                    x,y=struct.unpack_from('<3f',av,i*0x28+8),struct.unpack_from('<3f',bv,i*0x28+8)
                    if any(abs(a-b)>1e-5*max(1,abs(a)) for a,b in zip(x,y)):raise AssertionError('Recursive LOD manager position differs')
                    av[i*0x28+8:i*0x28+20]=bv[i*0x28+8:i*0x28+20]=bytes(12)
            if av!=bv:raise AssertionError('Recursive LOD manager complete allocation differs')
            counts['complete_recursive_lod_manager_allocations']+=1
            # Unsupported active trigger cleanup is outside this metadata path.
            # Disable pending trigger metadata only after all copy comparisons.
            for a,b in zip(*nodes):
                pa,pb=read(a,0x118),read(b,0x118)
                if read(pa)==0x1404c7a90 and read(pa,8,'I')&0x1000:disable(pa,0x1000);core.fs15_scene_property_flags(pb,0x1000,0,None);counts['pending_trigger_cleanup_boundaries']+=1
            clear(remap_a,remap_b,owner)
            if list_a.begin:release(list_a.begin)
            if list_b.begin:owner.release(None,list_b.begin)
            for a,b in [(a,b) for a,b in zip(*nodes) if not read(a,0x28)]:
                if read(a)==0x1404c1c48:C.WINFUNCTYPE(P,P,C.c_uint32)(native.base+0xd7bb0)(a,1)
                elif read(a)==0x1404c1078:C.WINFUNCTYPE(P,P,C.c_uint32)(native.base+0xb5910)(a,1)
                else:deleting(a,1)
                if not destroy(None,b):raise AssertionError('Recursive clone forest teardown rejected')
            if Manager.from_address(manager).count or context.manager.count or Vector.from_address(pending).end!=Vector.from_address(pending).begin or context.pending.end!=context.pending.begin:raise AssertionError('Recursive cleanup retained LOD ownership')
            def free_ids(p):
                if read(p,0x29,'B'):return
                free_ids(read(p));free_ids(read(p,16));release(p)
            free_ids(read(head,8));release(head)
            for p in (read(manager),read(pending)):
                if p:release(p)
            for p in (context.manager.entries,context.pending.begin):
                if p:owner.release(None,p)
            if owner.blocks or owner.registry or native.registry() or len(native.allocations)!=1:raise AssertionError('Recursive clone lifetime leaked')
            counts['complete_recursive_forest_lifetimes']+=1
            if path:counts['original_complete_subtree_clones']+=1
        put(native.base+0x162cd18,0,0);C.memset(manager,0,0x20);C.memset(pending,0,24)
    paths=sorted(p for p in (ROOT/'src/core').glob('*') if p.suffix in ('.cpp','.h'))
    report=dict(status='passed',reference_sha256=EXPECTED_IMAGE,native_entries=['0x1401ba050','0x14016ea00','0x14016eb20','0x140096190','0x140088360','0x140320740','0x1401b9d70'],rebuilt_source_sha256={p.relative_to(ROOT).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in paths},tested_dll_sha256=hashlib.sha256(args.dll.read_bytes()).hexdigest(),comparisons=dict(counts),original_results_sha256=digest.hexdigest(),local_assets=assets,
        scope='Pointer-keyed remap factory/unique insertion/RB balancing/subtree free, clone-list allocation/growth and recursive pre-order TRS/Camera/LOD cloning. Private unchanged original code; platform/owned callback transport and finite/allocation/depth/work guards authored. Standalone remap topology/opaque bytes exact with identical numeric keys; scene address ordering differs across allocations, so recursive remaps compare normalized associations and original code directly reads reconstructed maps. Complete records/properties/attributes/child and clone-list allocations checked, transform/libm tolerance 1e-5. Complete supported original subtrees retain original attributes/hierarchy. Pending trigger metadata disabled only for bounded teardown after comparison. Shape/Light/reference clones, original top-level finalization, skin/animation remapping, live backend effects and full engine startup remain open.')
    write_json(ROOT/'analysis/scene-clone-tree-validation.json',report);print(json.dumps(report,indent=2))


if __name__=='__main__':main()
