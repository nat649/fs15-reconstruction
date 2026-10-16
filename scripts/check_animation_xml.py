"""Compare recovered GE6 numeric grammar and XML animation callback chains."""
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
from check_animation_resources import LoadServices
from check_shared_scene import Context,normalize_record
from check_scene_lifecycle import PortableServices,Services,DESTROY,P,N,read,put

DURATION=C.CFUNCTYPE(C.c_int,P,C.c_char_p,C.POINTER(C.c_float))
NODE_ID=C.CFUNCTYPE(C.c_int,P,C.c_char_p,C.POINTER(C.c_uint32))


class XmlServices(C.Structure):
    _fields_=[('resources',LoadServices),('duration',DURATION),('node_id',NODE_ID)]


class Attribute(C.Structure):
    _fields_=[('name',C.c_char_p),('name_bytes',N),('value',C.c_char_p),('value_bytes',N)]


class View(C.Structure):
    _fields_=[('set',P),('clip',P),('channel',P),('node_id',C.c_uint32),('reserved',C.c_uint32),('node_count',N),('rejected',N)]


def attributes(pairs):
    pairs=[(k.encode() if isinstance(k,str) else k,v.encode() if isinstance(v,str) else v) for k,v in pairs]
    native=(C.c_char_p*(len(pairs)*2+1))();portable=(Attribute*len(pairs))()
    for i,(key,value) in enumerate(pairs):native[i*2]=key;native[i*2+1]=value;portable[i]=Attribute(key,len(key)+1,value,len(value)+1 if value is not None else 0)
    return native,portable


def xml_events(root):
    output=[]
    for group in root.findall('./Animation/AnimationSets/AnimationSet'):
        output.append((0,list(group.attrib.items())))
        for clip in group.findall('Clip'):
            output.append((2,list(clip.attrib.items())))
            for channel in clip.findall('Keyframes'):
                output.append((4,list(channel.attrib.items())))
                output.extend((6,list(key.attrib.items())) for key in channel.findall('Keyframe'))
                output.append((5,[]))
            output.append((3,[]))
        output.append((1,[]))
    return output


def synthetic(rng,case):
    output=[];nodes=1+case%7
    for group in range(1+case%2):
        output.append((0,[('name','long-xml-character-set' if group%2 else 'xml-set')]))
        for clip in range(1+case%3):
            output.append((2,[('name','long-xml-animation-clip' if clip%2 else 'clip'),('duration','125.5')]))
            for node in range(nodes):
                controls=1+(case+clip+node)%15;output.append((4,[('nodeId',str(101+node))]))
                output.append((6,[('rotation','0 0 0')]))  # missing time: native no-op
                for key in range(1+case%6):
                    a=[('time',str(key*25))]
                    if controls&1:a.append(('translation',' '.join(f'{rng.uniform(-20,20):.7g}' for _ in range(3))))
                    if controls&2:a.append(('rotation',' '.join(f'{rng.uniform(-360,360):.7g}' for _ in range(3))))
                    if controls&4:a.append(('scale',' '.join(f'{rng.uniform(0.3,3):.7g}' for _ in range(3))))
                    if controls&8:a.append(('visibility',('FALSE','false','true','0','anything')[key%5]))
                    for name in ('iptin','iptout','iprin','iprout','ipsin','ipsout'):
                        if (case+key)%3:a.append((name,'linear' if (case+key)%3==1 else 'LINEAR'))
                    # Keep first duplicate value and case-sensitive attributes.
                    a.extend([('time','9999'),('Time','8888')]);output.append((6,a))
                    if key==0 and case%5==0:output.append((6,a))  # retained rejected key
                if case%7==0:
                    bad=[('time','100000')]
                    if controls!=1:bad.append(('translation','0 0 0'))
                    output.append((6,bad))  # controls mismatch, also caller-owned
                output.append((5,[]))
            output.append((3,[]))
        output.append((1,[]))
    return output


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--dll',type=Path,required=True);parser.add_argument('--i3d',action='append',default=[],type=Path);args=parser.parse_args()
    inventory=ROOT/'analysis/inventory.json'
    if not args.i3d and inventory.is_file():
        game=Path(json.loads(inventory.read_text())['game_root'])
        for name in ('krone/kroneSwadro2000','kuhn/kuhnGA8521','poettinger/poettingerTop1252'):
            path=game/'data/vehicles/tools'/f'{name}.i3d'
            if path.is_file():args.i3d.append(path)
    core=C.CDLL(str(args.dll.resolve()));rng=random.Random(0xf515199);counts=Counter();digest=hashlib.sha256();assets=[]
    signatures={'fs15_parse_scene_float':[P,N,P,C.POINTER(N)],'fs15_parse_scene_float_vector':[P,N,P,N,C.POINTER(N)],
        'fs15_animation_xml_create':[C.POINTER(XmlServices)],'fs15_animation_xml_event':[P,C.c_uint,P,N,C.POINTER(C.c_int)],
        'fs15_animation_xml_view':[P,C.POINTER(View)],'fs15_animation_xml_destroy':[P],
        'fs15_scene_construct':[P,N,C.c_uint,C.c_char_p,N,C.c_uint32,C.c_uint8,C.POINTER(Services)],
        'fs15_scene_destruct':[P,N,C.c_uint,C.c_uint32,C.POINTER(Services)],
        'fs15_shared_scene_release':[P,N,C.POINTER(Context),C.POINTER(Services)],
        'fs15_character_set_destruct':[P,N,C.c_uint32,C.POINTER(Context),C.POINTER(Services)]}
    for name,signature in signatures.items():getattr(core,name).argtypes=signature;getattr(core,name).restype=P if name.endswith('_create') else C.c_int
    @DURATION
    def duration(_,text,out):
        try:out[0]=float(text);return 1
        except (ValueError,OverflowError):return 0
    @NODE_ID
    def node_id(_,text,out):
        try:out[0]=max(-2147483648,min(2147483647,int(text)));return 1
        except ValueError:return 0
    with NativeAnimationXml() as native:
        scalar=C.WINFUNCTYPE(P,P,P)(native.base+0x26e60);vector=C.WINFUNCTYPE(C.c_uint32,P,P,C.c_uint32)(native.base+0x26850)
        samples=[b'',b' ',b'-0',b'  -12.345e-2tail',b'+12',b'1E3',b'1e+3',b'.25',b'-.25',b'1e-45',b'0e40',b'1e39',b'1e',b'\t12',b'1.00000011920928955078125']
        samples += [f'{rng.uniform(-1e6,1e6):.{case%12}f}e{rng.randrange(-50,51):+d}suffix'.encode() for case in range(12000)]
        for text in samples:
            source=C.create_string_buffer(text);a=C.c_float();b=C.c_float(73);cursor=N(9);end=scalar(source,C.byref(a))
            if not core.fs15_parse_scene_float(source,len(source),C.byref(b),C.byref(cursor)) or bytes(a)!=bytes(b) or cursor.value!=end-C.addressof(source):raise AssertionError(f'Native decimal grammar differs for {text!r}: {a.value}/{b.value}, cursor={cursor.value}/{end-C.addressof(source)}')
            digest.update(bytes(a));counts['scalar_float_results_and_cursors']+=1
        for case in range(6000):
            text=(['','1 2 3',' 1 2 3','1  2 3','1 2 ','1\t2 3','-0 .5 1e-45','1e39 0e40 -1e-45'][case%8] if case<80 else ' '.join(f'{rng.uniform(-50,50):.8g}' for _ in range(1+case%8))).encode()
            source=C.create_string_buffer(text);a=(C.c_float*10)(*([123]*10));b=(C.c_float*10)(*([123]*10));capacity=case%10;written=N(99)
            count=vector(source,a,capacity)
            if not core.fs15_parse_scene_float_vector(source,len(source),b,capacity,C.byref(written)) or written.value!=count or bytes(a)!=bytes(b):raise AssertionError(f'Native vector grammar differs for {text!r}, cap={capacity}')
            digest.update(bytes(a));counts['vector_float_frames']+=1

        callbacks=[C.WINFUNCTYPE(None,P,P,P)(native.base+entry) for entry in (0x199890,0x199970,0x199d70,0x199e90,0x199ec0,0x199fa0,0x19a100)]
        node_ctor=C.WINFUNCTYPE(P,P,C.c_char_p,C.c_uint32)(native.base+0x10fa90);node_dtor=C.WINFUNCTYPE(None,P)(native.base+0x10fbb0)
        def allocate(size):return native.callbacks[0](native.heap,0,size)
        def release(pointer):return native.callbacks[1](native.heap,0,pointer)
        def exact(a,b,size,pointers=()):
            av,bv=normalize_record(a,size,pointers),normalize_record(b,size,pointers)
            if av!=bv:raise AssertionError(f'XML native record differs: size={hex(size)}, offsets={[hex(i) for i in range(size) if av[i]!=bv[i]][:20]}')
            digest.update(av)
        def pointers(a,b,offset):
            av=[read(a,offset+j*8) for j in range(3)];bv=[read(b,offset+j*8) for j in range(3)]
            if (av[1]-av[0],av[2]-av[0])!=(bv[1]-bv[0],bv[2]-bv[0]):raise AssertionError('XML resource vector capacity differs')
            return zip([read(av[0],p) for p in range(0,av[1]-av[0],8)],[read(bv[0],p) for p in range(0,bv[1]-bv[0],8)])
        def channel_equal(a,b):
            exact(a,b,0xd0,(8,16,24))
            for ka,kb in pointers(a,b,8):
                flags=read(ka,4,'H');size=8+12*bool(flags&1)+8*bool(flags&2)+12*bool(flags&4);exact(ka,kb,size);counts['complete_xml_key_records']+=1
            counts['complete_xml_channel_records']+=1
        def clip_equal(a,b):
            exact(a,b,0x50,(0x30,0x38,0x40,8) if read(a,0x20)>15 else (0x30,0x38,0x40))
            if read(a,0x20)>15 and C.string_at(read(a,8),read(a,0x18)+1)!=C.string_at(read(b,8),read(b,0x18)+1):raise AssertionError('XML clip long name differs')
            for ca,cb in pointers(a,b,0x30):channel_equal(ca,cb)
        def set_equal(a,b):
            exact(a,b,0xd0,(0x10,0x48,0xa8,0xb0,0xb8));da,db=read(a,0x48),read(b,0x48)
            exact(da,db,0xc0,(0x18,0x20,0x28,0x38,0x40,0x48,0x58,0x60,0x68,0x78,0x80,0x88,0xa0,0xa8,0xb0))
            for offset in (0x18,0x38,0x58,0x78):
                aa=[read(da,offset+j*8) for j in range(3)];bb=[read(db,offset+j*8) for j in range(3)]
                if (aa[1]-aa[0],aa[2]-aa[0])!=(bb[1]-bb[0],bb[2]-bb[0]) or C.string_at(aa[0],aa[2]-aa[0])!=C.string_at(bb[0],bb[2]-bb[0]):raise AssertionError('XML bound default pose vector differs')
            for ca,cb in pointers(da,db,0xa0):clip_equal(ca,cb)
        cases=[(None,synthetic(rng,case)) for case in range(60)]+[(p,xml_events(ET.parse(p).getroot())) for p in args.i3d]
        for case,(path,events) in enumerate(cases):
            ids=sorted({int(v) for event,attrs in events if event==4 for k,v in attrs if k=='nodeId'})
            owner=PortableServices(core);owner.sequence=case*1000;C.c_uint32.from_address(native.base+0x1630d60).value=owner.sequence;context=Context()
            @DESTROY
            def shared(_,p):return core.fs15_shared_scene_release(p,0x10,C.byref(context),C.byref(owner.api))
            owner.api.release_shared=shared
            nodes=[{},{}]
            for source_id in ids:
                a,b=allocate(0x170),owner.allocate(None,0x170);node_ctor(a,b'xml-node',0)
                if not core.fs15_scene_construct(b,0x170,3,b'xml-node',9,0,1,C.byref(owner.api)):raise AssertionError('XML fixture node constructor failed')
                nodes[0][source_id]=a;nodes[1][source_id]=b
            # Read-only native scene node-ID tree; original XML ordinal map is
            # allocated/mutated by the unchanged native callback implementation.
            scene_head=C.create_string_buffer(0x30);scene_nil=C.addressof(scene_head);put(scene_nil,0x29,1,'B');tree_blocks=[]
            def tree(values,parent):
                if not values:return scene_nil
                mid=len(values)//2;key=values[mid];block=C.create_string_buffer(0x30);tree_blocks.append(block);p=C.addressof(block)
                put(p,0x18,key,'I');put(p,0x20,nodes[0][key]);put(p,8,parent);put(p,0,tree(values[:mid],p));put(p,16,tree(values[mid+1:],p));return p
            put(scene_nil,8,tree(ids,scene_nil));scene=C.create_string_buffer(0x900);base=C.addressof(scene);put(base,0x120,scene_nil)
            ordinal_head=allocate(0x28)
            for offset in (0,8,16):put(ordinal_head,offset,ordinal_head)
            put(ordinal_head,0x20,1,'B');put(ordinal_head,0x21,1,'B');put(base,0x7c8,ordinal_head)
            from check_animation_resources import RESOLVE,LOADED
            @RESOLVE
            def resolve(_,source_id):return nodes[1].get(source_id,0)
            completed=[]
            @LOADED
            def loaded(_,p,diagnostic):completed.append((read(p,0x18,'I') if p else None,diagnostic))
            resources=LoadServices(None,C.pointer(owner.api),C.pointer(context),resolve,loaded);services=XmlServices(resources,duration,node_id)
            state=core.fs15_animation_xml_create(C.byref(services))
            if not state:raise AssertionError('XML state creation failed')
            retained=[];rejected=[]
            for operation,pairs in events:
                a,b=attributes(pairs);before=set(native.allocations);end=read(read(base,0x7b0),16) if operation==6 and read(base,0x7b0) else None
                callbacks[operation](scene,None,a);diagnostic=C.c_int(-1)
                if not core.fs15_animation_xml_event(state,operation,b,len(b),C.byref(diagnostic)):raise AssertionError(f'XML callback rejected {case}/{operation}: {pairs}')
                view=View();core.fs15_animation_xml_view(state,C.byref(view))
                expected=[read(base,at) for at in (0x7a0,0x7a8,0x7b0)]
                if [bool(p) for p in expected]!=[bool(view.set),bool(view.clip),bool(view.channel)] or view.node_id!=read(base,0x7b8,'I') or view.node_count!=(read(base,0x7e8)-read(base,0x7e0))//4:raise AssertionError('XML callback state differs')
                if operation==6 and end is not None and read(read(base,0x7b0),16)==end:
                    leaked=set(native.allocations)-before
                    if leaked:rejected.extend(leaked);counts['retained_rejected_xml_keys']+=len(leaked)
                if view.rejected!=len(rejected):raise AssertionError('Rejected-key retained ownership differs')
                if expected[0]:set_equal(expected[0],view.set)
                if expected[1]:clip_equal(expected[1],view.clip)
                if expected[2]:channel_equal(expected[2],view.channel)
                original=native.registry()
                if original.keys()!=owner.registry.keys():raise AssertionError('XML object registry IDs differ')
                if operation==1:
                    for identifier,p in original.items():
                        if read(p)==0x1404c9678:set_equal(p,owner.registry[identifier])
                    counts['completed_xml_groups']+=1
                counts['xml_callback_'+str(operation)]+=1;counts['complete_xml_callback_states']+=1
            # Node-owned completed groups and explicitly owned zero-node groups.
            for source_id in ids:
                a,b=nodes[0][source_id],nodes[1][source_id];node_dtor(a)
                if not core.fs15_scene_destruct(b,0x170,3,0,C.byref(owner.api)):raise AssertionError('XML node cleanup failed')
                release(a);owner.release(None,b)
            for registry,side in ((native.registry(),0),(owner.registry,1)):
                for p in list(registry.values()):
                    if read(p)!=0x1404c9678 or read(p,0x30,'I'):raise AssertionError('Unexpected residual XML object')
                    if side==0:C.WINFUNCTYPE(P,P,C.c_uint32)(native.base+0x1750c0)(p,1)
                    elif not core.fs15_character_set_destruct(p,0xd0,1,C.byref(context),C.byref(owner.api)):raise AssertionError('Zero-node XML group cleanup failed')
            if not core.fs15_animation_xml_destroy(state):raise AssertionError('XML state cleanup failed')
            C.WINFUNCTYPE(None,P)(native.base+0xaa9a0)(base+0x7c0);release(ordinal_head)
            if read(base,0x7e0):release(read(base,0x7e0))
            for p in rejected:release(p)
            for offset in (0,0x20):
                p=read(native.base+0x162c2c0+offset)
                if p:release(p)
                C.memset(native.base+0x162c2c0+offset,0,24);p=read(C.addressof(context)+offset)
                if p:owner.release(None,p)
            if native.registry() or owner.registry or len(native.allocations)!=1 or owner.blocks:raise AssertionError('XML lifetime leaked owned/orphan storage')
            counts['complete_xml_lifetimes']+=1
            if path is not None:assets.append(dict(name=path.name,sha256=hashlib.sha256(path.read_bytes()).hexdigest(),groups=sum(op==0 for op,_ in events),clips=sum(op==2 for op,_ in events),channels=sum(op==4 for op,_ in events),keys=sum(op==6 for op,_ in events)))
    paths=('src/core/animation_xml.cpp','src/core/animation_xml.h','src/core/animation_storage.h','src/core/animation_resources.cpp','src/core/animation_resources.h',
        'src/core/animation.h','src/core/shared_scene.cpp','src/core/shared_scene.h','src/core/scene_lifecycle.cpp','src/core/scene_lifecycle.h','src/core/scene_math.cpp','src/core/scene_math.h','src/core/scene_trs.cpp','src/core/scene_trs.h')
    report=dict(status='passed',reference_sha256=EXPECTED_IMAGE,native_entries=['0x140026e60','0x140026850','0x140199890','0x140199970','0x140199d70','0x140199e90','0x140199ec0','0x140199fa0','0x14019a100'],
        rebuilt_source_sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in paths},tested_dll_sha256=hashlib.sha256(args.dll.read_bytes()).hexdigest(),
        comparisons=dict(counts),original_results_sha256=digest.hexdigest(),local_assets=assets,private_crt_fls=True,additional_kernel_imports=4,
        scope='Finite defined XML key callback payloads, exact decimal/vector grammar, nested resource construction/binding and retained rejected-key ownership. Original code uses private heap/FLS/C-locale fixtures without code-byte patches. Authored XML attribute/event/node-map transport, CRT conversion services and guards; original XML tokenizer, CRT startup, full scene/renderer/console execution remain open.')
    write_json(ROOT/'analysis/animation-xml-validation.json',report);print(json.dumps(report,indent=2))


if __name__=='__main__':main()
