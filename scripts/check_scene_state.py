"""Compare recovered scene-state paths with pinned native code on synthetic trees.

Known original vtables are used only in the isolated reference process. Synthetic
observer callbacks record notifications without modifying nodes. No allocator,
game entry point, renderer, real physics actor or live game process is executed.
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
from native_gar import NativeGarCipher,EXPECTED_IMAGE

STRIDE=0x170
NONE=C.c_size_t(-1).value
P=C.c_void_p
N=C.c_size_t


class Properties(C.Structure):
    _fields_=[('unknown',C.c_uint64),('flags',C.c_uint32),('reserved',C.c_uint32)]


class Arena(C.Structure):
    _fields_=[('nodes',P),('node_bytes',N),('node_stride',N),('children',P),('child_count',N),
              ('observers',P),('observer_count',N),('properties',P),('property_count',N),
              ('kinds',P),('kind_count',N),('special_root',N)]


class Notification(C.Structure):
    _fields_=[('node',N),('observer',C.c_uint64),('origin',C.c_int32),('reserved',C.c_uint32)]


def canonical(data,count):
    result=bytearray(data.raw)
    for i in range(count):
        for offset in (0x28,0x30,0x38,0x40,0x58,0x60,0x68,0x118):
            struct.pack_into('<Q',result,i*STRIDE+offset,0)
    return bytes(result)


def fixture(rng,index,operation,reference,callback):
    count=1+index%31
    parents=[None]+[rng.randrange(i) if index%2 else (i-1)//2 for i in range(1,count)]
    children=[[j for j,p in enumerate(parents) if p==i] for i in range(count)]
    kinds=(C.c_uint8*count)(*[rng.randrange(2) for _ in range(count)])
    selected=index%count
    if operation>=3:kinds[selected]=operation-3
    props=(Properties*count)()
    observers_per_node=[rng.randrange(3) for _ in range(count)]
    observer_total=sum(observers_per_node)
    observer_vtable=(C.c_uint64*2)(0,C.cast(callback,P).value)
    observer_records=(C.c_uint64*(max(1,observer_total)*2))()
    observer_links=(C.c_uint64*max(1,observer_total))()
    observer_ids={}
    for i in range(observer_total):
        observer_records[i*2]=C.addressof(observer_vtable)
        observer_records[i*2+1]=i
        observer_links[i]=C.addressof(observer_records)+i*16
        observer_ids[observer_links[i]]=i
    payload=bytearray(rng.randbytes(count*STRIDE))
    obs=0
    for i in range(count):
        at=i*STRIDE
        props[i].flags=rng.getrandbits(32)
        if index%5==0:props[i].flags|=0x98
        struct.pack_into('<Q',payload,at,reference.base+(0x4c5d98 if kinds[i] else 0x4c0cd8))
        flags=rng.getrandbits(32)
        # Exercise all visibility/dirty/observer combinations, including root.
        struct.pack_into('<I',payload,at+0x50,flags)
        type_flags=rng.getrandbits(32)|0x20
        if index%9==0 and i%3==1:type_flags&=~0x20
        struct.pack_into('<I',payload,at+0x1c,type_flags)
        for location in (0x98,0xd8):
            matrix=[1,0,0,0,0,1,0,0,0,0,1,0,*[rng.uniform(-30,30) for _ in range(3)],1]
            matrix[rng.choice((0,5,10))]=rng.choice((-1,1))
            struct.pack_into('<16f',payload,at+location,*matrix)
        struct.pack_into('<Q',payload,at+0x118,C.addressof(props)+i*16)
        struct.pack_into('<4f',payload,at+0x128,rng.uniform(0,10),rng.uniform(20,100),0,0)
        struct.pack_into('<2I',payload,at+0x130,rng.getrandbits(32),rng.getrandbits(32))
        struct.pack_into('<2f',payload,at+0x138,rng.uniform(0,10),rng.uniform(20,100))
        struct.pack_into('<2I',payload,at+0x140,rng.getrandbits(32),rng.getrandbits(32))
        if index%7==0:
            # Equal cached/local clip/mask fields exercise propagation pruning.
            payload[at+0x138:at+0x148]=payload[at+0x128:at+0x138]
        if index%13==0:
            struct.pack_into('<f',payload,at+0x128,-0.0 if i%2 else 0.0)
            struct.pack_into('<f',payload,at+0x138,0.0)
        begin=C.addressof(observer_links)+obs*8
        end=begin+observers_per_node[i]*8
        struct.pack_into('<3Q',payload,at+0x58,begin,end,end)
        obs+=observers_per_node[i]
    copies=[C.create_string_buffer(bytes(payload),len(payload)) for _ in range(2)]
    link_copies=[(C.c_uint64*max(1,count-1))() for _ in range(2)]
    for copy,links in zip(copies,link_copies):
        cursor=0
        for i in range(count):
            struct.pack_into('<Q',copy,i*STRIDE+0x28,C.addressof(copy)+parents[i]*STRIDE if parents[i] is not None else 0)
            start=C.addressof(links)+cursor*8
            for child in children[i]:links[cursor]=C.addressof(copy)+child*STRIDE;cursor+=1
            end=C.addressof(links)+cursor*8
            struct.pack_into('<3Q',copy,i*STRIDE+0x30,start,end,end)
    special=0 if index%2 else NONE
    view=Arena(C.addressof(copies[1]),len(payload),STRIDE,C.addressof(link_copies[1]),count-1,
        C.addressof(observer_links),observer_total,C.addressof(props),count,C.addressof(kinds),count,special)
    # Keep every referenced caller-owned allocation alive through both calls.
    retained=(props,kinds,observer_vtable,observer_records,observer_links,link_copies)
    return copies,view,selected,observer_ids,retained


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dll',type=Path,required=True)
    args=parser.parse_args()
    core=C.CDLL(str(args.dll.resolve()))
    update=core.fs15_scene_state_update
    update.argtypes=[C.POINTER(Arena),N,C.c_uint,C.c_uint32,C.c_int32,P,N,C.POINTER(N)]
    update.restype=C.c_int
    setter=core.fs15_scene_set_local_matrix
    setter.argtypes=[C.POINTER(Arena),N,P,N];setter.restype=C.c_int
    rng=random.Random(0xf51557a7e)
    counts=Counter();digest=hashlib.sha256();trace=[];active_base=0

    @C.WINFUNCTYPE(None,P,P,C.c_int32)
    def observe(observer,node,origin):
        trace.append(((node-active_base)//STRIDE,observer,int(origin)))

    with NativeGarCipher() as native:
        propagate=C.WINFUNCTYPE(None,P,C.c_uint32,C.c_int32)(native.base+0xb1b00)
        rescan=C.WINFUNCTYPE(None,P,C.c_uint32)(native.base+0xb6160)
        inherited=C.WINFUNCTYPE(None,P,C.c_uint32)(native.base+0xb61f0)
        setters=[C.WINFUNCTYPE(None,P,P)(native.base+entry) for entry in (0xb15d0,0x10fce0)]
        singleton=C.c_void_p.from_address(native.base+0x162cd18);saved=singleton.value
        try:
            for index in range(2400):
                for operation in range(5):
                    copies,view,selected,observer_ids,retained=fixture(rng,index,operation,native,observe)
                    original,rebuilt=copies;count=len(original)//STRIDE
                    before=original.raw
                    active_base=C.addressof(original)
                    singleton.value=active_base+view.special_root*STRIDE if view.special_root!=NONE else 0
                    node=active_base+selected*STRIDE
                    trace.clear()
                    mask=(0,1,2,4,8,16,32,0x3f,0xffffffff)[index%9]
                    origin=(0,1,-1,2)[index%4]
                    if operation<3:
                        if operation==0:propagate(node,mask,origin)
                        elif operation==1:rescan(node,mask)
                        else:inherited(node,mask)
                        events=(Notification*(len(observer_ids)+1))();used=N(0)
                        if not update(C.byref(view),selected,operation,mask,origin,events,len(events),C.byref(used)):
                            raise AssertionError(f'Scene state rejected valid fixture {index}/{operation}')
                        actual=[(e.node,e.observer,e.origin) for e in events[:used.value]]
                        if trace!=actual:raise AssertionError(f'Notification order differs at {index}/{operation}')
                        counts['notification_trace_comparisons']+=1
                        counts['notifications']+=len(trace)
                        digest.update(json.dumps([(n,observer_ids[o],v) for n,o,v in trace]).encode())
                    else:
                        matrix=(C.c_float*16)(*[rng.uniform(-5,5) for _ in range(16)])
                        setters[operation-3](node,matrix)
                        if not setter(C.byref(view),selected,matrix,16):raise AssertionError('Matrix setter rejected')
                    a,b=canonical(original,count),canonical(rebuilt,count)
                    if operation==0:
                        counts['propagation_local_matrix_changes']+=sum(
                            before[i*STRIDE+0x98:i*STRIDE+0xd8]!=original.raw[i*STRIDE+0x98:i*STRIDE+0xd8]
                            for i in range(count))
                    if a!=b:
                        differences=[(i//STRIDE,hex(i%STRIDE)) for i,(x,y) in enumerate(zip(a,b)) if x!=y]
                        raise AssertionError(f'Scene records differ at {index}/{operation}: {differences[:16]}')
                    counts[('propagation','observer_rescan','inherited_flag','matrix_set','trs_matrix_set')[operation]]+=1
                    digest.update(a)
        finally:singleton.value=saved
    paths=('src/core/scene_state.cpp','src/core/scene_state.h','src/core/scene_graph.cpp',
           'src/core/scene_graph.h','src/core/scene_math.cpp','src/core/scene_math.h')
    report=dict(status='passed',reference_sha256=EXPECTED_IMAGE,
        native_entries=['0x1400b1b00','0x1400b6160','0x1400b61f0','0x1400b15d0','0x14010fce0'],
        rebuilt_source_sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in paths},
        rebuilt_dll_sha256=hashlib.sha256(args.dll.read_bytes()).hexdigest(),comparisons=dict(counts),
        original_results_sha256=digest.hexdigest(),
        scope='Finite synthetic bounded trees with base/TRS virtual dispatch, dirty/visibility/clip/mask propagation, '
              'world-preserving child matrix branch and side-effect-free observer notification order. '
              'No allocator, object destruction, arbitrary derived classes, callback side effects, real physics or engine startup.')
    write_json(ROOT/'analysis/scene-state-validation.json',report)
    print(json.dumps(report,indent=2))


if __name__=='__main__':main()
