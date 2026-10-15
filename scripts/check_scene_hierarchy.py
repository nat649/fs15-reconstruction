"""Compare bounded GE6 hierarchy updates with the pinned original native image.

All vectors are preallocated synthetic memory. Native allocation/growth, game
startup, real objects and observer callback side effects are not exercised.
"""
import argparse
from collections import Counter
import ctypes as C
import hashlib
import json
from pathlib import Path
import random
import struct
from inventory import ROOT, write_json
from native_gar import NativeGarCipher, EXPECTED_IMAGE
from check_scene_state import Arena, Notification, fixture, STRIDE, N, P, NONE


def canonical_nodes(data, count, links):
    result=bytearray(data.raw)
    for i in range(count):
        at=i*STRIDE
        parent=struct.unpack_from('<Q',result,at+0x28)[0]
        struct.pack_into('<Q',result,at+0x28,(parent-C.addressof(data))//STRIDE+1 if parent else 0)
        for offset in (0x30,0x38,0x40):
            pointer=struct.unpack_from('<Q',result,at+offset)[0]
            struct.pack_into('<Q',result,at+offset,(pointer-C.addressof(links))//8)
        struct.pack_into('<Q',result,at+0x118,i)
    return bytes(result)


def canonical_links(data, links):
    base=C.addressof(data)
    return [((value-base)//STRIDE+1) if base<=value<base+len(data) and (value-base)%STRIDE==0 else value for value in links]


def parents_of(data):
    return [((p-C.addressof(data))//STRIDE if p else None)
            for p in (struct.unpack_from('<Q',data,i*STRIDE+0x28)[0] for i in range(len(data)//STRIDE))]


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dll',type=Path,required=True)
    args=parser.parse_args()
    core=C.CDLL(str(args.dll.resolve()))
    link=core.fs15_scene_link
    link.argtypes=[C.POINTER(Arena),P,N,N,C.c_uint32,C.c_int,P,N,C.POINTER(N),C.POINTER(C.c_uint32)]
    link.restype=C.c_int
    unlink=core.fs15_scene_unlink
    unlink.argtypes=[C.POINTER(Arena),P,N,C.c_uint32,C.c_int,P,N,C.POINTER(N)]
    unlink.restype=C.c_int
    counts=Counter();digest=hashlib.sha256();trace=[];active_base=0
    rng=random.Random(0xf5151a1c)

    @C.WINFUNCTYPE(None,P,P,C.c_int32)
    def observe(observer,node,origin):trace.append(((node-active_base)//STRIDE,observer,int(origin)))

    with NativeGarCipher() as native:
        native_links=[C.WINFUNCTYPE(C.c_uint32,P,P,C.c_uint32)(native.base+entry) for entry in (0x1363d0,0xb17b0)]
        native_unlink=C.WINFUNCTYPE(None,P,C.c_uint32,C.c_uint8)(native.base+0xb19c0)
        singleton=C.c_void_p.from_address(native.base+0x162cd18);saved=singleton.value
        # Read the native threshold instead of assuming the TRS gimbal epsilon.
        epsilon=C.c_float.from_address(native.base+0x4db3c0).value
        try:
            for case in range(1600):
                fixture_index=case+1 if (case+1)%31 else case+2
                copies,view,_,observer_ids,retained=fixture(rng,fixture_index,0,native,observe)
                count=len(copies[0])//STRIDE
                parents=parents_of(copies[0]);capacity=count+2
                links=[(C.c_uint64*(count*capacity))() for _ in range(2)]
                for data,slots in zip(copies,links):
                    for i in range(count):
                        kids=[j for j,p in enumerate(parents) if p==i]
                        begin=C.addressof(slots)+i*capacity*8
                        struct.pack_into('<3Q',data,i*STRIDE+0x30,begin,begin+len(kids)*8,begin+capacity*8)
                        for j in range(capacity):slots[i*capacity+j]=C.addressof(data)+kids[j]*STRIDE if j<len(kids) else 0xfeed0000+j
                        # Exercise the identity-root optimization and strict
                        # comparison boundary on every inspected matrix lane.
                        if i==0 and case%3==0:
                            matrix=[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1]
                            lane=(0,1,2,4,5,6,8,9,10,12,13,14)[case%12]
                            matrix[lane]+=epsilon*(0,0.5,1,1.5,-1)[case%5]
                            struct.pack_into('<16f',data,0x98,*matrix)
                            struct.pack_into('<I',data,0x50,struct.unpack_from('<I',data,0x50)[0]|0x40)
                view.children=C.addressof(links[1]);view.child_count=len(links[1])
                original,rebuilt=copies;active_base=C.addressof(original)
                singleton.value=active_base+view.special_root*STRIDE if view.special_root!=NONE else 0
                # Sequential operations exercise attach -> detach -> reattach,
                # inherited state changes and vector erase/rotation residues.
                for step in range(12):
                    parents=parents_of(original)
                    possible=[]
                    for parent in range(count):
                        for child in range(count):
                            if child==parent:continue
                            cursor=parent
                            while cursor is not None and cursor!=child:cursor=parents[cursor]
                            if cursor is None:possible.append((parent,child))
                    operation=(case+step)%4
                    nonempty=[i for i in range(count) if struct.unpack_from('<Q',original,i*STRIDE+0x38)[0]>struct.unpack_from('<Q',original,i*STRIDE+0x30)[0]]
                    if operation>=2 and not nonempty:operation=1
                    events=(Notification*(len(observer_ids)*3+1))();used=N();attached=C.c_uint32(0xffffffff)
                    trace.clear()
                    if operation<2:
                        parent,child=rng.choice(possible)
                        position=(0,1,0xffffffff,rng.randrange(count+3))[step%4]
                        expected=native_links[operation](active_base+parent*STRIDE,active_base+child*STRIDE,position)
                        ok=link(C.byref(view),links[1],parent,child,position,operation,events,len(events),C.byref(used),C.byref(attached))
                        if attached.value!=expected:raise AssertionError(f'Inserted index differs at {case}/{step}')
                        counts[('raw_links','matrix_links')[operation]]+=1
                        counts['reparents' if parents[child] is not None else 'root_attachments']+=1
                        if parents[child]==parent:counts['same_parent_reorders']+=1
                    else:
                        parent=rng.choice(nonempty)
                        start,end=struct.unpack_from('<2Q',original,parent*STRIDE+0x30)
                        slot=rng.randrange((end-start)//8);suppress=operation-2
                        native_unlink(active_base+parent*STRIDE,slot,suppress)
                        ok=unlink(C.byref(view),links[1],parent,slot,suppress,events,len(events),C.byref(used))
                        counts['suppressed_unlinks' if suppress else 'unlinks']+=1
                    if not ok:raise AssertionError(f'Bounded hierarchy rejected {case}/{step}/{operation}')
                    actual=[(e.node,e.observer,e.origin) for e in events[:used.value]]
                    if trace!=actual:raise AssertionError(f'Notification trace differs at {case}/{step}: {trace[:8]} vs {actual[:8]}')
                    a=canonical_nodes(original,count,links[0])
                    b=canonical_nodes(rebuilt,count,links[1])
                    if a!=b:
                        differences=[(i//STRIDE,hex(i%STRIDE)) for i,(x,y) in enumerate(zip(a,b)) if x!=y]
                        raise AssertionError(f'Node state differs at {case}/{step}/{operation}: {differences[:16]}')
                    x,y=canonical_links(original,links[0]),canonical_links(rebuilt,links[1])
                    if x!=y:raise AssertionError(f'Complete child storage differs at {case}/{step}')
                    counts['complete_arenas']+=1;counts['notification_traces']+=1;counts['notifications']+=len(trace)
                    digest.update(a);digest.update(json.dumps(x).encode())
                    digest.update(json.dumps([(i,observer_ids[o],origin) for i,o,origin in trace]).encode())
        finally:singleton.value=saved
    paths=('src/core/scene_state.cpp','src/core/scene_state.h','src/core/scene_graph.cpp',
           'src/core/scene_graph.h','src/core/scene_math.cpp','src/core/scene_math.h',
           'src/core/scene_trs.cpp','src/core/scene_trs.h')
    report=dict(status='passed',reference_sha256=EXPECTED_IMAGE,
        native_entries=['0x1401363d0','0x1400b17b0','0x1400b19c0'],
        rebuilt_source_sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in paths},
        tested_dll_sha256=hashlib.sha256(args.dll.read_bytes()).hexdigest(),
        cases=dict(counts),original_results_sha256=digest.hexdigest(),native_identity_epsilon=epsilon,
        scope='Preallocated synthetic scene forests, three bounded hierarchy paths, exact node bytes, parent/child order, unused vector slots and side-effect-free notification order/origin. No native vector allocation/growth, object constructor/destructor, registry, arbitrary derived callbacks, full engine startup or console execution.')
    write_json(ROOT/'analysis/scene-hierarchy-validation.json',report)
    print(json.dumps(report,indent=2))


if __name__=='__main__':main()
