"""Compare recovered LOD construction, distance cache and owned hierarchy flow."""
import argparse
from collections import Counter
import ctypes as C
import hashlib
import json
from pathlib import Path
import random
import struct
from inventory import ROOT,write_json
from native_gar import EXPECTED_IMAGE
from native_animation_xml import NativeAnimationXml
from check_scene_lifecycle import PortableServices,Services,DESTROY,P,N,read,put,canonical
from check_scene_state import Notification
from check_scene_owned import Vector,OwnedView

BASE=C.CFUNCTYPE(C.c_int,P,P,C.c_uint32,C.c_int32)
WORLD=C.CFUNCTYPE(C.c_int,P,P,P)
MARK=C.CFUNCTYPE(C.c_int,P,P,C.c_uint32)
class LodServices(C.Structure):
    _fields_=[('user',P),('memory',C.POINTER(Services)),('base',BASE),('world',WORLD),('mark',MARK)]
class Entry(C.Structure):
    _fields_=[('node',P),('position',C.c_float*3),('squared',C.c_float*4),('levels',C.c_uint8),('selected',C.c_uint8),('stamp',C.c_uint16)]
class Manager(C.Structure):
    _fields_=[('entries',P),('viewer',C.c_float*3),('factor',C.c_float),('count',C.c_uint16),('capacity',C.c_uint16),('stamp',C.c_uint32)]
class Context(C.Structure):
    _fields_=[('manager',Manager),('pending',Vector)]
class Queue(C.Structure):
    _fields_=[('indices',C.POINTER(N)),('capacity',N),('count',N)]
ENTRIES=[0xd7a70,0xd7be0,0xd7bb0,0xd7620,0xd7840,0xd78f0,0xd60b0,0xd7d50,0xd7e20,0xd7ea0,0xd7db0,0xb6260,0x87e50]


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--dll',required=True,type=Path);args=parser.parse_args()
    core=C.CDLL(str(args.dll.resolve()));rng=random.Random(0xf51510d);counts=Counter();digest=hashlib.sha256()
    signatures={'fs15_scene_lod_construct':[P,N,C.c_char_p,N,P,C.c_uint32,C.POINTER(Services)],
        'fs15_scene_lod_destruct':[P,N,C.c_uint32,C.POINTER(Context),C.POINTER(LodServices)],
        'fs15_scene_lod_set_index':[P,C.c_uint32,C.POINTER(LodServices)],'fs15_scene_lod_child_token':[P,P,P],
        'fs15_scene_inherited_lod_token':[P,P],'fs15_scene_lod_register':[P,P,P,P,C.c_uint32,P,C.POINTER(Services)],
        'fs15_scene_lod_move':[P,C.c_uint32,P,P,C.c_uint32],'fs15_scene_lod_remove':[P,C.c_uint32,C.POINTER(LodServices)],
        'fs15_scene_lod_test':[P,C.c_uint32,P],'fs15_scene_lod_enqueue':[P,P,C.POINTER(Services)],
        'fs15_scene_lod_state':[P,C.c_uint32,C.c_int32,P,C.POINTER(LodServices)],'fs15_scene_lod_update':[P,P,C.POINTER(LodServices)],
        'fs15_scene_owned_state':[P,N,C.c_uint,C.c_uint32,C.c_int32,P,N,P],
        'fs15_scene_owned_state_lod':[P,N,C.c_uint,C.c_uint32,C.c_int32,P,N,P,P],
        'fs15_scene_owned_trs_lod':[P,N,C.c_uint,P,N,P,N,P,P,P],
        'fs15_scene_owned_link_lod':[P,N,N,C.c_uint32,C.c_int,C.POINTER(Services),P,N,P,P,P],
        'fs15_scene_owned_unlink_lod':[P,N,C.c_uint32,C.c_int,P,N,P,P],
        'fs15_scene_owned_world':[P,N,P,N],'fs15_scene_owned_mark_branch':[P],
        'fs15_scene_construct':[P,N,C.c_uint,C.c_char_p,N,C.c_uint32,C.c_uint8,C.POINTER(Services)],
        'fs15_scene_destruct':[P,N,C.c_uint,C.c_uint32,C.POINTER(Services)]}
    for name,signature in signatures.items():getattr(core,name).argtypes=signature;getattr(core,name).restype=C.c_int
    def floats(values):return (C.c_float*len(values))(*values)
    with NativeAnimationXml() as native:
        allocate=lambda size:native.callbacks[0](native.heap,0,size)
        release=lambda p:native.callbacks[1](native.heap,0,p)
        ctor=C.WINFUNCTYPE(P,P,C.c_char_p,P,C.c_uint32)(native.base+0xd7a70)
        dtor=C.WINFUNCTYPE(None,P)(native.base+0xd7be0);deleting=C.WINFUNCTYPE(P,P,C.c_uint32)(native.base+0xd7bb0)
        state=C.WINFUNCTYPE(None,P,C.c_uint32,C.c_int32)(native.base+0xd7d50)
        set_index=C.WINFUNCTYPE(None,P,C.c_uint32)(native.base+0xd7e20)
        child_token=C.WINFUNCTYPE(C.c_uint32,P,P)(native.base+0xd7db0)
        inherited=C.WINFUNCTYPE(C.c_uint32,P,P)(native.base+0xb6260)
        register=C.WINFUNCTYPE(None,P,P,P,P,C.c_uint32,P)(native.base+0xd7620)
        remove=C.WINFUNCTYPE(None,P,C.c_uint32)(native.base+0xd7840)
        move=C.WINFUNCTYPE(None,P,C.c_uint32,P,P,C.c_uint32)(native.base+0xd78f0)
        test=C.WINFUNCTYPE(C.c_uint8,P,C.c_uint32)(native.base+0xd60b0)
        update=C.WINFUNCTYPE(None,P)(native.base+0xd7ea0)
        native_manager=native.base+0x162cc78;native_pending=native.base+0x162ccf8
        C.memset(native_manager,0,0x20);C.memset(native_pending,0,24)
        def pointer_vector(pointer,mapping):
            p=Vector.from_address(pointer);return ((p.end-p.begin)//8,(p.capacity-p.begin)//8,tuple(mapping.get(read(p.begin,j),('opaque',read(p.begin,j))) for j in range(0,p.capacity-p.begin,8)))
        def manager_equal(a,b,amap,bmap):
            aa,bb=Manager.from_address(a),Manager.from_address(b)
            av,bv=bytearray(C.string_at(a,0x20)),bytearray(C.string_at(b,0x20));av[:8]=bv[:8]=bytes(8)
            if av!=bv:raise AssertionError('LOD manager header differs')
            av,bv=bytearray(C.string_at(aa.entries,aa.capacity*0x28) if aa.entries else b''),bytearray(C.string_at(bb.entries,bb.capacity*0x28) if bb.entries else b'')
            for i in range(aa.capacity):
                x,y=struct.unpack_from('<Q',av,i*0x28)[0],struct.unpack_from('<Q',bv,i*0x28)[0]
                struct.pack_into('<Q',av,i*0x28,amap.get(x,x));struct.pack_into('<Q',bv,i*0x28,bmap.get(y,y))
            if av!=bv:raise AssertionError('LOD complete manager allocation differs')
            digest.update(av);counts['complete_manager_allocations']+=1
        # Constructor defaults, exact child-token arguments and standalone state.
        for case in range(160):
            owner=PortableServices(core);owner.sequence=case*3;C.c_uint32.from_address(native.base+0x1630d60).value=owner.sequence
            context=Context();a,b=allocate(0x188),owner.allocate(None,0x188);distances=floats([rng.uniform(-500,500) for _ in range(9)]);count=case%10;name=(b'',b'LOD',b'long-lod-name-'*3)[case%3]
            ctor(a,name,distances,count)
            if not core.fs15_scene_lod_construct(b,0x188,name,len(name)+1,distances,count,C.byref(owner.api)):raise AssertionError('LOD constructor rejected')
            def exact():
                av,bv=canonical(C.string_at(a,0x188),3),canonical(C.string_at(b,0x188),3)
                if av!=bv:raise AssertionError(f'LOD node bytes differ {case}: {[hex(i) for i in range(len(av)) if av[i]!=bv[i]][:16]}')
                digest.update(av)
            exact();counts['complete_lod_constructors']+=1
            tokens=[[],[]];children=[]
            @C.WINFUNCTYPE(None,P,C.c_uint32)
            def mark_native(p,token):tokens[0].append((read(p,8),token))
            table=(N*6)(0,0,0,0,0,C.cast(mark_native,P).value)
            links=(N*(case%8))()
            for i in range(len(links)):
                child=(N*2)(C.addressof(table),i);children.append(child);links[i]=C.addressof(child)
            for p in (a,b):
                base=C.addressof(links) if len(links) else 0
                for offset,value in ((0x30,base),(0x38,base+len(links)*8),(0x40,base+len(links)*8)):put(p,offset,value)
            @MARK
            def mark_portable(_,p,token):tokens[1].append((read(p,8),token));return 1
            services=LodServices(None,C.pointer(owner.api),BASE(),WORLD(),mark_portable)
            for identifier in (case%31,case%31,0x3fffffff,0x80000001):
                set_index(a,identifier)
                if not core.fs15_scene_lod_set_index(b,identifier,C.byref(services)) or tokens[0]!=tokens[1]:raise AssertionError('LOD set-index child tokens differ')
                for child in [*links,None,0x12345678]:
                    expected=child_token(a,child);actual=C.c_uint32()
                    if not core.fs15_scene_lod_child_token(b,child,C.byref(actual)) or expected!=actual.value:raise AssertionError('LOD child token differs')
                    counts['lod_child_token_queries']+=1
                counts['lod_index_updates']+=1
            # Synthetic callback children are externally owned, not destructor members.
            for p in (a,b):
                for offset in (0x30,0x38,0x40):put(p,offset,0)
                put(p,0x180,0x3fffffff,'I')
            array=(P*1)(b);kinds=(C.c_uint8*1)(1);view=OwnedView(array,1,kinds,0);used=N()
            C.c_void_p.from_address(native.base+0x162cd18).value=a
            @BASE
            def base_state(_,p,mask,origin):return core.fs15_scene_owned_state(C.byref(view),0,0,mask,origin,None,0,C.byref(used))
            services.base=base_state
            for mask in (0,1,2,3,0x3f,1):
                state(a,mask,case%2)
                if not core.fs15_scene_lod_state(b,mask,case%2,C.byref(context),C.byref(services)):raise AssertionError('Standalone LOD state rejected')
                exact()
                if pointer_vector(native_pending,{a:1})!=pointer_vector(C.addressof(context)+0x20,{b:1}):raise AssertionError('LOD pending queue differs')
                counts['standalone_lod_states']+=1
            if case%2:
                deleting(a,1);ok=core.fs15_scene_lod_destruct(b,0x188,1,C.byref(context),C.byref(services));counts['lod_deleting_destructors']+=1
            else:
                dtor(a);ok=core.fs15_scene_lod_destruct(b,0x188,0,C.byref(context),C.byref(services));exact();release(a);owner.release(None,b);counts['lod_non_deleting_destructors']+=1
            if not ok:raise AssertionError('LOD destructor rejected')
            if read(native_pending)!=0:release(read(native_pending))
            if context.pending.begin:owner.release(None,context.pending.begin)
            C.memset(native_pending,0,24)
            if owner.blocks or owner.registry or len(native.allocations)!=1 or native.registry():raise AssertionError('LOD leaf lifetime leaked')
        # One registry crosses both 1024-entry growth boundaries. Native row
        # owners are valid dummy LOD views, not constructed/registered objects.
        owner=PortableServices(core);ma,mb=Manager(),Manager();blocks=[[],[]];maps=[{},{}];services=LodServices(None,C.pointer(owner.api),BASE(),WORLD(),MARK())
        for i in range(2100):
            pair=[C.create_string_buffer(0x188),C.create_string_buffer(0x188)]
            for side,block in enumerate(pair):
                blocks[side].append(block);p=C.addressof(block);maps[side][p]=i+1;put(p,0,0x1404c1c48);put(p,0x180,0x3fffffff,'I')
            position=floats([rng.uniform(-1000,1000) for _ in range(3)]);distances=floats([rng.uniform(-500,500) for _ in range(7)]);count=i%8;ia=C.c_uint32();ib=C.c_uint32()
            register(C.byref(ma),pair[0],position,distances,count,C.byref(ia))
            if not core.fs15_scene_lod_register(C.byref(mb),pair[1],position,distances,count,C.byref(ib),C.byref(owner.api)) or ia.value!=ib.value:raise AssertionError('LOD register differs')
            set_index(pair[0],ia.value)
            if not core.fs15_scene_lod_set_index(pair[1],ib.value,C.byref(services)):raise AssertionError('LOD row index rejected')
            if i%17==0:manager_equal(C.addressof(ma),C.addressof(mb),*maps)
            counts['manager_registrations']+=1
        for i in range(12000):
            index=rng.randrange(ma.count);row=Entry.from_address(ma.entries+index*0x28);tag=i%4
            if i%3!=0:
                for m in (ma,mb):m.stamp=(i*13)&0xffffffff;m.factor=C.c_float((0,0.25,1,4,25)[i%5]).value
                for j in range(3):ma.viewer[j]=mb.viewer[j]=row.position[j]+C.c_float(rng.uniform(-1000,1000)).value
            token=(tag<<30)|index;expected=test(C.byref(ma),token);actual=C.c_int()
            if not core.fs15_scene_lod_test(C.byref(mb),token,C.byref(actual)) or bool(expected)!=bool(actual.value):raise AssertionError('LOD stamp-cached distance selection differs')
            if C.string_at(ma.entries+index*0x28+8,32)!=C.string_at(mb.entries+index*0x28+8,32):raise AssertionError('LOD query modified row bytes differently')
            if i%7==0:
                position=floats([rng.uniform(-500,500) for _ in range(3)]);distances=floats([0,10,100,500]);count=i%7
                move(C.byref(ma),index,position,distances,count)
                if not core.fs15_scene_lod_move(C.byref(mb),index,position,distances,count):raise AssertionError('LOD move rejected')
                counts['manager_moves']+=1
            counts['distance_cache_queries']+=1
        manager_equal(C.addressof(ma),C.addressof(mb),*maps)
        while ma.count:
            index=rng.randrange(ma.count+2);remove(C.byref(ma),index)
            if not core.fs15_scene_lod_remove(C.byref(mb),index,C.byref(services)):raise AssertionError('LOD swap-remove rejected')
            for a,b in zip(*blocks):
                if read(C.addressof(a),0x180,'I')!=read(C.addressof(b),0x180,'I'):raise AssertionError('Swapped LOD owner index differs')
            if ma.count%41==0:manager_equal(C.addressof(ma),C.addressof(mb),*maps)
            counts['manager_removals']+=1
        release(ma.entries);owner.release(None,mb.entries)
        # Mixed nested LOD/TRS forests: base propagation dispatches LOD children,
        # queue requests reach the real native global register, moves invalidate
        # row caches, hiding unregisters, and deleting a forest compacts indices.
        plain_ctor=C.WINFUNCTYPE(P,P,C.c_char_p,C.c_uint32)(native.base+0x10fa90)
        plain_delete=C.WINFUNCTYPE(P,P,C.c_uint32)(native.base+0x10fb10)
        native_link=C.WINFUNCTYPE(C.c_uint32,P,P,C.c_uint32)(native.base+0xb17b0)
        native_rebuild=C.WINFUNCTYPE(None,P)(native.base+0x10fd00)
        for case in range(36):
            owner=PortableServices(core);context=Context();nodes=[[],[]];kinds=(C.c_uint8*16)(*[2 if i%3==1 else 1 for i in range(16)])
            owner.sequence=case*100;C.c_uint32.from_address(native.base+0x1630d60).value=owner.sequence
            for i,kind in enumerate(kinds):
                size=0x188 if kind==2 else 0x170;a,b=allocate(size),owner.allocate(None,size)
                if kind==2:
                    distances=floats([0,10,100,1000,2000]);ctor(a,b'lod-group',distances,(case+i)%6)
                    ok=core.fs15_scene_lod_construct(b,size,b'lod-group',10,distances,(case+i)%6,C.byref(owner.api))
                else:plain_ctor(a,b'transform',0);ok=core.fs15_scene_construct(b,size,3,b'transform',10,0,1,C.byref(owner.api))
                if not ok:raise AssertionError('Mixed forest constructor rejected')
                nodes[0].append(a);nodes[1].append(b);owner.kinds[b]=3
            maps=[{p:i+1 for i,p in enumerate(side)} for side in nodes];portable_indices={p:i for i,p in enumerate(nodes[1])}
            array=(P*16)(*nodes[1]);view=OwnedView(array,16,kinds,0);events=(Notification*256)();used=N();warning=C.c_int();attached=C.c_uint32();requests=(N*256)();queue=Queue(requests,len(requests),0)
            C.c_void_p.from_address(native.base+0x162cd18).value=nodes[0][0]
            @WORLD
            def world(_,p,position):return core.fs15_scene_owned_world(C.byref(view),portable_indices[p],position,3)
            @MARK
            def mark(_,p,token):counts['owned_marker_dispatches']+=1;return core.fs15_scene_owned_mark_branch(p)
            services=LodServices(None,C.pointer(owner.api),BASE(),world,mark)
            @DESTROY
            def destroy(_,p):
                if kinds[portable_indices[p]]==2:return core.fs15_scene_lod_destruct(p,0x188,1,C.byref(context),C.byref(services))
                return core.fs15_scene_destruct(p,0x170,3,1,C.byref(owner.api))
            owner.api.destroy_child=destroy
            def enqueue():
                for j in range(queue.count):
                    if not core.fs15_scene_lod_enqueue(C.byref(context),nodes[1][requests[j]],C.byref(owner.api)):raise AssertionError('Owned LOD enqueue rejected')
                counts['owned_queue_requests']+=queue.count;queue.count=0
            def compare():
                for i,(a,b) in enumerate(zip(*nodes)):
                    size=0x188 if kinds[i]==2 else 0x170
                    av,bv=bytearray(canonical(C.string_at(a,size),3,nodes[0])),bytearray(canonical(C.string_at(b,size),3,nodes[1]))
                    for offset in (0x30,0x38,0x40):av[offset:offset+8]=bv[offset:offset+8]=bytes(8)
                    for offset,length in ((0x98,16),(0xd8,16),(0x148,9)):
                        x,y=struct.unpack_from('<'+'f'*length,av,offset),struct.unpack_from('<'+'f'*length,bv,offset)
                        if any(abs(a-b)>1e-5*max(1,abs(a)) for a,b in zip(x,y)):raise AssertionError('Mixed LOD forest matrix differs')
                        av[offset:offset+length*4]=bv[offset:offset+length*4]=bytes(length*4)
                    if av!=bv:raise AssertionError(f'Mixed LOD forest record differs {case}/{i}: {[hex(j) for j in range(size) if av[j]!=bv[j]][:16]}')
                    def children(p,mapping):
                        begin,end,capacity=(read(p,at) for at in (0x30,0x38,0x40))
                        return (end-begin,capacity-begin,tuple(mapping.get(read(begin,j),read(begin,j)) for j in range(0,capacity-begin,8)))
                    if children(a,maps[0])!=children(b,maps[1]):raise AssertionError('Mixed child allocation differs')
                    native_token=C.WINFUNCTYPE(C.c_uint32,P,P)(read(read(a),0x30))(a,None);rebuilt_token=C.c_uint32()
                    if not core.fs15_scene_inherited_lod_token(b,C.byref(rebuilt_token)) or native_token!=rebuilt_token.value:raise AssertionError('Inherited LOD token differs')
                    counts['inherited_lod_token_queries']+=1;digest.update(av)
                if pointer_vector(native_pending,maps[0])!=pointer_vector(C.addressof(context)+0x20,maps[1]):raise AssertionError('Owned pending LOD allocation differs')
                manager_equal(native_manager,C.addressof(context.manager),*maps);counts['mixed_owned_forest_states']+=1
            def flush():
                pending=Vector.from_address(native_pending);original=[read(pending.begin,j) for j in range(0,pending.end-pending.begin,8)]
                rebuilt=[read(context.pending.begin,j) for j in range(0,context.pending.end-context.pending.begin,8)]
                for a,b in zip(original,rebuilt):
                    update(a)
                    if not core.fs15_scene_lod_update(b,C.byref(context),C.byref(services)):raise AssertionError('LOD world/registration update rejected')
                    counts['lod_resource_updates']+=1
                pending.end=pending.begin;context.pending.end=context.pending.begin
            for i in range(1,16):
                parent=(i-1)//3;native_link(nodes[0][parent],nodes[0][i],0xffffffff)
                if not core.fs15_scene_owned_link_lod(C.byref(view),parent,i,0xffffffff,1,C.byref(owner.api),events,len(events),C.byref(used),C.byref(attached),C.byref(queue)):raise AssertionError('Owned LOD link rejected')
                enqueue();compare()
            for step in range(60):
                target=(case+step)%16
                if step%3==0:
                    for p in (nodes[0][target],nodes[1][target]):put(p,0x148,float(step+case),'f')
                    native_rebuild(nodes[0][target])
                    ok=core.fs15_scene_owned_trs_lod(C.byref(view),target,1,None,0,events,len(events),C.byref(used),C.byref(warning),C.byref(queue))
                else:
                    # Toggle local visibility, then dispatch the actual virtual
                    # update, including repeated masks suppressed by base logic.
                    for p in (nodes[0][target],nodes[1][target]):put(p,0x50,read(p,0x50,'I')^1,'I')
                    C.WINFUNCTYPE(None,P,C.c_uint32,C.c_int32)(read(read(nodes[0][target]),0x20))(nodes[0][target],3,0)
                    ok=core.fs15_scene_owned_state_lod(C.byref(view),target,0,3,0,events,len(events),C.byref(used),C.byref(queue))
                if not ok:raise AssertionError('Mixed LOD state/rebuild rejected')
                enqueue();compare();flush();compare()
                m=Manager.from_address(native_manager)
                for j in range(m.count):
                    m.stamp=context.manager.stamp=(case*60+step)&0xffff;m.factor=context.manager.factor=1
                    for axis in range(3):m.viewer[axis]=context.manager.viewer[axis]=float(step*17)
                    for level in range(4):
                        token=(level<<30)|j;actual=C.c_int()
                        if not core.fs15_scene_lod_test(C.byref(context.manager),token,C.byref(actual)) or bool(test(native_manager,token))!=bool(actual.value):raise AssertionError('Owned LOD distance cache differs')
                        counts['owned_distance_cache_queries']+=1
            flush();compare();plain_delete(nodes[0][0],1)
            if not core.fs15_scene_destruct(nodes[1][0],0x170,3,1,C.byref(owner.api)):raise AssertionError('Mixed LOD recursive cleanup rejected')
            if Manager.from_address(native_manager).count or context.manager.count:raise AssertionError('LOD registry retains deleted nodes')
            for pointer in (read(native_manager),read(native_pending)):
                if pointer:release(pointer)
            for pointer in (context.manager.entries,context.pending.begin):
                if pointer:owner.release(None,pointer)
            C.memset(native_manager,0,0x20);C.memset(native_pending,0,24)
            if owner.blocks or owner.registry or len(native.allocations)!=1 or native.registry():raise AssertionError('Mixed LOD forest leaked storage')
            counts['mixed_owned_forest_lifetimes']+=1
        C.c_void_p.from_address(native.base+0x162cd18).value=0
        if owner.blocks or len(native.allocations)!=1:raise AssertionError('LOD registry storage leaked')
    paths=tuple('src/core/'+p for p in ('scene_lod.cpp','scene_lod.h','scene_owned.cpp','scene_owned.h','scene_state.cpp','scene_state.h','scene_lifecycle.cpp','scene_lifecycle.h','scene_math.cpp','scene_math.h','scene_graph.cpp','scene_graph.h','scene_trs.cpp','scene_trs.h','shared_scene.cpp','shared_scene.h','scene_attributes.cpp','scene_attributes.h','scene_properties.cpp','scene_properties.h'))
    report=dict(status='passed',reference_sha256=EXPECTED_IMAGE,native_entries=[f'0x140{entry:06x}' for entry in ENTRIES],rebuilt_source_sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in paths},tested_dll_sha256=hashlib.sha256(args.dll.read_bytes()).hexdigest(),comparisons=dict(counts),original_results_sha256=digest.hexdigest(),scope='Known LOD records, register/move/swap-remove and stamp-cached distance selection, child tokens and supported lifetime/state paths. Platform allocation/registration/dispatch and queue collection are authored transport. Original image bytes execute unchanged in a private fixture. Full spatial culling, derived rendering nodes, complete I3D loader, renderer and engine startup remain open.')
    write_json(ROOT/'analysis/scene-lod-validation.json',report);print(json.dumps(report,indent=2))


if __name__=='__main__':main()
