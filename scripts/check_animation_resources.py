"""Compare recovered GE6 resource construction and whole binary animation loads.

Pinned original code runs only in an isolated private-heap process. Node-ID lookup
uses a read-only synthetic native map; the portable side uses an authored resolver.
Original assets are optional local inputs and are never copied into the repository.
"""
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
from check_shared_scene import NativeShared,Context,normalize_record
from check_scene_lifecycle import PortableServices,Services,DESTROY,P,N,read,put
from check_scene_state import Arena,Notification
from check_animation import Services as PlaybackServices,REFRESH,SETTER,PROPAGATE

RESOLVE=C.CFUNCTYPE(P,P,C.c_uint32)
LOADED=C.CFUNCTYPE(None,P,P,C.c_uint32)
ENTRIES=[0x2805b0,0x1cb5c0,0x1bd190,0x1bd2f0,0x1768b0,0x1751e0,0x1ca840]


class LoadServices(C.Structure):
    _fields_=[('user',P),('memory',C.POINTER(Services)),('context',C.POINTER(Context)),('resolve',RESOLVE),('loaded',LOADED)]


class Stats(C.Structure):
    _fields_=[('consumed',N),('groups',C.c_uint32),('loaded',C.c_uint32),('skipped',C.c_uint32),('warnings',C.c_uint32)]


def floats(values):return (C.c_float*len(values))(*values)
def vector(pointer,offset,storage):
    begin=C.addressof(storage) if storage is not None else 0;size=C.sizeof(storage) if storage is not None else 0
    for at,v in ((offset,begin),(offset+8,begin+size),(offset+16,begin+size)):put(pointer,at,v)


def decode(data):
    """Independent fixture inventory, also used to build the resolver map."""
    offset=0;groups=[]
    def u():
        nonlocal offset
        n=struct.unpack_from('>I',data,offset)[0];offset+=4;return n
    def number():return struct.unpack('>f',struct.pack('>I',u()))[0]
    def text():
        nonlocal offset
        n=u();value=data[offset:offset+n];offset+=(n+3)&~3;return value
    version=u()
    for _ in range(u()):
        group={'name':text(),'clips':[]}
        for _ in range(u()):
            clip={'name':text(),'duration':number(),'channels':[]}
            for _ in range(u()):
                channel={'node':u(),'keys':[]}
                for _ in range(u()):
                    key={'time':number(),'flags':u()}
                    for bit,label in ((1,'translation'),(2,'rotation'),(4,'scale')):
                        if key['flags']&bit:key[label]=[number() for _ in range(3)]
                    channel['keys'].append(key)
                clip['channels'].append(channel)
            group['clips'].append(clip)
        groups.append(group)
    if offset!=len(data):raise AssertionError('Fixture inventory did not consume the complete animation')
    return version,groups


def encode(groups,version=3):
    parts=[struct.pack('>II',version,len(groups))]
    def u(n):parts.append(struct.pack('>I',n))
    def text(s):u(len(s));parts.append(s+bytes((-len(s))%4))
    def number(f):parts.append(struct.pack('>f',f))
    for group in groups:
        text(group['name']);u(len(group['clips']))
        for clip in group['clips']:
            text(clip['name']);number(clip['duration']);u(len(clip['channels']))
            for channel in clip['channels']:
                u(channel['node']);u(len(channel['keys']))
                for key in channel['keys']:
                    number(key['time']);u(key['flags'])
                    for bit,label in ((1,'translation'),(2,'rotation'),(4,'scale')):
                        if key['flags']&bit:
                            for v in key[label]:number(v)
    return b''.join(parts)


def fixture(rng,case):
    groups=[];count=1+case%9
    # Later groups deliberately reuse nodes, exercising old-set destruction.
    for g in range(1+case%3):
        group={'name':(b'short' if g%2 else b'long-animation-character-set'),'clips':[]}
        for j in range(1+case%3):
            duration=2+case%17;clip={'name':b'clip' if j%2 else b'long-animation-clip-name','duration':duration,'channels':[]}
            for i in range(count):
                flags=(1,2,4,8,3,5,7,15)[(i+case+j)%8];channel={'node':(i+1)*101,'keys':[]}
                # Empty serialized channels are discarded before ordinal assignment.
                number=0 if i==count-1 and case%11==0 else 1+(case+i)%7
                for k in range(number):
                    key={'time':duration*k/max(1,number-1),'flags':flags|(rng.getrandbits(6)<<4)|(0x8000 if k%2 else 0),
                        'translation':[rng.uniform(-20,20) for _ in range(3)],'rotation':[rng.uniform(-3,3) for _ in range(3)],'scale':[rng.uniform(0.3,3) for _ in range(3)]}
                    channel['keys'].append(key)
                clip['channels'].append(channel)
            group['clips'].append(clip)
        groups.append(group)
    return encode(groups,2 if case%2 else 3)


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--dll',required=True,type=Path);parser.add_argument('--anim',action='append',type=Path,default=[])
    args=parser.parse_args();core=C.CDLL(str(args.dll.resolve()))
    inventory=ROOT/'analysis/inventory.json'
    if not args.anim and inventory.is_file():
        from local_reference_assets import reference_asset
        candidate=reference_asset('data/vehicles/tools/poettinger/poettingerTop1252.i3d.anim')
        if candidate.is_file():args.anim.append(candidate)
    signatures={'fs15_animation_euler_quaternion':[P,P,N],
        'fs15_animation_key_create':[C.c_float,C.c_uint32,P,P,P,C.c_int,C.POINTER(Services),C.POINTER(P)],
        'fs15_animation_channel_add_key':[P,P,C.POINTER(Services),C.POINTER(C.c_int)],'fs15_animation_channel_finalize':[P],
        'fs15_animation_clip_add_channel':[P,P,C.POINTER(Services)],'fs15_animation_set_add_clip':[P,P,C.POINTER(Services)],
        'fs15_animation_load_binary':[P,N,C.c_int,C.POINTER(LoadServices),C.POINTER(Stats)],
        'fs15_scene_construct':[P,N,C.c_uint,C.c_char_p,N,C.c_uint32,C.c_uint8,C.POINTER(Services)],
        'fs15_scene_destruct':[P,N,C.c_uint,C.c_uint32,C.POINTER(Services)],
        'fs15_shared_scene_release':[P,N,C.POINTER(Context),C.POINTER(Services)],
        'fs15_character_set_destruct':[P,N,C.c_uint32,C.POINTER(Context),C.POINTER(Services)],
        'fs15_animation_update':[P,C.c_float,C.POINTER(PlaybackServices)],
        'fs15_animation_set_time':[P,C.c_uint32,C.c_float],
        'fs15_scene_set_local_matrix':[C.POINTER(Arena),N,P,N],
        'fs15_scene_state_update':[C.POINTER(Arena),N,C.c_uint,C.c_uint32,C.c_int32,P,N,C.POINTER(N)],
        'fs15_scene_trs_update':[C.POINTER(Arena),N,C.c_uint,P,N,P,N,C.POINTER(N),C.POINTER(C.c_int)]}
    for name,signature in signatures.items():getattr(core,name).argtypes=signature;getattr(core,name).restype=C.c_int
    rng=random.Random(0xf5151ca);counts=Counter();digest=hashlib.sha256();errors={};assets=[]
    def compare(kind,a,b):
        maximum=errors.get(kind,0)
        for x,y in zip(a,b):
            error=abs(x-y)/max(1,abs(x));maximum=max(maximum,error)
            if not math.isfinite(x) or not math.isfinite(y) or error>8e-6:raise AssertionError(f'{kind} differs: {x} vs {y}')
        errors[kind]=maximum;counts[kind]+=1
        if struct.pack('<'+'f'*len(a),*a)==struct.pack('<'+'f'*len(b),*b):counts[kind+'_bit_exact']+=1

    with NativeShared() as native:
        C.WINFUNCTYPE(P)(native.base+0x524b0)()
        euler=C.WINFUNCTYPE(P,P,P)(native.base+0x2805b0)
        key_create=C.WINFUNCTYPE(P,C.c_float,C.c_uint32,P,P,P,C.c_uint8)(native.base+0x1cb5c0)
        insert=C.WINFUNCTYPE(None,P,P)(native.base+0x1bd190);finalize=C.WINFUNCTYPE(None,P)(native.base+0x1bd2f0)
        load=C.WINFUNCTYPE(None,P,P)(native.base+0x1ca840)
        node_ctor=C.WINFUNCTYPE(P,P,C.c_char_p,C.c_uint32)(native.base+0x10fa90)
        node_dtor=C.WINFUNCTYPE(None,P)(native.base+0x10fbb0)
        seek=C.WINFUNCTYPE(None,P,C.c_uint32,C.c_float)(native.base+0x1762a0)
        update=C.WINFUNCTYPE(None,P,C.c_float)(native.base+0x175820)
        def allocate(size):return native.callbacks[0](native.heap,0,size)
        def release(p):return native.callbacks[1](native.heap,0,p)
        for case in range(4000):
            r=floats([rng.uniform(-30,30) for _ in range(3)]);a,b=floats([0]*4),floats([0]*4);euler(a,r)
            if not core.fs15_animation_euler_quaternion(r,b,4):raise AssertionError('Euler conversion rejected')
            compare('euler_quaternions',list(a),list(b));digest.update(bytes(a))
            owner=PortableServices(core);flags=case%16|(rng.getrandbits(6)<<4);time=C.c_float(rng.uniform(-50,500));t=floats([rng.uniform(-30,30) for _ in range(3)]);q=floats([rng.uniform(-1,1) for _ in range(4)]);s=floats([rng.uniform(0.3,4) for _ in range(3)]);visible=case%3
            original=key_create(time,flags,t,q,s,visible);rebuilt=P()
            if not core.fs15_animation_key_create(time,flags,t,q,s,visible,C.byref(owner.api),C.byref(rebuilt)):raise AssertionError('Key packing rejected')
            size=8+12*bool(flags&1)+8*bool(flags&2)+12*bool(flags&4)
            if C.string_at(original,size)!=C.string_at(rebuilt,size):raise AssertionError('Packed key bytes differ')
            digest.update(C.string_at(original,size));counts['packed_key_allocations']+=1;release(original);owner.release(None,rebuilt.value)
        for case in range(220):
            owner=PortableServices(core);channels=[allocate(0xd0),owner.allocate(None,0xd0)]
            for channel in channels:
                C.memset(channel,0xa5,0xd0);C.memset(channel+8,0,24);put(channel,0,0,'I');C.memset(channel+0xb0,0,12);C.memset(channel+0xc0,0,16)
            flags=(case%15)+1;count=1+case%51
            for j in range(count):
                t=floats([j,j+1,j+2]);q=floats([1,0,0,0]);s=floats([1,1,1]);keys=[key_create(float(j),flags,t,q,s,1),P()]
                if not core.fs15_animation_key_create(float(j),flags,t,q,s,1,C.byref(owner.api),C.byref(keys[1])):raise AssertionError('Key construction failed')
                keys[1]=keys[1].value;insert(channels[0],keys[0]);diagnostic=C.c_int()
                if not core.fs15_animation_channel_add_key(channels[1],keys[1],C.byref(owner.api),C.byref(diagnostic)) or diagnostic.value:raise AssertionError('Ordered insertion rejected')
                for pointer in channels:finalize(pointer) if pointer==channels[0] else None
                if not core.fs15_animation_channel_finalize(channels[1]):raise AssertionError('Lookup finalization rejected')
                if normalize_record(channels[0],0xd0,(8,16,24))!=normalize_record(channels[1],0xd0,(8,16,24)):raise AssertionError('Whole channel differs after insertion/lookup generation')
                for offset in (16,24):
                    if read(channels[0],offset)-read(channels[0],8)!=read(channels[1],offset)-read(channels[1],8):raise AssertionError('Key capacity differs')
                counts['channel_insert_finalize_records']+=1
            # Both native diagnostics must retain the caller's rejected key.
            for time,badflags,expected in ((0,flags,1),(count,flags^1,2)):
                key=key_create(float(time),badflags,t,q,s,0);copy=P()
                core.fs15_animation_key_create(float(time),badflags,t,q,s,0,C.byref(owner.api),C.byref(copy));insert(channels[0],key);diagnostic=C.c_int()
                if not core.fs15_animation_channel_add_key(channels[1],copy,C.byref(owner.api),C.byref(diagnostic)) or diagnostic.value!=expected:raise AssertionError('Insertion diagnostic differs')
                if normalize_record(channels[0],0xd0,(8,16,24))!=normalize_record(channels[1],0xd0,(8,16,24)):raise AssertionError('Rejected key changed channel differently')
                release(key);owner.release(None,copy.value);counts['rejected_key_diagnostics']+=1
            for side,channel in enumerate(channels):
                begin,end=read(channel,8),read(channel,16)
                for at in range(begin,end,8):release(read(at)) if side==0 else owner.release(None,read(at))
                release(begin) if side==0 else owner.release(None,begin)
                release(channel) if side==0 else owner.release(None,channel)
            if owner.blocks:raise AssertionError('Key fixture leaked storage')

        inputs=[(None,fixture(rng,case)) for case in range(90)]+[(path,path.read_bytes()) for path in args.anim]
        for case,(path,payload) in enumerate(inputs):
            _,groups=decode(payload);ids=sorted({ch['node'] for g in groups for c in g['clips'] for ch in c['channels'] if ch['keys']})
            owner=PortableServices(core);context=Context();owner.sequence=case*1000;C.c_uint32.from_address(native.base+0x1630d60).value=owner.sequence
            @DESTROY
            def shared(_,pointer):return core.fs15_shared_scene_release(pointer,0x10,C.byref(context),C.byref(owner.api))
            owner.api.release_shared=shared
            nodes=[{},{}]
            for source_id in ids:
                pair=[allocate(0x170),owner.allocate(None,0x170)];node_ctor(pair[0],b'animation-node',0)
                if not core.fs15_scene_construct(pair[1],0x170,3,b'animation-node',15,0,1,C.byref(owner.api)):raise AssertionError('Node construction failed')
                for side in range(2):nodes[side][source_id]=pair[side]
            sentinel=C.create_string_buffer(0x30);sentinel_address=C.addressof(sentinel);put(sentinel_address,0x29,1,'B');entries=[]
            def tree(values,parent):
                if not values:return sentinel_address
                middle=len(values)//2;key=values[middle];entry=C.create_string_buffer(0x30);entries.append(entry);p=C.addressof(entry)
                put(p,0x18,key,'I');put(p,0x20,nodes[0][key]);put(p,8,parent);put(p,0,tree(values[:middle],p));put(p,16,tree(values[middle+1:],p));return p
            put(sentinel_address,8,tree(ids,sentinel_address));scene=C.create_string_buffer(0x18);put(C.addressof(scene),8,sentinel_address);source=C.create_string_buffer(payload)
            @RESOLVE
            def resolve(_,source_id):return nodes[1].get(source_id,0)
            observed=[]
            @LOADED
            def loaded(_,pointer,diagnostic):observed.append((read(pointer,0x18,'I') if pointer else None,diagnostic))
            services=LoadServices(None,C.pointer(owner.api),C.pointer(context),resolve,loaded);stats=Stats()
            load(scene,C.addressof(source)+4)
            if not core.fs15_animation_load_binary(source,len(payload),1,C.byref(services),C.byref(stats)):raise AssertionError(f'Binary loading rejected case {case}')
            if stats.consumed!=len(payload) or stats.loaded!=len(groups) or stats.skipped or stats.warnings or any(d for _,d in observed):raise AssertionError('Load result differs')
            counts['constructed_binary_groups']+=len(groups)
            original=native.registry();rebuilt=owner.registry
            if original.keys()!=rebuilt.keys():raise AssertionError('Object registry IDs differ')
            def bytes_equal(a,b,size,pointers=()):
                x,y=normalize_record(a,size,pointers),normalize_record(b,size,pointers)
                if x!=y:raise AssertionError(f'Native record bytes differ: size={size}, offsets={[hex(i) for i in range(size) if x[i]!=y[i]][:24]}')
                digest.update(x)
            def vec_equal(a,b,offset,step,numeric=False,pointer=False):
                av=[read(a,offset+j*8) for j in range(3)];bv=[read(b,offset+j*8) for j in range(3)]
                used=av[1]-av[0];cap=av[2]-av[0]
                if (used,cap)!=(bv[1]-bv[0],bv[2]-bv[0]):raise AssertionError('Vector extents differ')
                if pointer:return [read(av[0],j) for j in range(0,used,step)],[read(bv[0],j) for j in range(0,used,step)]
                x,y=C.string_at(av[0],cap) if cap else b'',C.string_at(bv[0],cap) if cap else b''
                if numeric:
                    compare('default_pose_vectors',struct.unpack('<'+'f'*(used//4),x[:used]),struct.unpack('<'+'f'*(used//4),y[:used]))
                    if x[used:]!=y[used:]:raise AssertionError('Unused pose capacity differs')
                elif x!=y:raise AssertionError('Vector payload differs')
                digest.update(x);return [],[]
            for identifier in original:
                a,b=original[identifier],rebuilt[identifier]
                if read(a)!=read(b):raise AssertionError('Native class token differs')
                if read(a)!=0x1404c9678:
                    bytes_equal(a,b,0x170,(0x10,0x80,0x118,0x120));continue
                counts['complete_loaded_character_sets']+=1
                bytes_equal(a,b,0xd0,(0x10,0x48,0xa8,0xb0,0xb8))
                da,db=read(a,0x48),read(b,0x48)
                bytes_equal(da,db,0xc0,(0x18,0x20,0x28,0x38,0x40,0x48,0x58,0x60,0x68,0x78,0x80,0x88,0xa0,0xa8,0xb0))
                for offset,step,numeric in ((0x18,12,True),(0x38,16,True),(0x58,12,True),(0x78,4,False)):vec_equal(da,db,offset,step,numeric)
                ta,tb=vec_equal(a,b,0xa8,8,pointer=True)
                reverse=[{p:k for k,p in nodes[side].items()} for side in range(2)]
                if [reverse[0][p] for p in ta]!=[reverse[1][p] for p in tb]:raise AssertionError('Bound node order differs')
                ca,cb=vec_equal(da,db,0xa0,8,pointer=True)
                for ac,bc in zip(ca,cb):
                    long_name=read(ac,0x20)>15;bytes_equal(ac,bc,0x50,(0x30,0x38,0x40,8) if long_name else (0x30,0x38,0x40))
                    if long_name and C.string_at(read(ac,8),read(ac,0x18)+1)!=C.string_at(read(bc,8),read(bc,0x18)+1):raise AssertionError('Long clip name differs')
                    cha,chb=vec_equal(ac,bc,0x30,8,pointer=True)
                    for ach,bch in zip(cha,chb):
                        bytes_equal(ach,bch,0xd0,(8,16,24));ka,kb=vec_equal(ach,bch,8,8,pointer=True)
                        for ak,bk in zip(ka,kb):
                            flags=read(ak,4,'H');size=8+12*bool(flags&1)+8*bool(flags&2)+12*bool(flags&4);bytes_equal(ak,bk,size)
                            counts['loaded_key_records']+=1
                        counts['loaded_channel_records']+=1
                    counts['loaded_clip_records']+=1
            # Exercise the actual loaded graphs through both evaluators, not
            # only their construction/storage. Each fixture node is a detached
            # TRS root, so one-record arenas supply the recovered scene services.
            callback_errors=[];node_kind=C.c_uint8(1)
            def view(node):return Arena(node,0x170,0x170,None,0,None,0,read(node,0x118),1,C.addressof(node_kind),1,N(-1).value)
            @REFRESH
            def refresh(_,node):
                arena=view(node);used=N();warning=C.c_int()
                ok=core.fs15_scene_trs_update(C.byref(arena),0,0,None,0,None,0,C.byref(used),C.byref(warning))
                if not ok:callback_errors.append('refresh')
                return ok
            @SETTER
            def matrix(_,node,m):
                arena=view(node);ok=core.fs15_scene_set_local_matrix(C.byref(arena),0,m,16)
                if not ok:callback_errors.append('matrix')
                return ok
            @PROPAGATE
            def propagate(_,node,mask,origin):
                arena=view(node);used=N();ok=core.fs15_scene_state_update(C.byref(arena),0,0,mask,origin,None,0,C.byref(used))
                if not ok or origin:callback_errors.append('propagate')
                return ok
            playback=PlaybackServices(None,refresh,matrix,propagate)
            for identifier,a in original.items():
                if read(a)!=0x1404c9678:continue
                b=rebuilt[identifier];duration=read(read(read(read(a,0x48),0xa0)),0,'f')
                for pointer in (a,b):put(pointer,0x54,7,'I');put(pointer,0x64,0,'I');put(pointer,0x74,0,'f');put(pointer,0x94,1,'f')
                for part in (0,0.125,0.5,0.875,1,-0.25,1.25):
                    time=C.c_float(part*duration);seek(a,0,time)
                    if not core.fs15_animation_set_time(b,0,time):raise AssertionError('Loaded-graph seek rejected')
                    update(a+0x38,0)
                    if not core.fs15_animation_update(b,0,C.byref(playback)) or callback_errors:raise AssertionError(f'Loaded pose update rejected: {callback_errors}')
                    bytes_equal(a,b,0xd0,(0x10,0x48,0xa8,0xb0,0xb8))
                    for source_id in ids:
                        na,nb=nodes[0][source_id],nodes[1][source_id]
                        x,y=bytearray(normalize_record(na,0x170,(0x10,0x80,0x118,0x120))),bytearray(normalize_record(nb,0x170,(0x10,0x80,0x118,0x120)))
                        for offset,size in ((0x98,16),(0xd8,16),(0x148,9)):
                            compare('loaded_playback_float_regions',struct.unpack_from('<'+'f'*size,x,offset),struct.unpack_from('<'+'f'*size,y,offset))
                            x[offset:offset+size*4]=y[offset:offset+size*4]=bytes(size*4)
                        if x!=y:raise AssertionError('Loaded playback non-float node bytes differ')
                    counts['loaded_resource_pose_updates']+=1
            # Registry membership after rebinding covers later-group deletion;
            # node cleanup must release every final set/data/clip/key allocation.
            for source_id in ids:
                node_dtor(nodes[0][source_id])
                if not core.fs15_scene_destruct(nodes[1][source_id],0x170,3,0,C.byref(owner.api)):raise AssertionError('Loaded node destruction failed')
                release(nodes[0][source_id]);owner.release(None,nodes[1][source_id])
            for registry,side in ((native.registry(),0),(owner.registry,1)):
                for p in list(registry.values()):
                    if read(p)!=0x1404c9678 or read(p,0x30,'I'):raise AssertionError('Unexpected remaining loaded object')
                    if side==0:C.WINFUNCTYPE(P,P,C.c_uint32)(native.base+0x1750c0)(p,1)
                    elif not core.fs15_character_set_destruct(p,0xd0,1,C.byref(context),C.byref(owner.api)):raise AssertionError('Zero-node set cleanup failed')
            for offset in (0,0x20):
                base=native.base+0x162c2c0+offset;pointer=read(base)
                if pointer:release(pointer)
                C.memset(base,0,24);pointer=read(C.addressof(context)+offset)
                if pointer:owner.release(None,pointer)
            if native.registry() or owner.registry or len(native.allocations)!=1 or owner.blocks:raise AssertionError('Resource lifetime leaked owned storage')
            counts['complete_binary_load_lifetimes']+=1
            if path is not None:assets.append(dict(name=path.name,sha256=hashlib.sha256(payload).hexdigest(),bytes=len(payload),groups=len(groups),clips=sum(len(g['clips']) for g in groups),channels=sum(len(c['channels']) for g in groups for c in g['clips']),keys=sum(len(ch['keys']) for g in groups for c in g['clips'] for ch in c['channels'])))
    paths=('src/core/animation_resources.cpp','src/core/animation_resources.h','src/core/animation_storage.h','src/core/animation.cpp','src/core/animation.h','src/core/shared_scene.cpp','src/core/shared_scene.h',
        'src/core/scene_state.cpp','src/core/scene_state.h','src/core/scene_graph.cpp','src/core/scene_graph.h',
        'src/core/scene_lifecycle.cpp','src/core/scene_lifecycle.h','src/core/scene_math.cpp','src/core/scene_math.h','src/core/scene_trs.cpp','src/core/scene_trs.h')
    report=dict(status='passed',reference_sha256=EXPECTED_IMAGE,native_entries=['0x'+format(0x140000000+e,'x') for e in ENTRIES],
        rebuilt_source_sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in paths},tested_dll_sha256=hashlib.sha256(args.dll.read_bytes()).hexdigest(),
        comparisons=dict(counts),max_scaled_float_errors=errors,float_tolerance=8e-6,original_results_sha256=digest.hexdigest(),local_assets=assets,
        scope='Known resolved-node binary animation payload loading, packed key construction, ordered insertion/lookup generation and nested ownership, compared with unchanged original code. Authored bounded file/version wrapper, allocator/registry/node resolver transport and failure guards. No filesystem/archive loader, XML callback parsing, scheduling, skinning, renderer or complete engine/console startup.')
    write_json(ROOT/'analysis/animation-resources-validation.json',report);print(json.dumps(report,indent=2))


if __name__=='__main__':main()
