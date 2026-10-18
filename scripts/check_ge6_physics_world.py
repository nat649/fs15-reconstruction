"""Compare actual GE6 factory/solver trajectories with the recompiled backend.

Original game code stays local. This tests bounded ordinary rigid-body scenes,
not GIANTS heightfield algorithms or complete Bt2RaycastVehicle integration.
"""
import argparse
import ctypes as C
import hashlib
import json
import math
import random
import struct
from inventory import ROOT,write_json
from native_gar import EXPECTED_IMAGE
from native_physics_world import NativePhysicsWorld,F,P,N

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--dll',required=True);a=ap.parse_args()
    lib=C.CDLL(str((ROOT/a.dll).resolve()))
    lib.fs15_ge6_sphere_drop.argtypes=[F,F,F,F,C.c_int,F,P,N];lib.fs15_ge6_sphere_drop.restype=C.c_int
    lib.fs15_ge6_middleware_size.argtypes=[C.c_uint];lib.fs15_ge6_middleware_size.restype=N
    lib.fs15_ge6_hook_trace.argtypes=[P,N,C.c_int,P,N];lib.fs15_ge6_hook_trace.restype=C.c_int
    key_type=C.CFUNCTYPE(C.c_int,P,N)
    lib.fs15_ge6_sort.argtypes=[P,N,key_type,P];lib.fs15_ge6_sort.restype=C.c_int
    expected=[0x1d0,0x150,0x120,0x2900,0xb8,0x2e0,0x380,0xc0,0x40]
    sizes=[lib.fs15_ge6_middleware_size(i) for i in range(9)]
    if sizes!=expected:raise AssertionError(f'Inspected Windows middleware layouts differ: {sizes}')
    rng=random.Random(0xf515b211);digest=hashlib.sha256();maximum=0.;frames=0;contacts=0;cases=64;hook_states=0;hook_calls=0;sort_states=0
    with NativePhysicsWorld() as native:
        baseline=len(native.aligned)
        for case in range(cases):
            gravity=rng.choice([-9.81,-10.,-3.5]);radius=rng.choice([.25,.5,1.,2.]);height=radius+rng.uniform(.1,8.);mass=rng.choice([.25,1.,15.,200.]);dt=rng.choice([1/60,1/120,1/30]);steps=240
            gravity,height,radius,mass,dt=[F(v).value for v in (gravity,height,radius,mass,dt)]
            original=native.sphere_drop(gravity,height,radius,mass,steps,dt);out=(F*(steps*7))()
            if not lib.fs15_ge6_sphere_drop(gravity,height,radius,mass,steps,dt,out,len(out)):raise AssertionError('Recompiled GE6 world failed')
            for i,row in enumerate(original):
                for j,x in enumerate(row):
                    y=out[i*7+j];error=abs(x-y)/max(1.,abs(x),abs(y));maximum=max(maximum,error)
                    if not math.isfinite(y) or error>2e-4:raise AssertionError(f'Trajectory differs case={case} frame={i} lane={j}: {x} vs {y}')
                contacts+=row[6]>0;digest.update(struct.pack('<7f',*row));frames+=1
            if len(native.aligned)!=baseline:raise AssertionError(f'Original physics allocations retained: {len(native.aligned)-baseline}')
        if not contacts:raise AssertionError('No original collision response tested')
        for case in range(96):
            keys=[rng.choice([-2,-1,0,0,0,1,2]) for _ in range(1+case%12)];steps=1+case%3
            original=native.hook_trace(keys,steps);key_array=(C.c_int*len(keys))(*keys);out=(C.c_uint32*20000)()
            count=lib.fs15_ge6_hook_trace(key_array,len(keys),steps,out,len(out))
            rebuilt=[tuple(out[i*3:i*3+3]) for i in range(max(0,count))]
            if count<0 or original!=rebuilt:raise AssertionError(f'Nonempty vehicle solver callbacks differ case={case}: {original[:12]} vs {rebuilt[:12]}')
            digest.update(json.dumps(original).encode());hook_states+=1;hook_calls+=len(original)
            if len(native.aligned)!=baseline:raise AssertionError('Vehicle fixture retained middleware allocations')
        for case in range(1024):
            count=2+case%96;actors=[];keys=[rng.randint(-8,8) for _ in range(count)];events=[[],[]];phase=[0];ids={}
            def get(token):events[phase[0]].append(ids[token]);return C.c_int.from_address(token+8).value
            nk=C.WINFUNCTYPE(C.c_int,P)(get);table=(N*2)(0,C.cast(nk,P).value)
            for i,key in enumerate(keys):
                actor=C.create_string_buffer(16);struct.pack_into('<Qi',actor,0,C.addressof(table),key);ids[C.addressof(actor)]=i;actors.append(actor)
            arrays=[(N*count)(*[C.addressof(x) for x in actors]) for _ in range(2)]
            vector=C.create_string_buffer(0x20);struct.pack_into('<iiQ',vector,4,count,count,0);struct.pack_into('<Q',vector,0x10,C.addressof(arrays[0]))
            phase[0]=0;native.fn(0x27fe40,None,P,P,C.c_int,C.c_int)(vector,None,0,count-1)
            phase[0]=1;pk=key_type(lambda _,token:get(token));lib.fs15_ge6_sort(arrays[1],count,pk,None)
            if list(arrays[0])!=list(arrays[1]) or events[0]!=events[1]:raise AssertionError('Native vehicle sort/equal-key order differs')
            digest.update(json.dumps([ids[x] for x in arrays[0]]).encode());sort_states+=1
    report=dict(status='passed',original_image_sha256=EXPECTED_IMAGE,upstream_revision='0e1a77047cfaa9bf94f7318686ba4179f40c8c1b',
        upstream_archive_sha256='46f19770d23a3b05fb40e181214b977a1451b04324bdb3ba081b82149c41684f',windows_layout_bytes=sizes,
        sphere_drop_lifetimes=cases,trajectory_frames=frames,contact_frames=contacts,maximum_scaled_error=maximum,comparison_digest=digest.hexdigest(),
        nonempty_vehicle_hook_states=hook_states,vehicle_callback_calls=hook_calls,vehicle_sort_states=sort_states,
        rebuilt_source_sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in ['src/ge6/physics_world.cpp','src/ge6/physics_world.h','src/core/physics_vehicle_sort.cpp','src/core/physics_vehicle_sort.h']},
        scope='GE6 world/solver routing over reused upstream Bullet 2014. Actual unchanged native factory, composed original collision-object/body initialization, original sphere shapes, 20-iteration native stepping, generated contacts and teardown compared. Nonempty borrowed vehicle callback routing and native equal-key sort order compared separately. Owner/allocator/timer transport authored, including explicit cleanup of the native factory external pair cache; original powf auxiliary constants mapped. Ordinary sphere scenes only; native heightfield algorithms, broadphase concurrency, Bt2RaycastVehicle implementation and full engine startup remain open.')
    write_json(ROOT/'analysis/ge6-physics-world-validation.json',report);print(json.dumps(report,indent=2))
if __name__=='__main__':main()
