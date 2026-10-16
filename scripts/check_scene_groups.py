"""Compare recovered TransformGroup/LOD XML callback chains with original GE6."""
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
from check_animation_xml import attributes
from check_scene_xml import XmlServices,REBUILD,PROPAGATE,INTEGER,REGISTER
from check_scene_lifecycle import PortableServices,Services,DESTROY,P,N,read,put,canonical
from check_scene_owned import Vector,OwnedView
from check_scene_lod import Context,Manager,LodServices,BASE,WORLD,MARK,Queue
from check_shared_scene import Context as SharedContext
from check_animation_resources import LoadServices,RESOLVE,LOADED
from check_animation_xml import XmlServices as AnimationXmlServices,DURATION,NODE_ID
from check_animation import Services as PlaybackServices,REFRESH,SETTER

CREATED=C.CFUNCTYPE(C.c_int,P,P,C.c_uint,N)
LINK=C.CFUNCTYPE(C.c_int,P,P,P,C.c_uint32)
class GroupServices(C.Structure):
    _fields_=[('common',XmlServices),('created',CREATED),('link',LINK)]
class Result(C.Structure):
    _fields_=[('node',P),('bytes',N),('kind',C.c_uint)]


def synthetic(rng,case):
    output=[]
    def group(depth,serial):
        pairs=[('name',f'group-{case}-{serial}'),('nodeId',str(serial%13)),('refId','888'),('translation',f'{serial%17}  {depth} {-serial%7}'),('rotation',f'0 {serial*7%360} 0'),('scale',f'{1+serial%3} 1 0.5'),('visibility',('false' if serial%5==0 else 'true'))]
        if serial%3!=0:pairs.append(('lodDistance',('','0','0  10 100',' 0 10  ','0 10 100 1000 5000')[serial%5]))
        if serial%4==0:pairs.extend([('dynamic','true'),('density','0.7'),('collisionMask','-1')])
        if serial%7==0:pairs.extend([('joint','true'),('driveSpring','2'),('rotMaxX','90')])
        if serial%2:pairs.reverse()  # includes lodDistance before name
        pairs.append(('name','ignored'))
        if any(key=='lodDistance' for key,_ in pairs):pairs.append(('lodDistance','999 999'))
        output.append((0,pairs))
        if depth<3:
            for i in range(1+serial%3):group(depth+1,serial*3+i+1)
        output.append((1,[]))
    for i in range(1+case%3):group(0,case*20+i+1)
    # Missing/null first name is ignored without allocating or moving current.
    output.extend([(0,[('Name','wrong-case')]),(0,[('name',None),('name','ignored')])])
    return output


def asset_cases():
    inventory=ROOT/'analysis/inventory.json'
    if not inventory.is_file():return [],[]
    game=Path(json.loads(inventory.read_text())['game_root']);cases=[];assets=[]
    for path in [game/'data/vehicles/tools/krone/kroneSwadro2000.i3d',ROOT/'recovered/gar/data/maps/map01.i3d',ROOT/'recovered/gar/data/maps/map02.i3d']:
        if not path.is_file():continue
        scene=ET.parse(path).getroot().find('Scene')
        if scene is None:continue
        supported={};selected=[]
        def classify(element):
            children=list(element);valid=element.tag=='TransformGroup' and all(supported.get(id(c),0)>0 for c in children)
            count=1+sum(supported.get(id(c),0) for c in children) if valid else 0
            supported[id(element)]=count if count<=80 else 0
        for element in reversed(list(scene.iter())):classify(element)
        def select(element):
            count=supported.get(id(element),0)
            if count>1:selected.append(element);return
            for child in element:select(child)
        select(scene);selected=selected[::max(1,len(selected)//18)][:24]
        # Only complete supported original subtrees are loaded. No Shape/Light/
        # Camera nodes are discarded and reparented to invent a supported map.
        if not selected:
            selected=[e for e in scene.iter('TransformGroup') if supported.get(id(e),0)==1][:12]
        def events(element,out):
            out.append((0,list(element.attrib.items())))
            for child in element:events(child,out)
            out.append((1,[]))
        for element in selected:
            out=[];events(element,out);cases.append((path,out))
        assets.append(dict(name=path.name,sha256=hashlib.sha256(path.read_bytes()).hexdigest(),complete_supported_subtrees=len(selected),group_callbacks=sum(len(ev) for p,ev in cases if p==path)))
    return cases,assets


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--dll',type=Path,required=True);args=parser.parse_args()
    core=C.CDLL(str(args.dll.resolve()));rng=random.Random(0xf51518c1a0);counts=Counter();digest=hashlib.sha256()
    signatures={'fs15_scene_float_reserve':[P,N,C.POINTER(Services)],'fs15_scene_float_grow':[P,N,C.POINTER(Services)],'fs15_scene_float_vector':[P,N,C.c_uint32,P,C.POINTER(Services)],
        'fs15_scene_group_begin':[C.POINTER(P),P,N,P,C.POINTER(Result)],'fs15_scene_group_end':[C.POINTER(P)],
        'fs15_scene_construct':[P,N,C.c_uint,C.c_char_p,N,C.c_uint32,C.c_uint8,C.POINTER(Services)],'fs15_scene_destruct':[P,N,C.c_uint,C.c_uint32,C.POINTER(Services)],
        'fs15_scene_owned_state_lod':[P,N,C.c_uint,C.c_uint32,C.c_int32,P,N,P,P],'fs15_scene_owned_trs_lod':[P,N,C.c_uint,P,N,P,N,P,P,P],
        'fs15_scene_owned_link_lod':[P,N,N,C.c_uint32,C.c_int,C.POINTER(Services),P,N,P,P,P],'fs15_scene_owned_world':[P,N,P,N],'fs15_scene_owned_mark_branch':[P],
        'fs15_scene_owned_matrix':[P,N,P,N],
        'fs15_scene_lod_enqueue':[P,P,C.POINTER(Services)],'fs15_scene_lod_update':[P,P,P],'fs15_scene_lod_destruct':[P,N,C.c_uint32,P,P],'fs15_scene_property_flags':[P,C.c_uint32,C.c_int,P],
        'fs15_animation_xml_create':[C.POINTER(AnimationXmlServices)],'fs15_animation_xml_event':[P,C.c_uint,P,N,P],'fs15_animation_xml_destroy':[P],
        'fs15_shared_scene_release':[P,N,C.POINTER(SharedContext),C.POINTER(Services)],'fs15_animation_set_time':[P,C.c_uint32,C.c_float],
        'fs15_animation_update':[P,C.c_float,C.POINTER(PlaybackServices)]}
    for name,signature in signatures.items():getattr(core,name).argtypes=signature;getattr(core,name).restype=C.c_int
    core.fs15_animation_xml_create.restype=P
    with NativeAnimationXml() as native:
        allocate=lambda size:native.callbacks[0](native.heap,0,size)
        release=lambda p:native.callbacks[1](native.heap,0,p)
        parse=C.WINFUNCTYPE(None,P,P,C.c_uint32)(native.base+0x266d0)
        reserve=C.WINFUNCTYPE(None,P,N)(native.base+0x281d0);grow=C.WINFUNCTYPE(None,P,N)(native.base+0x28150)
        begin=C.WINFUNCTYPE(None,P,P,P)(native.base+0x18c1a0);end=C.WINFUNCTYPE(None,P)(native.base+0x18c370)
        ctor=C.WINFUNCTYPE(P,P,C.c_char_p,C.c_uint32)(native.base+0x10fa90);deleting=C.WINFUNCTYPE(P,P,C.c_uint32)(native.base+0x10fb10)
        original_state=C.WINFUNCTYPE(None,P,C.c_uint32,C.c_int32)(native.base+0xb1b00)
        lod_update=C.WINFUNCTYPE(None,P)(native.base+0xd7ea0);integer_native=C.WINFUNCTYPE(C.c_int32,C.c_char_p)(native.base+0x3fd784)
        disable=C.WINFUNCTYPE(None,P,C.c_uint32)(native.base+0x9e5f0)
        C.WINFUNCTYPE(P)(native.base+0x524b0)()
        animation_callbacks=[C.WINFUNCTYPE(None,P,P,P)(native.base+entry) for entry in (0x199890,0x199970,0x199d70,0x199e90,0x199ec0,0x199fa0,0x19a100)]
        native_seek=C.WINFUNCTYPE(None,P,C.c_uint32,C.c_float)(native.base+0x1762a0);native_playback=C.WINFUNCTYPE(None,P,C.c_float)(native.base+0x175820)
        manager_pointer=native.base+0x162cc78;pending_pointer=native.base+0x162ccf8
        for case in range(100):
            owner=PortableServices(core);a,b=Vector(),Vector()
            for step in range(30):
                if step%7==0:
                    wanted=(a.end-a.begin)//4+step%5;reserve(C.byref(a),wanted);ok=core.fs15_scene_float_reserve(C.byref(b),wanted,C.byref(owner.api));counts['float_reserves']+=1
                elif step%7==1:
                    extra=step%11;grow(C.byref(a),extra);ok=core.fs15_scene_float_grow(C.byref(b),extra,C.byref(owner.api));counts['float_grows']+=1
                else:
                    text=(['',' ','0  10 100',' 0 10  ','1.25e-2 -3.5 6','0\t10 20'][step%6] if case<20 else ' '.join(f'{rng.uniform(-1000,1000):.7g}' for _ in range(1+step%8))).encode()
                    source=C.create_string_buffer(text);scan=len(text) if step%3 else rng.randrange(len(text)+1)
                    parse(source,C.byref(a),scan);ok=core.fs15_scene_float_vector(source,len(source),scan,C.byref(b),C.byref(owner.api));counts['float_parser_appends']+=1
                if not ok or (a.end-a.begin,a.capacity-a.begin,C.string_at(a.begin,a.capacity-a.begin) if a.begin else b'')!=(b.end-b.begin,b.capacity-b.begin,C.string_at(b.begin,b.capacity-b.begin) if b.begin else b''):raise AssertionError('Native LOD float-vector state differs')
                digest.update(C.string_at(a.begin,a.capacity-a.begin) if a.begin else b'');counts['complete_float_vector_states']+=1
            if a.begin:release(a.begin)
            if b.begin:owner.release(None,b.begin)
            if owner.blocks:raise AssertionError('Float-vector storage leaked')
        local,assets=asset_cases();cases=[(None,synthetic(rng,i)) for i in range(24)]+local
        for case,(path,events) in enumerate(cases):
            C.memset(manager_pointer,0,0x20);C.memset(pending_pointer,0,24);context=Context();shared_context=SharedContext();owner=PortableServices(core);nodes=[[],[]];kinds=[];lookup={};source_ids={}
            @DESTROY
            def shared(_,p):return core.fs15_shared_scene_release(p,0x10,C.byref(shared_context),C.byref(owner.api))
            owner.api.release_shared=shared
            owner.sequence=case*1000;C.c_uint32.from_address(native.base+0x1630d60).value=owner.sequence
            a,b=allocate(0x170),owner.allocate(None,0x170);ctor(a,b'scene-root',0)
            if not core.fs15_scene_construct(b,0x170,3,b'scene-root',11,0,1,C.byref(owner.api)):raise AssertionError('Group fixture root construction failed')
            nodes[0].append(a);nodes[1].append(b);kinds.append(1);lookup[b]=0;owner.kinds[b]=3
            loader=C.create_string_buffer(0x900);loader_pointer=C.addressof(loader);put(loader_pointer,0x580,a)
            head=allocate(0x30)
            for offset in (0,8,16):put(head,offset,head)
            put(head,0x28,1,'B');put(head,0x29,1,'B');put(loader_pointer,0x120,head)
            current=P(b);requests=(N*1024)();queue=Queue(requests,len(requests),0);used=N();warning=C.c_int();attached=C.c_uint32()
            def view():
                array=(P*len(nodes[1]))(*nodes[1]);kind=(C.c_uint8*len(kinds))(*kinds);return array,kind,OwnedView(array,len(array),kind,0)
            def enqueue():
                for i in range(queue.count):
                    if not core.fs15_scene_lod_enqueue(C.byref(context),nodes[1][requests[i]],C.byref(owner.api)):raise AssertionError('Group LOD enqueue failed')
                counts['lod_queue_requests']+=queue.count;queue.count=0
            @REBUILD
            def rebuild(_,p):
                array,kind,v=view();ok=core.fs15_scene_owned_trs_lod(C.byref(v),lookup[p],1,None,0,None,0,C.byref(used),C.byref(warning),C.byref(queue));enqueue();return ok
            @PROPAGATE
            def propagate(_,p,mask,origin):
                array,kind,v=view();ok=core.fs15_scene_owned_state_lod(C.byref(v),lookup[p],0,mask,origin,None,0,C.byref(used),C.byref(queue));enqueue();return ok
            @INTEGER
            def integer(_,text,out):out[0]=integer_native(text)&0xffffffff;return 1
            @REGISTER
            def registered(_,key,p):source_ids.setdefault(key,p);return 1
            @CREATED
            def created(_,p,kind,bytes):lookup[p]=len(nodes[1]);nodes[1].append(p);kinds.append(kind);owner.kinds[p]=3;return 1
            @LINK
            def link(_,parent,child,position):
                array,kind,v=view();ok=core.fs15_scene_owned_link_lod(C.byref(v),lookup[parent],lookup[child],position,1,C.byref(owner.api),None,0,C.byref(used),C.byref(attached),C.byref(queue));enqueue();return ok
            services=GroupServices(XmlServices(None,C.pointer(owner.api),rebuild,propagate,integer,registered),created,link)
            @WORLD
            def world(_,p,out):
                array,kind,v=view();return core.fs15_scene_owned_world(C.byref(v),lookup[p],out,3)
            @MARK
            def mark(_,p,token):return core.fs15_scene_owned_mark_branch(p)
            lod_services=LodServices(None,C.pointer(owner.api),BASE(),world,mark)
            @DESTROY
            def destroy(_,p):return core.fs15_scene_lod_destruct(p,0x188,1,C.byref(context),C.byref(lod_services)) if kinds[lookup[p]]==2 else core.fs15_scene_destruct(p,0x170,3,1,C.byref(owner.api))
            owner.api.destroy_child=destroy
            C.c_void_p.from_address(native.base+0x162cd18).value=a;original_state(a,2,0)
            if not propagate(None,b,2,0):raise AssertionError('Group fixture visible root failed')
            def tree(p):
                if read(p,0x29,'B'):return {}
                return {**tree(read(p)),read(p,0x18,'I'):read(p,0x20),**tree(read(p,16))}
            def compare():
                if len(nodes[0])!=len(nodes[1]):raise AssertionError('Group callback allocation count differs')
                maps=[{p:i+1 for i,p in enumerate(side)} for side in nodes]
                for i,(a,b) in enumerate(zip(*nodes)):
                    size=0x188 if kinds[i]==2 else 0x170
                    av,bv=bytearray(canonical(C.string_at(a,size),3,nodes[0])),bytearray(canonical(C.string_at(b,size),3,nodes[1]))
                    for offset in (0x30,0x38,0x40):av[offset:offset+8]=bv[offset:offset+8]=bytes(8)
                    struct.pack_into('<Q',av,0x120,bool(read(a,0x120)));struct.pack_into('<Q',bv,0x120,bool(read(b,0x120)))
                    for offset,length in ((0x98,16),(0xd8,16),(0x148,9)):
                        x,y=struct.unpack_from('<'+'f'*length,av,offset),struct.unpack_from('<'+'f'*length,bv,offset)
                        if any(abs(a-b)>1e-5*max(1,abs(a)) for a,b in zip(x,y)):raise AssertionError('Group loaded transform differs')
                        av[offset:offset+length*4]=bv[offset:offset+length*4]=bytes(length*4)
                    if av!=bv:raise AssertionError(f'Group callback node differs {case}/{i}: {[hex(j) for j in range(size) if av[j]!=bv[j]][:16]}')
                    def vector(p,mapping):
                        begin,end,capacity=(read(p,o) for o in (0x30,0x38,0x40))
                        return (end-begin,capacity-begin,tuple(mapping.get(read(begin,j),read(begin,j)) for j in range(0,capacity-begin,8)))
                    if vector(a,maps[0])!=vector(b,maps[1]) or C.string_at(read(a,0x10))!=C.string_at(read(b,0x10)):raise AssertionError('Group child allocation or name differs')
                    p,q=read(a,0x118),read(b,0x118);prop_size={0x1404c0ca8:0x10,0x1404c7a90:0x88,0x1404c8158:0x90}[read(p)]
                    pa,pb=bytearray(C.string_at(p,prop_size)),bytearray(C.string_at(q,prop_size))
                    at=0x28 if prop_size==0x88 else 0x20 if prop_size==0x90 else None
                    if at is not None:pa[at:at+8]=pb[at:at+8]=bytes(8)
                    if pa!=pb:raise AssertionError('Group pending physics metadata differs')
                    digest.update(av);digest.update(pa);counts['complete_group_node_records']+=1
                if maps[0].get(read(loader_pointer,0x580))!=maps[1].get(current.value):raise AssertionError('Group current-parent stack differs')
                if {key:maps[0][p] for key,p in tree(read(head,8)).items()}!={key:maps[1][p] for key,p in source_ids.items()}:raise AssertionError('Group source-ID registry differs')
                def pending(pointer,mapping):
                    v=Vector.from_address(pointer);return (v.end-v.begin,v.capacity-v.begin,tuple(mapping.get(read(v.begin,j),read(v.begin,j)) for j in range(0,v.capacity-v.begin,8)))
                if pending(pending_pointer,maps[0])!=pending(C.addressof(context)+0x20,maps[1]):raise AssertionError('Group LOD pending allocation differs')
                counts['complete_group_callback_states']+=1
            for operation,pairs in events:
                if operation==0:
                    na,pa=attributes(pairs);previous=read(loader_pointer,0x580);begin(loader,None,na);new=read(loader_pointer,0x580)
                    if new!=previous:nodes[0].append(new)
                    result=Result()
                    if not core.fs15_scene_group_begin(C.byref(current),pa,len(pa),C.byref(services),C.byref(result)):raise AssertionError(f'Group begin rejected {case}: {pairs}')
                    if bool(result.node)!=(new!=previous):raise AssertionError('Group ignored-name result differs')
                    if result.node:counts['constructed_lod_groups' if result.kind==2 else 'constructed_trs_groups']+=1
                    else:counts['ignored_group_begins']+=1
                else:
                    end(loader)
                    if not core.fs15_scene_group_end(C.byref(current)):raise AssertionError('Supported group end rejected')
                    counts['group_end_callbacks']+=1
                compare()
            candidates=[key for key,p in source_ids.items() if all(read(p,0x160+j*4,'f')!=0 for j in range(3))]
            if candidates:
                source_id=min(candidates,key=lambda key:0 if kinds[lookup[source_ids[key]]]==2 else 1)
                source_text=str(source_id if source_id<0x80000000 else source_id-0x100000000)
                ordinal_head=allocate(0x28)
                for offset in (0,8,16):put(ordinal_head,offset,ordinal_head)
                put(ordinal_head,0x20,1,'B');put(ordinal_head,0x21,1,'B');put(loader_pointer,0x7c8,ordinal_head)
                @DURATION
                def duration(_,text,out):out[0]=float(text);return 1
                @NODE_ID
                def node_id(_,text,out):out[0]=integer_native(text)&0xffffffff;return 1
                @RESOLVE
                def resolve(_,key):return source_ids.get(key,0)
                completed=[]
                @LOADED
                def loaded(_,p,diagnostic):completed.append((p,diagnostic))
                load_services=LoadServices(None,C.pointer(owner.api),C.pointer(shared_context),resolve,loaded)
                animation_services=AnimationXmlServices(load_services,duration,node_id);xml=core.fs15_animation_xml_create(C.byref(animation_services))
                if not xml:raise AssertionError('Loaded-scene animation state creation failed')
                sequence=[(0,[('name','loaded-scene')]),(2,[('name','move'),('duration','10')]),(4,[('nodeId',source_text)]),
                    (6,[('time','0'),('translation','0 0 0'),('iptin','linear'),('iptout','linear')]),
                    (6,[('time','10'),('translation','10 5 -3'),('iptin','linear'),('iptout','linear')]),(5,[]),(3,[]),(1,[])]
                for operation,pairs in sequence:
                    na,pa=attributes(pairs);animation_callbacks[operation](loader,None,na);diagnostic=C.c_int()
                    if not core.fs15_animation_xml_event(xml,operation,pa,len(pa),C.byref(diagnostic)):raise AssertionError('Animation callback on loaded group failed')
                    counts['loaded_scene_animation_callbacks']+=1
                original_sets=[p for p in native.registry().values() if read(p)==0x1404c9678]
                if len(original_sets)!=1 or len(completed)!=1 or completed[0][1]:raise AssertionError('Loaded-scene animation did not bind exactly once')
                sa,sb=original_sets[0],completed[0][0]
                for p in (sa,sb):put(p,0x58,3,'I');put(p,0x78,0,'f')
                @REFRESH
                def refresh(_,p):
                    array,kind,v=view();return core.fs15_scene_owned_trs_lod(C.byref(v),lookup[p],0,None,0,None,0,C.byref(used),C.byref(warning),C.byref(queue))
                @SETTER
                def matrix(_,p,m):
                    array,kind,v=view();return core.fs15_scene_owned_matrix(C.byref(v),lookup[p],m,16)
                playback=PlaybackServices(None,refresh,matrix,propagate)
                for time in (0,2.5,5,7.5,10):
                    native_seek(sa,0,time)
                    if not core.fs15_animation_set_time(sb,0,time):raise AssertionError('Loaded-scene animation seek failed')
                    native_playback(sa+0x38,0)
                    if not core.fs15_animation_update(sb,0,C.byref(playback)):raise AssertionError('Loaded-scene animation update failed')
                    if C.string_at(sa+0x50,0x58)!=C.string_at(sb+0x50,0x58):raise AssertionError('Loaded-scene animation controls differ')
                    compare();counts['loaded_scene_animation_updates']+=1
                if not core.fs15_animation_xml_destroy(xml):raise AssertionError('Loaded-scene animation transport cleanup failed')
                C.WINFUNCTYPE(None,P)(native.base+0xaa9a0)(loader_pointer+0x7c0);release(ordinal_head)
                if read(loader_pointer,0x7e0):release(read(loader_pointer,0x7e0))
            # Post-load queued LOD updates use the newly loaded hierarchy.
            original_pending=Vector.from_address(pending_pointer)
            for i in range((original_pending.end-original_pending.begin)//8):
                p,q=read(original_pending.begin,i*8),read(context.pending.begin,i*8);lod_update(p)
                if not core.fs15_scene_lod_update(q,C.byref(context),C.byref(lod_services)):raise AssertionError('Loaded LOD resource update failed')
                counts['loaded_lod_resource_updates']+=1
            original_pending.end=original_pending.begin;context.pending.end=context.pending.begin;compare()
            ma,mb=Manager.from_address(manager_pointer),context.manager
            av,bv=bytearray(C.string_at(manager_pointer,0x20)),bytearray(bytes(mb));av[:8]=bv[:8]=bytes(8)
            if av!=bv:raise AssertionError('Loaded-scene LOD manager header differs')
            maps=[{p:i+1 for i,p in enumerate(side)} for side in nodes]
            av,bv=bytearray(C.string_at(ma.entries,ma.capacity*0x28) if ma.entries else b''),bytearray(C.string_at(mb.entries,mb.capacity*0x28) if mb.entries else b'')
            for i in range(ma.capacity):
                for payload,mapping in ((av,maps[0]),(bv,maps[1])):
                    value=struct.unpack_from('<Q',payload,i*0x28)[0];struct.pack_into('<Q',payload,i*0x28,mapping.get(value,value))
                if i<ma.count:
                    x,y=struct.unpack_from('<3f',av,i*0x28+8),struct.unpack_from('<3f',bv,i*0x28+8)
                    if any(abs(a-b)>1e-5*max(1,abs(a)) for a,b in zip(x,y)):raise AssertionError('Animated loaded LOD world position differs')
                    av[i*0x28+8:i*0x28+20]=bv[i*0x28+8:i*0x28+20]=bytes(12)
            if av!=bv:raise AssertionError('Loaded-scene complete LOD manager allocation differs')
            counts['loaded_lod_manager_allocations']+=1
            if read(loader_pointer,0x580)!=nodes[0][0] or current.value!=nodes[1][0]:raise AssertionError('Loaded group callbacks did not return to root')
            for a,b in zip(*nodes):
                p,q=read(a,0x118),read(b,0x118)
                if read(p)==0x1404c7a90 and read(p,8,'I')&0x1000:disable(p,0x1000);core.fs15_scene_property_flags(q,0x1000,0,None)
            deleting(nodes[0][0],1)
            if not core.fs15_scene_destruct(nodes[1][0],0x170,3,1,C.byref(owner.api)):raise AssertionError('Loaded group forest cleanup failed')
            def free_tree(p):
                if read(p,0x29,'B'):return
                free_tree(read(p));free_tree(read(p,16));release(p)
            free_tree(read(head,8));release(head)
            for p in (read(manager_pointer),read(pending_pointer)):
                if p:release(p)
            for p in (context.manager.entries,context.pending.begin):
                if p:owner.release(None,p)
            for offset in (0,0x20):
                p=read(native.base+0x162c2c0+offset)
                if p:release(p)
                C.memset(native.base+0x162c2c0+offset,0,24);p=read(C.addressof(shared_context)+offset)
                if p:owner.release(None,p)
            if owner.blocks or owner.registry or native.registry() or len(native.allocations)!=1:raise AssertionError('Group XML callback ownership leaked')
            counts['complete_group_forest_lifetimes']+=1
            if path:counts['original_asset_group_subtrees']+=1
        C.c_void_p.from_address(native.base+0x162cd18).value=0;C.memset(manager_pointer,0,0x20);C.memset(pending_pointer,0,24)
    paths=tuple('src/core/'+p for p in ('scene_groups.cpp','scene_groups.h','scene_xml.cpp','scene_xml.h','animation_xml.cpp','animation_xml.h','animation.cpp','animation.h','animation_resources.cpp','animation_resources.h','animation_storage.h','scene_owned.cpp','scene_owned.h','scene_lod.cpp','scene_lod.h','scene_state.cpp','scene_state.h','scene_lifecycle.cpp','scene_lifecycle.h','scene_math.cpp','scene_math.h','scene_graph.cpp','scene_graph.h','scene_trs.cpp','scene_trs.h','scene_properties.cpp','scene_properties.h','shared_scene.cpp','shared_scene.h'))
    report=dict(status='passed',reference_sha256=EXPECTED_IMAGE,native_entries=['0x1400266d0','0x140028150','0x1400281d0','0x14018c1a0','0x14018c370'],rebuilt_source_sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in paths},tested_dll_sha256=hashlib.sha256(args.dll.read_bytes()).hexdigest(),comparisons=dict(counts),original_results_sha256=digest.hexdigest(),local_assets=assets,
        scope='Decoded TransformGroup/LOD begin and non-shape end callbacks; original-order construction, common attributes, source IDs, owned attachment, animation binding/playback and post-load LOD/recursive cleanup. Complete supported original subtrees retain original attributes/hierarchy; other derived nodes are not reinterpreted. Float-vector capacities/spare bytes and loaded LOD manager allocations compared. Record pointers normalized; hierarchy libm tolerance 1e-5. XML tokenizer, complete loader, shape finalization, derived renderer/physics services and full engine startup remain open.')
    write_json(ROOT/'analysis/scene-groups-validation.json',report);print(json.dumps(report,indent=2))


if __name__=='__main__':main()
