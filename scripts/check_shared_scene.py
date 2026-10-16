"""Compare recovered CharacterSet ownership/binding with isolated GE6 code."""
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
from native_scene_lifecycle import NativeSceneLifecycle
from check_scene_lifecycle import PortableServices,Services,DESTROY,P,N,read,put,canonical


class Context(C.Structure):
    _fields_=[('active',C.c_uint64*3),('unknown18',C.c_uint64),('pending',C.c_uint64*3),('unknown38',C.c_uint64)]


class NativeShared(NativeSceneLifecycle):
    def __init__(self):
        super().__init__(math_helpers=True)
        self.animation_critical=self.base+0x162c300
        C.memset(self.base+0x162c2c0,0,0x40);self.kernel.InitializeCriticalSection(self.animation_critical)
        C.c_uint32.from_address(self.base+0x1630dc8).value|=1;C.c_uint32.from_address(self.base+0x1630d90).value=0
    def close(self):
        if getattr(self,'animation_critical',None):self.kernel.DeleteCriticalSection(self.animation_critical);self.animation_critical=None
        super().close()


def normalize_record(pointer,size,offsets):
    payload=bytearray(C.string_at(pointer,size))
    for at in offsets:struct.pack_into('<Q',payload,at,0 if read(pointer,at)==0 else 1)
    return bytes(payload)


def vector_state(pointer,step,pointer_ids=None):
    begin,end,capacity=(read(pointer,i) for i in (0,8,16))
    raw=C.string_at(begin,capacity-begin) if begin else b''
    if pointer_ids is not None:
        raw=tuple(pointer_ids.get(read(begin,i),('unused',read(begin,i))) for i in range(0,capacity-begin,step))
    return ((end-begin)//step,(capacity-begin)//step,raw)


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--dll',type=Path,required=True);args=parser.parse_args()
    core=C.CDLL(str(args.dll.resolve()))
    core.fs15_scene_construct.argtypes=[P,N,C.c_uint,C.c_char_p,N,C.c_uint32,C.c_uint8,C.POINTER(Services)];core.fs15_scene_construct.restype=C.c_int
    core.fs15_scene_destruct.argtypes=[P,N,C.c_uint,C.c_uint32,C.POINTER(Services)];core.fs15_scene_destruct.restype=C.c_int
    core.fs15_character_data_construct.argtypes=[P,N];core.fs15_character_data_construct.restype=C.c_int
    core.fs15_character_set_construct.argtypes=[P,N,C.c_char_p,N,P,C.POINTER(Context),C.POINTER(Services)];core.fs15_character_set_construct.restype=C.c_int
    core.fs15_character_set_bind.argtypes=[P,P,N,C.POINTER(Context),C.POINTER(Services),C.POINTER(C.c_int)];core.fs15_character_set_bind.restype=C.c_int
    core.fs15_shared_scene_release.argtypes=[P,N,C.POINTER(Context),C.POINTER(Services)];core.fs15_shared_scene_release.restype=C.c_int
    core.fs15_character_set_destruct.argtypes=[P,N,C.c_uint32,C.POINTER(Context),C.POINTER(Services)];core.fs15_character_set_destruct.restype=C.c_int
    core.fs15_scene_property_clone.argtypes=[P,N,C.POINTER(Services),C.POINTER(P)];core.fs15_scene_property_clone.restype=C.c_int
    core.fs15_scene_property_replace.argtypes=[P,N,P,C.POINTER(Services)];core.fs15_scene_property_replace.restype=C.c_int
    rng=random.Random(0xf5155a4e);counts=Counter();digest=hashlib.sha256();max_error=0.0
    with NativeShared() as native:
        data_ctor=C.WINFUNCTYPE(P,P)(native.base+0x174c10)
        set_ctor=C.WINFUNCTYPE(P,P,C.c_char_p,P)(native.base+0x174e40)
        node_ctor=C.WINFUNCTYPE(P,P,C.c_char_p,C.c_uint32)(native.base+0x10fa90)
        binder=C.WINFUNCTYPE(None,P,P,C.c_uint32)(native.base+0x1752f0)
        node_dtor=C.WINFUNCTYPE(None,P)(native.base+0x10fbb0)
        set_dtor=C.WINFUNCTYPE(None,P)(native.base+0x1750f0)
        property_clone=C.WINFUNCTYPE(P,P,P)(native.base+0xb1520)
        property_replace=C.WINFUNCTYPE(None,P,P)(native.base+0xb1e10)
        for case in range(220):
            portable=PortableServices(core);context=Context();portable.sequence=case*100
            C.c_uint32.from_address(native.base+0x1630d60).value=portable.sequence
            @DESTROY
            def release(_,pointer):return core.fs15_shared_scene_release(pointer,0x10 if read(pointer)==0x1404c9688 else 0xc0,C.byref(context),C.byref(portable.api))
            portable.api.release_shared=release
            def allocate(size):
                a=native.callbacks[0](native.heap,0,size);b=portable.allocate(None,size)
                payload=rng.randbytes(size);C.memmove(a,payload,size);C.memmove(b,payload,size);return a,b
            # Constructor's null-data branch and nondeleting destruction.
            auto_set=allocate(0xd0);name=b'new-data'
            set_ctor(auto_set[0],name,None)
            if not core.fs15_character_set_construct(auto_set[1],0xd0,name,len(name)+1,None,C.byref(context),C.byref(portable.api)):raise AssertionError('Auto-data constructor failed')
            if normalize_record(auto_set[0],0xd0,(0x10,0x48))!=normalize_record(auto_set[1],0xd0,(0x10,0x48)):raise AssertionError('Auto-data record differs')
            if C.string_at(read(auto_set[0],0x48),0xc0)!=C.string_at(read(auto_set[1],0x48),0xc0):raise AssertionError('Auto-created data differs')
            set_dtor(auto_set[0])
            if not core.fs15_character_set_destruct(auto_set[1],0xd0,0,C.byref(context),C.byref(portable.api)):raise AssertionError('Auto-data cleanup failed')
            if normalize_record(auto_set[0],0xd0,(0x10,0x48))!=normalize_record(auto_set[1],0xd0,(0x10,0x48)):raise AssertionError('Nondeleting set record differs')
            native.callbacks[1](native.heap,0,auto_set[0]);portable.release(None,auto_set[1]);counts['auto_data_constructor_destructor_pairs']+=1
            data=allocate(0xc0);data_ctor(data[0])
            if not core.fs15_character_data_construct(data[1],0xc0):raise AssertionError('Data constructor failed')
            if C.string_at(data[0],0xc0)!=C.string_at(data[1],0xc0):raise AssertionError('Data constructor bytes differ')
            counts['data_constructor_records']+=1
            sets=[];node_pairs=[];objects=[{},{}]
            for j in range(2):
                pair=allocate(0xd0);name=b'shared-animation'
                set_ctor(pair[0],name,data[0])
                if not core.fs15_character_set_construct(pair[1],0xd0,name,len(name)+1,data[1],C.byref(context),C.byref(portable.api)):raise AssertionError('Set constructor failed')
                for side in range(2):objects[side][pair[side]+0x38]=('observer',j)
                if normalize_record(pair[0],0xd0,(0x10,0x48))!=normalize_record(pair[1],0xd0,(0x10,0x48)):raise AssertionError('CharacterSet constructor bytes differ')
                sets.append(pair);counts['set_constructor_records']+=1
            node_count=1+case%40
            for i in range(node_count):
                pair=allocate(0x170);node_ctor(pair[0],b'bone',0)
                if not core.fs15_scene_construct(pair[1],0x170,3,b'bone',5,0,1,C.byref(portable.api)):raise AssertionError('Node constructor failed')
                # Cached native TRS inputs preserve realistic nontrivial poses;
                # every seventh case exercises lazy matrix decomposition.
                cache=[rng.uniform(-20,20) for _ in range(3)]+[rng.uniform(-2,2) for _ in range(3)]+[rng.uniform(0.2,3) for _ in range(3)]
                for side in range(2):
                    C.memmove(pair[side]+0x148,struct.pack('<9f',*cache),36)
                    if case%7==0:put(pair[side],0x50,read(pair[side],0x50,'I')|0x80,'I')
                    if i%3==0:put(pair[side],0x50,read(pair[side],0x50,'I')&~1,'I')
                    objects[side][pair[side]]=i
                node_pairs.append(pair)
            # Exercise the actual default property clone/replacement path.
            old_props=[read(node_pairs[0][side],0x118) for side in range(2)]
            flags=rng.getrandbits(32)
            for pointer in old_props:put(pointer,8,flags,'I')
            clone=property_clone(old_props[0],node_pairs[0][0]);copy=P()
            if not core.fs15_scene_property_clone(old_props[1],16,C.byref(portable.api),C.byref(copy)):raise AssertionError('Property clone failed')
            if C.string_at(clone,16)!=C.string_at(copy.value,16):raise AssertionError('Property clone bytes differ')
            property_replace(node_pairs[0][0],clone)
            if not core.fs15_scene_property_replace(node_pairs[0][1],0x170,copy,C.byref(portable.api)):raise AssertionError('Property replacement failed')
            # Avoid entering the still unrecovered active-trigger destructor.
            for pointer in (clone,copy.value):put(pointer,8,0,'I')
            counts['property_clone_replacement_pairs']+=1
            # Populate clip/channel/keyframe ownership, including long names.
            short_clip=case%17==2 and node_count>1;bad_node=case%13==1 and node_count>1
            channels=node_count-1 if short_clip else node_count
            clips=allocate(0x48);clip_name=(b'walk' if case%2 else b'long-animation-clip-name')
            channel_vectors=allocate(channels*8)
            for side in range(2):
                string=clips[side]+8;put(string,0x10,len(clip_name));capacity=15 if len(clip_name)<=15 else len(clip_name)|15;put(string,0x18,capacity)
                if capacity>15:
                    buffer=(native.callbacks[0](native.heap,0,capacity+1) if side==0 else portable.allocate(None,capacity+1));put(string,0,buffer)
                else:buffer=string
                C.memmove(buffer,clip_name+b'\0',len(clip_name)+1)
                for at,val in ((0x30,channel_vectors[side]),(0x38,channel_vectors[side]+channels*8),(0x40,channel_vectors[side]+channels*8)):put(clips[side],at,val)
            for k in range(channels):
                group=allocate(0x20);number=k%4;key_vectors=allocate(number*8) if number else (0,0)
                for side in range(2):
                    put(channel_vectors[side],k*8,group[side]);put(group[side],8,key_vectors[side]);put(group[side],16,key_vectors[side]+number*8);put(group[side],24,key_vectors[side]+number*8)
                for index in range(number):
                    block=allocate(24+index*4)
                    for side in range(2):put(key_vectors[side],index*8,block[side])
                counts['owned_clip_channel_records']+=1
            clip_lists=allocate(8)
            for side in range(2):
                put(clip_lists[side],0,clips[side]);put(data[side],0xa0,clip_lists[side]);put(data[side],0xa8,clip_lists[side]+8);put(data[side],0xb0,clip_lists[side]+8)
                if bad_node:put(node_pairs[-1][side],0x1c,read(node_pairs[-1][side],0x1c,'I')&~0x40,'I')
            first_lists=[(C.c_uint64*node_count)(*[p[side] for p in node_pairs]) for side in range(2)]
            # First group creates default poses, second reuses the same data
            # and rebinds the same nodes, releasing the previous reference.
            for j,pair in enumerate(sets):
                if j==1:
                    # Keep one external reference so rebind does not destroy
                    # the old set before remaining nodes have been detached.
                    for side in range(2):put(sets[0][side],0x30,read(sets[0][side],0x30,'I')+1,'I')
                binder(pair[0],first_lists[0],node_count);diagnostic=C.c_int(-1)
                expected_diagnostic=1 if bad_node else 2 if short_clip else 0
                if not core.fs15_character_set_bind(pair[1],first_lists[1],node_count,C.byref(context),C.byref(portable.api),C.byref(diagnostic)) or diagnostic.value!=expected_diagnostic:raise AssertionError('Binding status differs')
                counts['binding_diagnostic_'+str(diagnostic.value)]+=1
                if normalize_record(pair[0],0xd0,(0x10,0x48,0xa8,0xb0,0xb8))!=normalize_record(pair[1],0xd0,(0x10,0x48,0xa8,0xb0,0xb8)):raise AssertionError('Bound set differs')
                for offset,step in ((0x18,12),(0x38,16),(0x58,12),(0x78,4)):
                    a,b=vector_state(data[0]+offset,step),vector_state(data[1]+offset,step)
                    if a[:2]!=b[:2]:raise AssertionError('Pose vector size/capacity differs')
                    used=a[0]*step
                    if offset==0x78:
                        if a!=b:raise AssertionError('Visibility bit vector differs')
                    else:
                        if a[2][used:]!=b[2][used:]:raise AssertionError('Unused pose capacity differs')
                        for av,bv in zip(struct.unpack('<'+'f'*(used//4),a[2][:used]),struct.unpack('<'+'f'*(used//4),b[2][:used])):
                            error=abs(av-bv)/max(1.0,abs(av),abs(bv));max_error=max(max_error,error)
                            if error>8e-6:raise AssertionError(f'Pose float differs at {case}/{hex(offset)}: {av} vs {bv}')
                        if a[2][:used]!=b[2][:used]:counts['non_bit_exact_pose_buffers']+=1
                    digest.update(a[2]);counts['complete_default_pose_buffers']+=1
                if vector_state(pair[0]+0xa8,8,objects[0])!=vector_state(pair[1]+0xa8,8,objects[1]):raise AssertionError('Bound node vector differs')
                counts['set_binding_records']+=1
            if vector_state(native.base+0x162c2c0,8,objects[0])!=vector_state(C.addressof(context),8,objects[1]):raise AssertionError('Observer registration vector differs')
            counts['update_registry_snapshots']+=1
            # Both active and pending update-list removal paths.
            pending=allocate(16)
            for side in range(2):
                for j,pair in enumerate(sets):put(pair[side],0x40,1,'B');put(pending[side],j*8,pair[side]+0x38)
                base=native.base+0x162c2e0 if side==0 else C.addressof(context)+0x20
                put(base,0,pending[side]);put(base,8,pending[side]+16);put(base,16,pending[side]+16)
            # Explicit external reference to the first set is released first.
            ref0=sets[0][0]+0x28
            C.WINFUNCTYPE(P,P,C.c_uint32)(read(read(ref0)))(ref0,1)
            if not release(None,sets[0][1]+0x28):raise AssertionError('External reference release failed')
            for pair in node_pairs:
                node_dtor(pair[0])
                if not core.fs15_scene_destruct(pair[1],0x170,3,0,C.byref(portable.api)):raise AssertionError('Shared node destruction failed')
                if canonical(C.create_string_buffer(C.string_at(pair[0],0x170),0x170).raw,3)!=canonical(C.create_string_buffer(C.string_at(pair[1],0x170),0x170).raw,3):
                    # shared_resource is a stale pointer in both native records.
                    a=bytearray(C.string_at(pair[0],0x170));b=bytearray(C.string_at(pair[1],0x170))
                    a[0x120:0x128]=b[0x120:0x128]=bytes(8)
                    if canonical(bytes(a),3)!=canonical(bytes(b),3):raise AssertionError('Retired node record differs')
                counts['shared_node_destructors']+=1
                native.callbacks[1](native.heap,0,pair[0]);portable.release(None,pair[1])
            if native.registry() or portable.registry:raise AssertionError('Shared objects remain registered')
            if read(native.base+0x162c2c8)!=read(native.base+0x162c2c0) or context.active[0]!=context.active[1]:raise AssertionError('Observers remain active')
            # Global vector capacity is owned by the isolated context, not a set.
            for offset in (0,0x20):
                pointer=read(native.base+0x162c2c0+offset)
                if pointer:native.callbacks[1](native.heap,0,pointer)
                C.memset(native.base+0x162c2c0+offset,0,24)
                pointer=read(C.addressof(context)+offset)
                if pointer:portable.release(None,pointer)
            if len(native.allocations)!=1 or portable.blocks:raise AssertionError('Shared ownership leaked member storage')
            counts['complete_shared_lifetimes']+=1
    paths=('src/core/shared_scene.cpp','src/core/shared_scene.h','src/core/scene_lifecycle.cpp','src/core/scene_lifecycle.h',
        'src/core/scene_attributes.cpp','src/core/scene_attributes.h','src/core/scene_state.h','src/core/scene_math.cpp','src/core/scene_math.h','src/core/scene_trs.cpp','src/core/scene_trs.h')
    report=dict(status='passed',reference_sha256=EXPECTED_IMAGE,
        native_entries=['0x140174c10','0x140174cc0','0x140174e10','0x140174e40','0x1401750c0','0x1401750f0','0x1401752f0',
            '0x140176800','0x1401bd100','0x140041980','0x1400b1520','0x1400b1e10'],
        rebuilt_source_sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in paths},tested_dll_sha256=hashlib.sha256(args.dll.read_bytes()).hexdigest(),
        cases=dict(counts),original_results_sha256=digest.hexdigest(),float_tolerance=8e-6,max_scaled_float_error=max_error,
        scope='Known CharacterData/CharacterSet native-layout construction, pose binding/rebinding, embedded reference release, observer-list membership and node lifetime integration. Original code uses an isolated private heap and explicit initialized update-registry state. Platform allocation, locking/dispatch/registry integration and guards are authored; animation evaluation, full engine startup, other resource classes and console execution remain open.')
    write_json(ROOT/'analysis/shared-scene-validation.json',report);print(json.dumps(report,indent=2))


if __name__=='__main__':main()
