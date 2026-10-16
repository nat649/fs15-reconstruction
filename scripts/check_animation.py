"""Compare recovered animation paths with unchanged, pinned GE6 machine code.

Original execution is isolated from the installed game and never calls startup.
Known native TRS vtables and side-effect-free observer fixtures are used. The
reconstruction's virtual transport delegates to the recovered scene arena APIs.
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
from native_gar import NativeGarCipher,EXPECTED_IMAGE
from check_scene_state import Arena,Notification,STRIDE,NONE,fixture,canonical

P=C.c_void_p
N=C.c_size_t
ENTRIES=[0x524b0,0x174af0,0x2806f0,0x1766d0,0x176950,0x1762a0,0x176330,0x176370,0x1763b0,0x175820]
TOLERANCE=8e-6
REFRESH=C.CFUNCTYPE(C.c_int,P,P)
SETTER=C.CFUNCTYPE(C.c_int,P,P,P)
PROPAGATE=C.CFUNCTYPE(C.c_int,P,P,C.c_uint32,C.c_int32)


class Services(C.Structure):
    _fields_=[('user',P),('refresh',REFRESH),('matrix',SETTER),('propagate',PROPAGATE)]


def floats(values):return (C.c_float*len(values))(*values)
def buffer(size,rng):return C.create_string_buffer(rng.randbytes(size),size)
def address(p):return C.addressof(p)
def put(p,offset,value,kind='Q'):struct.pack_into('<'+kind,p,offset,value)
def read(p,offset,kind='Q'):return struct.unpack_from('<'+kind,p,offset)[0]
def vector(record,offset,storage,used=None):
    begin=address(storage) if storage is not None else 0
    size=C.sizeof(storage) if storage is not None else 0
    struct.pack_into('<3Q',record,offset,begin,begin+(size if used is None else used),begin+size)


def clips_fixture(rng,count,channel_count):
    """Paired clip graphs share immutable key payloads, never mutable channels."""
    graphs=[[],[]];retained=[]
    for clip_index in range(count):
        duration=C.c_float(1+rng.random()*9).value
        pair=[buffer(0x48,rng) for _ in range(2)]
        C.memmove(pair[1],pair[0],0x48)
        lists=[(C.c_uint64*channel_count)() for _ in range(2)];channels=[]
        name=[b'idle',b'long-animation-clip-name',b'walk',b'idle'][clip_index%4]
        for p in pair:
            put(p,0,duration,'f');p[8:40]=bytes(32);put(p,0x18,len(name));put(p,0x20,max(15,len(name)|15))
            if len(name)>15:
                s=C.create_string_buffer(name);retained.append(s);put(p,8,address(s))
            else:p[8:8+len(name)+1]=name+b'\0'
        for channel_index in range(channel_count):
            number=1+rng.randrange(9)
            # Interior first/last keys leave a genuine looping seam.
            times=[C.c_float(duration*(0.1+0.75*j/max(1,number-1))).value for j in range(number)]
            if number>2 and channel_index%4==0:times[1]=times[0]
            keys=(C.c_uint64*number)()
            for k,time in enumerate(times):
                key=buffer(48,rng);put(key,0,time,'f')
                put(key,4,rng.getrandbits(16)&0x83f0,'H');put(key,6,12,'B');put(key,7,20,'B')
                struct.pack_into('<3f',key,8,*[rng.uniform(-30,30) for _ in range(3)])
                q=[rng.randrange(-32767,32768) for _ in range(4)]
                if k%5==0:q=[0,0,0,32767 if k%2 else -32767]
                struct.pack_into('<4h',key,20,*q);struct.pack_into('<3f',key,28,*[rng.uniform(0.2,4) for _ in range(3)])
                keys[k]=address(key);retained.append(key)
            c=[buffer(0xd0,rng) for _ in range(2)];C.memmove(c[1],c[0],0xd0)
            flags=(0,1,2,4,8,3,5,7,15)[(channel_index+clip_index)%9]
            for side in range(2):
                put(c[side],2,flags,'H');vector(c[side],8,keys);put(c[side],0xa8,64/duration,'f');put(c[side],0xac,times[-1],'f')
                # Start=1 is always valid; alternate entries advance safely to
                # the lowest interval's upper key for this bucket.
                for bucket in range(64):
                    lower=times[0]+bucket/(64/duration)
                    start=1
                    while start<number-1 and times[start]<lower:start+=1
                    put(c[side],0x28+bucket*2,start,'H')
                put(c[side],0xc0,keys[0]);put(c[side],0xc8,keys[-1]);lists[side][channel_index]=address(c[side])
            channels.append(c);retained.extend([keys,*c])
        for side in range(2):vector(pair[side],0x30,lists[side]);graphs[side].append((pair[side],channels))
        retained.extend([*pair,*lists])
    return graphs,retained


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--dll',type=Path,required=True);args=parser.parse_args()
    core=C.CDLL(str(args.dll.resolve()))
    signatures={'fs15_animation_rsqrt_table':[P,N],'fs15_animation_normalize':[P,N],
        'fs15_animation_nlerp':[P,P,C.c_float,P,N],'fs15_animation_rotation':[P,P,N],
        'fs15_animation_sample':[P,C.c_float,C.c_int],'fs15_animation_set_time':[P,C.c_uint32,C.c_float],
        'fs15_animation_set_weight':[P,C.c_uint32,C.c_float],'fs15_animation_set_clip':[P,C.c_uint32,C.c_uint32],
        'fs15_animation_find_clip':[P,C.c_char_p,N,C.POINTER(C.c_uint32)],'fs15_animation_update':[P,C.c_float,C.POINTER(Services)],
        'fs15_scene_state_update':[C.POINTER(Arena),N,C.c_uint,C.c_uint32,C.c_int32,P,N,C.POINTER(N)],
        'fs15_scene_set_local_matrix':[C.POINTER(Arena),N,P,N],
        'fs15_scene_trs_update':[C.POINTER(Arena),N,C.c_uint,P,N,P,N,C.POINTER(N),C.POINTER(C.c_int)]}
    for name,signature in signatures.items():getattr(core,name).argtypes=signature;getattr(core,name).restype=C.c_int
    rng=random.Random(0xf515a91);counts=Counter();errors={};digest=hashlib.sha256();trace=[];active_base=0

    def compare(kind,a,b):
        if len(a)!=len(b):raise AssertionError('Comparison size differs')
        max_error=errors.get(kind,0)
        for x,y in zip(a,b):
            if not math.isfinite(x) or not math.isfinite(y):raise AssertionError('Nonfinite result')
            error=abs(x-y)/max(1,abs(x));max_error=max(max_error,error)
            if error>TOLERANCE:raise AssertionError(f'{kind}: {x} vs {y}, scaled error={error}')
        errors[kind]=max_error;counts[kind]+=1
        if struct.pack('<'+'f'*len(a),*a)==struct.pack('<'+'f'*len(b),*b):counts[kind+'_bit_exact']+=1

    @C.WINFUNCTYPE(None,P,P,C.c_int32)
    def observe(observer,node,origin):trace.append(((node-active_base)//STRIDE,observer,int(origin)))

    with NativeGarCipher(math_helpers=True) as native:
        initialize=C.WINFUNCTYPE(P)(native.base+0x524b0);initialize()
        expected=C.string_at(native.base+0x162c380,2048);seeds=(C.c_uint32*512)()
        if not core.fs15_animation_rsqrt_table(seeds,512) or bytes(seeds)!=expected:raise AssertionError('Generated rsqrt table differs')
        counts['rsqrt_seed_entries']=512;digest.update(expected)
        normalize=C.WINFUNCTYPE(None,P)(native.base+0x174af0)
        nlerp=C.WINFUNCTYPE(None,P,P,P,C.c_float)(native.base+0x2806f0)
        rotation=C.WINFUNCTYPE(None,P,P)(native.base+0x1766d0)
        sample=C.WINFUNCTYPE(None,P,C.c_float,C.c_uint8)(native.base+0x176950)
        controls=[C.WINFUNCTYPE(None,P,C.c_uint32,C.c_float)(native.base+entry) for entry in (0x1762a0,0x176330)]
        controls.append(C.WINFUNCTYPE(None,P,C.c_uint32,C.c_uint32)(native.base+0x176370))
        find=C.WINFUNCTYPE(C.c_uint32,P,C.c_char_p)(native.base+0x1763b0)
        update=C.WINFUNCTYPE(None,P,C.c_float)(native.base+0x175820)
        for index in range(8000):
            values=[rng.uniform(-100,100) for _ in range(4)]
            if index<8:values=[[0,0,0,0],[1,0,0,0],[-1,0,-0.0,0],[1e-30,0,0,0],[1e-18,0,0,0],[1e15,-1e15,2e15,0],[0,1,0,0],[0,0,1,1]][index]
            a,b=floats(values),floats(values);normalize(a)
            if not core.fs15_animation_normalize(b,4):raise AssertionError('Normalization rejected')
            compare('normalized_quaternions',list(a),list(b));digest.update(bytes(a))
            qa=floats([rng.uniform(-1,1) for _ in range(4)]);qb=floats([rng.uniform(-1,1) for _ in range(4)])
            if index%4==0:qb=floats([-f for f in qa])
            t=C.c_float(rng.uniform(-0.5,1.5));a,b=floats([0]*4),floats([0]*4);nlerp(a,qa,qb,t)
            if not core.fs15_animation_nlerp(qa,qb,t,b,4):raise AssertionError('Nlerp rejected')
            compare('nlerp_quaternions',list(a),list(b));digest.update(bytes(a))

        def compare_channels(graphs):
            for (_,ach),(_,bch) in zip(*graphs):
                for pair in ach:
                    a,b=pair
                    if a.raw!=b.raw:raise AssertionError(f'Complete channel selection/weights differ at {[(hex(i),a.raw[i:i+4].hex(),b.raw[i:i+4].hex()) for i in range(0,0xd0,4) if a.raw[i:i+4]!=b.raw[i:i+4]]}, case={case}, time={time}, loop={loop}')
                    normalized=bytearray(a.raw);normalized[8:0x20]=bytes(24)
                    key_addresses=list((C.c_uint64*((read(a,16)-read(a,8))//8)).from_address(read(a,8)))
                    for offset in (0xc0,0xc8):put(normalized,offset,key_addresses.index(read(a,offset)))
                    digest.update(normalized);counts['complete_channel_records']+=1
                    qa,qb=floats([0]*4),floats([0]*4);rotation(a,qa)
                    if not core.fs15_animation_rotation(b,qb,4):raise AssertionError('Compressed quaternion rejected')
                    compare('compressed_channel_quaternions',list(qa),list(qb));digest.update(bytes(qa))

        for case in range(240):
            graphs,retained=clips_fixture(rng,1,1+case%19);a,b=graphs[0][0][0],graphs[1][0][0];duration=read(a,0,'f')
            for time in (0,-0.0,-duration*2.25,duration*4.75,duration,duration*2,duration*0.1,duration*0.85,rng.uniform(0,duration)):
                for loop in (0,1):
                    sample(a,time,loop)
                    if not core.fs15_animation_sample(b,time,loop):raise AssertionError(f'Sample rejected {case}/{time}/{loop}')
                    compare_channels(graphs);counts['complete_clip_samples']+=1

        singleton=C.c_void_p.from_address(native.base+0x162cd18);saved=singleton.value
        logger=C.c_uint32.from_address(native.base+0x1630dc8);logger_count=C.c_uint32.from_address(native.base+0x1630d90)
        saved_logger=logger.value,logger_count.value;logger.value|=1;logger_count.value=0
        try:
            for case in range(280):
                copies,view,_,observer_ids,tree_storage=fixture(rng,case,0,native,observe);count=len(copies[0])//STRIDE
                # Keep the original scene graph, opaque bytes and observers;
                # select only the recovered TRS virtual slots for animation.
                for i in range(count):
                    tree_storage[1][i]=1
                    for p in copies:
                        put(p,i*STRIDE,native.base+0x4c5d98)
                        struct.pack_into('<9f',p,i*STRIDE+0x148,*struct.unpack_from('<3f',p,i*STRIDE+0xc8),0,0,0,1,1,1)
                graphs,retained=clips_fixture(rng,4,count)
                sets=[buffer(0xd0,rng) for _ in range(2)];C.memmove(sets[1],sets[0],0xd0)
                data=[buffer(0xc0,rng) for _ in range(2)];C.memmove(data[1],data[0],0xc0)
                clip_lists=[(C.c_uint64*4)(*[address(g[0]) for g in graph]) for graph in graphs]
                transforms=[(C.c_uint64*count)(*[address(p)+i*STRIDE for i in range(count)]) for p in copies]
                translations=floats([rng.uniform(-30,30) for _ in range(count*3)]);scales=floats([rng.uniform(0.4,3) for _ in range(count*3)])
                visibility=(C.c_uint32*((count+31)//32))(*[rng.getrandbits(32) for _ in range((count+31)//32)])
                set_flags=(0 if case%17==0 else 1)|(rng.choice((0,1,2,128))<<8)
                for side in range(2):
                    put(sets[side],0x48,address(data[side]));vector(sets[side],0xa8,transforms[side]);vector(data[side],0xa0,clip_lists[side])
                    vector(data[side],0x18,translations);vector(data[side],0x58,scales);vector(data[side],0x78,visibility);put(data[side],0x98,count)
                    put(sets[side],0x50,set_flags,'H')
                    for track in range(4):
                        # Native updates are paired, including repeated clip IDs.
                        flags=(3 if track%2 else 7) if case%3==0 else (0,1,3,7)[(case+track)%4]
                        duration=read(graphs[side][track][0],0,'f')
                        put(sets[side],0x54+track*4,flags,'I');put(sets[side],0x64+track*4,track,'I')
                        put(sets[side],0x74+track*4,(-2,-1,0,1,2)[(case+track)%5],'f')
                        put(sets[side],0x84+track*4,duration*((case%9)-2)/4,'f');put(sets[side],0x94+track*4,(-0.5,0,0.5,0.6,1,1.5)[(case+track)%6],'f')
                for name in (b'idle',b'walk',b'long-animation-clip-name',b'WALK',b'absent',b''):
                    found=C.c_uint32();expected=find(sets[0],name)
                    if not core.fs15_animation_find_clip(sets[1],name,len(name)+1,C.byref(found)) or expected!=found.value:raise AssertionError('Name lookup differs')
                    counts['clip_name_lookups']+=1
                for step in range(12):
                    track=step%4;operation=step%3
                    value=graphs[0][track][0] if operation==0 else None
                    operand=C.c_float(read(value,0,'f')*((step%5)-1)/2).value if operation==0 else C.c_float(rng.choice((0,0.5,0.51,1,2))).value
                    if operation==2:operand=(case+step)%4
                    controls[operation](sets[0],track,operand)
                    method=(core.fs15_animation_set_time,core.fs15_animation_set_weight,core.fs15_animation_set_clip)[operation]
                    if not method(sets[1],track,operand):raise AssertionError('State control rejected')
                    if sets[0].raw[0x50:0xa8]!=sets[1].raw[0x50:0xa8]:raise AssertionError('Track control state differs')
                    counts['track_control_records']+=1
                portable_trace=[];callback_errors=[]
                def run_scene(operation,node,matrix=None,mask=0,origin=0):
                    i=(node-address(copies[1]))//STRIDE;events=(Notification*(len(observer_ids)+1))();used=N();warning=C.c_int()
                    if operation==0:ok=core.fs15_scene_trs_update(C.byref(view),i,0,None,0,events,len(events),C.byref(used),C.byref(warning))
                    elif operation==1:ok=core.fs15_scene_set_local_matrix(C.byref(view),i,matrix,16)
                    else:ok=core.fs15_scene_state_update(C.byref(view),i,0,mask,origin,events,len(events),C.byref(used))
                    if not ok:callback_errors.append((operation,i));return 0
                    portable_trace.extend((e.node,e.observer,e.origin) for e in events[:used.value]);return 1
                @REFRESH
                def refresh(_,node):return run_scene(0,node)
                @SETTER
                def matrix(_,node,m):return run_scene(1,node,m)
                @PROPAGATE
                def propagate(_,node,mask,origin):return run_scene(2,node,mask=mask,origin=origin)
                services=Services(None,refresh,matrix,propagate)
                for step in range(8):
                    dt=(0,0.01,0.25,1,-0.25,10,0.1,0)[step];active_base=address(copies[0]);singleton.value=active_base+view.special_root*STRIDE if view.special_root!=NONE else 0
                    trace.clear();portable_trace.clear();update(address(sets[0])+0x38,dt)
                    if not core.fs15_animation_update(sets[1],dt,C.byref(services)):raise AssertionError(f'Update rejected {case}/{step}, callback={callback_errors}')
                    if sets[0].raw[0x50:0xa8]!=sets[1].raw[0x50:0xa8]:raise AssertionError(f'Update clocks/dirty state differ {case}/{step}')
                    if trace!=portable_trace:raise AssertionError(f'Ordered observer traces differ {case}/{step}')
                    counts['animation_update_notification_traces']+=1;counts['animation_notifications']+=len(trace)
                    for j in range(count):
                        a=bytearray(canonical(copies[0],count)[j*STRIDE:(j+1)*STRIDE]);b=bytearray(canonical(copies[1],count)[j*STRIDE:(j+1)*STRIDE])
                        for offset,number in ((0x98,16),(0xd8,16),(0x148,9)):
                            compare('updated_node_float_regions',struct.unpack_from('<'+'f'*number,a,offset),struct.unpack_from('<'+'f'*number,b,offset))
                            a[offset:offset+number*4]=b[offset:offset+number*4]=bytes(number*4)
                        if a!=b:raise AssertionError(f'Node non-float bytes differ {case}/{step}/{j}')
                    counts['complete_animation_update_arenas']+=1
                    digest.update(canonical(copies[0],count))
                    digest.update(sets[0].raw[0x50:0xa8]);digest.update(json.dumps([(n,observer_ids[o],v) for n,o,v in trace]).encode())
        finally:singleton.value=saved;logger.value,logger_count.value=saved_logger
    paths=('src/core/animation.cpp','src/core/animation.h','src/core/shared_scene.h','src/core/scene_state.cpp','src/core/scene_state.h',
        'src/core/scene_math.cpp','src/core/scene_math.h','src/core/scene_trs.cpp','src/core/scene_trs.h','src/core/scene_graph.cpp','src/core/scene_graph.h')
    report=dict(status='passed',reference_sha256=EXPECTED_IMAGE,native_entries=['0x'+format(0x140000000+e,'x') for e in ENTRIES],
        rebuilt_source_sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in paths},tested_dll_sha256=hashlib.sha256(args.dll.read_bytes()).hexdigest(),
        comparisons=dict(counts),max_scaled_float_errors=errors,float_tolerance=TOLERANCE,original_results_sha256=digest.hexdigest(),
        math_backend='Platform sqrt/fmod, recovered table seed/iteration and animation flow; original CRT math implementations remain unrecovered',
        scope='Finite native-layout animation keys/clips, four-track controls, compressed rotation and bounded known-TRS pose application. Full state/opaque bytes and observer order/origin checked against original code. Authored bounds, virtual transport and side-effect-free callbacks. No loader, scheduler, skinning, full engine startup or console execution.')
    write_json(ROOT/'analysis/animation-validation.json',report);print(json.dumps(report,indent=2))


if __name__=='__main__':main()
