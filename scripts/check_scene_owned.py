"""Differential checks for owned native vectors and separately allocated forests.

Original constructor, vector, hierarchy, state and destructor code executes in an
isolated private-heap reference; authored scatter transport calls recovered core
operations on the reconstructed side. No game entry point is started.
"""
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
from check_scene_lifecycle import PortableServices,Services,P,N,read,put,canonical
from check_scene_state import Notification,NONE
from check_scene_xml import XmlServices,REBUILD,PROPAGATE,INTEGER,REGISTER
from check_animation_xml import attributes


class Vector(C.Structure):
    _fields_=[('begin',N),('end',N),('capacity',N)]


class OwnedView(C.Structure):
    _fields_=[('nodes',C.POINTER(P)),('count',N),('kinds',C.POINTER(C.c_uint8)),('root',N)]


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--dll',type=Path,required=True);args=parser.parse_args()
    core=C.CDLL(str(args.dll.resolve()));rng=random.Random(0xf5150d34e0);counts=Counter();digest=hashlib.sha256();trace=[];native_indices={}
    signatures={'fs15_scene_construct':[P,N,C.c_uint,C.c_char_p,N,C.c_uint32,C.c_uint8,C.POINTER(Services)],
        'fs15_scene_destruct':[P,N,C.c_uint,C.c_uint32,C.POINTER(Services)],
        'fs15_scene_pointer_reserve':[C.POINTER(Vector),N,C.POINTER(Services)],
        'fs15_scene_pointer_grow':[C.POINTER(Vector),C.POINTER(Services)],
        'fs15_scene_pointer_append':[C.POINTER(Vector),P,C.POINTER(Services)],
        'fs15_scene_owned_link':[C.POINTER(OwnedView),N,N,C.c_uint32,C.c_int,C.POINTER(Services),P,N,C.POINTER(N),C.POINTER(C.c_uint32)],
        'fs15_scene_owned_unlink':[C.POINTER(OwnedView),N,C.c_uint32,C.c_int,P,N,C.POINTER(N)],
        'fs15_scene_owned_state':[C.POINTER(OwnedView),N,C.c_uint,C.c_uint32,C.c_int32,P,N,C.POINTER(N)],
        'fs15_scene_owned_trs':[C.POINTER(OwnedView),N,C.c_uint,P,N,P,N,C.POINTER(N),C.POINTER(C.c_int)],
        'fs15_scene_xml_apply':[P,P,N,C.c_int,C.c_int,C.c_int,C.c_char_p,N,C.POINTER(XmlServices)]}
    for name,signature in signatures.items():getattr(core,name).argtypes=signature;getattr(core,name).restype=C.c_int
    @C.WINFUNCTYPE(None,P,P,C.c_int32)
    def observe(observer,node,origin):trace.append((native_indices[node],observer,origin))
    with NativeAnimationXml() as native:
        allocate=lambda size:native.callbacks[0](native.heap,0,size)
        release=lambda p:native.callbacks[1](native.heap,0,p)
        reserve=C.WINFUNCTYPE(None,P,N)(native.base+0xd5f30)
        grow=C.WINFUNCTYPE(None,P)(native.base+0xd34e0)
        append=C.WINFUNCTYPE(None,P,P)(native.base+0x5a1b0)
        ctor=C.WINFUNCTYPE(P,P,C.c_char_p,C.c_uint32)(native.base+0x10fa90)
        destruct=C.WINFUNCTYPE(P,P,C.c_uint32)(native.base+0x10fb10)
        links=[C.WINFUNCTYPE(C.c_uint32,P,P,C.c_uint32)(native.base+entry) for entry in (0x1363d0,0xb17b0)]
        unlink=C.WINFUNCTYPE(None,P,C.c_uint32,C.c_uint8)(native.base+0xb19c0)
        state=C.WINFUNCTYPE(None,P,C.c_uint32,C.c_int32)(native.base+0xb1b00)
        rescan=C.WINFUNCTYPE(None,P,C.c_uint32)(native.base+0xb6160)
        rebuild=C.WINFUNCTYPE(None,P)(native.base+0x10fd00)
        common=C.WINFUNCTYPE(None,P,P,P,C.c_uint8,C.c_uint8,C.c_uint8)(native.base+0x18c100)
        scalar_integer=C.WINFUNCTYPE(C.c_int32,C.c_char_p)(native.base+0x3fd784)
        owner=PortableServices(core);a,b=Vector(),Vector()
        append(C.byref(a),C.byref(a))
        if not core.fs15_scene_pointer_append(C.byref(b),C.byref(b),C.byref(owner.api)) or read(a.begin)!=a.begin or read(b.begin)!=b.begin:raise AssertionError('External vector-metadata alias must be read after relocation')
        release(a.begin);owner.release(None,b.begin);counts['vector_metadata_alias_after_growth']=1
        # Actual reallocations, spare bytes and in-vector input aliasing.
        for case in range(120):
            owner=PortableServices(core);a,b=Vector(),Vector()
            for step in range(80):
                operation=(case+step)%9
                if operation==0:
                    wanted=(a.end-a.begin)//8+step%7;reserve(C.byref(a),wanted)
                    ok=core.fs15_scene_pointer_reserve(C.byref(b),wanted,C.byref(owner.api));counts['vector_reserve_calls']+=1
                elif operation==1:
                    grow(C.byref(a));ok=core.fs15_scene_pointer_grow(C.byref(b),C.byref(owner.api));counts['vector_grow_calls']+=1
                else:
                    used=(a.end-a.begin)//8;value=N(rng.getrandbits(64))
                    if operation%2 and used:
                        offset=rng.randrange(used)*8;pa,pb=a.begin+offset,b.begin+offset;counts['aliased_appends']+=1
                    else:pa=pb=C.addressof(value)
                    append(C.byref(a),pa);ok=core.fs15_scene_pointer_append(C.byref(b),pb,C.byref(owner.api));counts['vector_appends']+=1
                if not ok:raise AssertionError('Owned vector operation rejected')
                aa=((a.end-a.begin)//8,(a.capacity-a.begin)//8,C.string_at(a.begin,a.capacity-a.begin) if a.begin else b'')
                bb=((b.end-b.begin)//8,(b.capacity-b.begin)//8,C.string_at(b.begin,b.capacity-b.begin) if b.begin else b'')
                if aa!=bb:raise AssertionError('Owned vector length/capacity/complete allocation differs')
                digest.update(aa[2]);counts['complete_vector_allocations']+=1
            if a.begin:release(a.begin)
            if b.begin:owner.release(None,b.begin)
            if owner.blocks:raise AssertionError('Owned vector leaked')
        for case in range(120):
            owner=PortableServices(core);owner.sequence=case*100;C.c_uint32.from_address(native.base+0x1630d60).value=owner.sequence
            size=3+case%29;nodes=[[],[]];kinds=(C.c_uint8*size)(*([1]*size));observers=[]
            for i in range(size):
                a,b=allocate(0x170),owner.allocate(None,0x170);ctor(a,b'owned-node',0)
                if not core.fs15_scene_construct(b,0x170,3,b'owned-node',11,0,1,C.byref(owner.api)):raise AssertionError('Owned node construction rejected')
                nodes[0].append(a);nodes[1].append(b);owner.kinds[b]=3
            native_indices={p:i for i,p in enumerate(nodes[0])};portable_indices={p:i for i,p in enumerate(nodes[1])}
            table=(N*2)(0,C.cast(observe,P).value);events=(Notification*(size*4+8))();used=N();warning=C.c_int();attached=C.c_uint32()
            array=(P*size)(*nodes[1]);view=OwnedView(array,size,kinds,0)
            C.c_void_p.from_address(native.base+0x162cd18).value=nodes[0][0]
            @REBUILD
            def xml_rebuild(_,node):return core.fs15_scene_owned_trs(C.byref(view),portable_indices[node],1,None,0,events,len(events),C.byref(used),C.byref(warning))
            @PROPAGATE
            def xml_propagate(_,node,mask,origin):return core.fs15_scene_owned_state(C.byref(view),portable_indices[node],0,mask,origin,events,len(events),C.byref(used))
            @INTEGER
            def integer(_,text,out):out[0]=scalar_integer(text)&0xffffffff;return 1
            @REGISTER
            def registered(*_):return 1
            services=XmlServices(None,C.pointer(owner.api),xml_rebuild,xml_propagate,integer,registered)
            loader=C.create_string_buffer(0x900)
            for i,(a,b) in enumerate(zip(*nodes)):
                pairs=[('name',f'owned-{i}'),('translation',f'{i} {i%3} {-i}'),('rotation',f'{i*7} 0 45'),('scale','1.5 1 2'),('clipDistance',str(300-i)),('objectMask',str(0xffff-i))]
                if (case+i)%4==0:pairs.extend([('dynamic','true'),('density','2')])
                if (case+i)%7==0:pairs.extend([('joint','true'),('rotMaxX','90')])
                na,pa=attributes(pairs);common(a,loader,na,1,0,1)
                if not core.fs15_scene_xml_apply(b,pa,len(pa),1,0,1,b'',1,C.byref(services)):raise AssertionError('Owned common XML rejected')
                if i%3==0:
                    observer=(N*2)(C.addressof(table),i);observers.append(observer)
                    for node,side in ((a,0),(b,1)):
                        vector=allocate(8) if side==0 else owner.allocate(None,8);put(vector,0,C.addressof(observer))
                        for offset,value in ((0x58,vector),(0x60,vector+8),(0x68,vector+8)):put(node,offset,value)
                    rescan(a,1)
                    if not core.fs15_scene_owned_state(C.byref(view),i,1,1,0,events,len(events),C.byref(used)):raise AssertionError('Owned observer rescan rejected')
            def vector(p,mapping):
                begin,end,capacity=(read(p,offset) for offset in (0x30,0x38,0x40))
                return ((end-begin)//8,(capacity-begin)//8,tuple(mapping.get(read(begin,j),('opaque',read(begin,j))) for j in range(0,capacity-begin,8)))
            def compare():
                for i,(a,b) in enumerate(zip(*nodes)):
                    aa,bb=bytearray(canonical(C.string_at(a,0x170),3,nodes[0])),bytearray(canonical(C.string_at(b,0x170),3,nodes[1]))
                    for payload,pointer,mapping in ((aa,a,native_indices),(bb,b,portable_indices)):
                        struct.pack_into('<Q',payload,0x28,mapping.get(read(pointer,0x28),NONE))
                        for offset in (0x30,0x38,0x40,0x58,0x60,0x68):struct.pack_into('<Q',payload,offset,0)
                    for offset,length in ((0x98,16),(0xd8,16),(0x148,9)):
                        av,bv=struct.unpack_from('<'+'f'*length,aa,offset),struct.unpack_from('<'+'f'*length,bb,offset)
                        if any(abs(x-y)>1e-5*max(1,abs(x)) for x,y in zip(av,bv)):raise AssertionError(f'Owned transform differs {case}/{i}: {av}/{bv}')
                        aa[offset:offset+length*4]=bb[offset:offset+length*4]=bytes(length*4)
                    if aa!=bb:raise AssertionError(f'Owned node bytes differ {case}/{i}: {[hex(j) for j in range(len(aa)) if aa[j]!=bb[j]][:16]}')
                    if vector(a,native_indices)!=vector(b,portable_indices):raise AssertionError(f'Owned full child allocation differs {case}/{i}')
                    digest.update(aa);counts['complete_owned_node_records']+=1
                expected=trace;actual=[(events[j].node,events[j].observer,events[j].origin) for j in range(used.value)]
                if expected!=actual:raise AssertionError(f'Owned ordered notifications differ: {expected}/{actual}')
                counts['notification_events']+=len(actual);counts['complete_owned_forest_states']+=1
            for step in range(90):
                parents=[native_indices.get(read(p,0x28)) for p in nodes[0]];trace.clear();used.value=0;op=step%6
                if op<3:
                    child=1+rng.randrange(size-1);possible=[]
                    for p in range(size):
                        cursor=p
                        while cursor is not None and cursor!=child:cursor=parents[cursor]
                        if cursor is None:possible.append(p)
                    parent=rng.choice(possible);position=(0,1,0xffffffff)[step%3];matrix=step%2
                    expected=links[matrix](nodes[0][parent],nodes[0][child],position)
                    old_capacity=vector(nodes[1][parent],portable_indices)[1]
                    ok=core.fs15_scene_owned_link(C.byref(view),parent,child,position,matrix,C.byref(owner.api),events,len(events),C.byref(used),C.byref(attached))
                    if attached.value!=expected:raise AssertionError('Owned attached index differs')
                    if vector(nodes[1][parent],portable_indices)[1]>old_capacity:counts['child_vector_relocations']+=1
                    counts['matrix_links' if matrix else 'raw_links']+=1
                elif op==3:
                    nonempty=[i for i,p in enumerate(nodes[0]) if read(p,0x38)>read(p,0x30)]
                    if not nonempty:continue
                    parent=rng.choice(nonempty);slot=rng.randrange((read(nodes[0][parent],0x38)-read(nodes[0][parent],0x30))//8);suppress=case%2
                    unlink(nodes[0][parent],slot,suppress);ok=core.fs15_scene_owned_unlink(C.byref(view),parent,slot,suppress,events,len(events),C.byref(used));counts['unlinks']+=1
                elif op==4:
                    target=rng.randrange(size);mask=(1,2,4,8,0x10,0x20,0x3f)[case%7];origin=case%2
                    # Detached typed metadata remains opaque to world-preserving
                    # logic except its recovered first-16-byte property view.
                    state(nodes[0][target],mask,origin);ok=core.fs15_scene_owned_state(C.byref(view),target,0,mask,origin,events,len(events),C.byref(used));counts['state_updates']+=1
                else:
                    target=rng.randrange(size)
                    for p in (nodes[0][target],nodes[1][target]):put(p,0x148,float(step),'f')
                    rebuild(nodes[0][target]);ok=core.fs15_scene_owned_trs(C.byref(view),target,1,None,0,events,len(events),C.byref(used),C.byref(warning));counts['trs_rebuilds']+=1
                if not ok:raise AssertionError(f'Owned operation rejected {case}/{step}')
                compare()
            roots=[i for i,p in enumerate(nodes[0]) if not read(p,0x28)]
            for i in roots:
                destruct(nodes[0][i],1)
                if not core.fs15_scene_destruct(nodes[1][i],0x170,3,1,C.byref(owner.api)):raise AssertionError('Owned recursive cleanup rejected')
            if native.registry() or owner.registry or len(native.allocations)!=1 or owner.blocks:raise AssertionError('Owned forest leaked storage')
            counts['complete_owned_forest_lifetimes']+=1
        C.c_void_p.from_address(native.base+0x162cd18).value=0
    paths=tuple('src/core/'+p for p in ('scene_owned.cpp','scene_owned.h','scene_state.cpp','scene_state.h','scene_xml.cpp','scene_xml.h','scene_properties.cpp','scene_properties.h','scene_lifecycle.cpp','scene_lifecycle.h','scene_math.cpp','scene_math.h','scene_trs.cpp','scene_trs.h','scene_graph.cpp','scene_graph.h','shared_scene.cpp','shared_scene.h','scene_attributes.cpp','scene_attributes.h'))
    report=dict(status='passed',reference_sha256=EXPECTED_IMAGE,native_entries=['0x1400d5f30','0x1400d34e0','0x14005a1b0'],
        rebuilt_source_sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in paths},tested_dll_sha256=hashlib.sha256(args.dll.read_bytes()).hexdigest(),
        comparisons=dict(counts),original_results_sha256=digest.hexdigest(),
        scope='Recovered owned pointer-vector reserve/grow/alias-append and existing hierarchy/state/TRS paths on separately constructed, allocated objects with default/body/joint metadata. Complete capacities/spare slots and ordered side-effect-free notifications compared; float tolerance 1e-5. Scatter staging, bounds/failure guards, allocator/registry/dispatch transport authored. Complete I3D/LOD/derived-node callbacks, active physics, real observer effects and full engine startup remain open.')
    write_json(ROOT/'analysis/scene-owned-validation.json',report);print(json.dumps(report,indent=2))


if __name__=='__main__':main()
