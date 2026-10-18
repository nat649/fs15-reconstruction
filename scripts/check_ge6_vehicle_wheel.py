"""Differential GE6 suspension rays/filter/readback, not a full vehicle solver.

Requires the unchanged local pinned image on Windows. Public results contain
only counts, source hashes and comparisons; original bytes remain local.
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
from native_physics_world import NativePhysicsWorld,P,F,N

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--dll',required=True);args=ap.parse_args()
    lib=C.CDLL(str((ROOT/args.dll).resolve()))
    lib.fs15_ge6_wheel_frame.argtypes=[P,P]
    lib.fs15_ge6_wheel_ray.argtypes=[P,C.c_int,P]
    lib.fs15_ge6_wheel_filter.argtypes=[C.c_int]*8;lib.fs15_ge6_wheel_filter.restype=C.c_int
    lib.fs15_ge6_wheel_finish.argtypes=[P,N,C.c_int,C.c_int,F,F,F,F,P]
    rng=random.Random(0x6b715);digest=hashlib.sha256();maximum=0.;counts={'ray_frame':0,'wheel_ray':0,'collision_filter':0,'finish':0}
    def compare(x,y,kind):
        nonlocal maximum
        for a,b in zip(x,y):
            if not math.isfinite(a) or not math.isfinite(b):raise AssertionError((kind,a,b))
            error=abs(a-b)/max(1,abs(a),abs(b));maximum=max(maximum,error)
            if error>2e-6:raise AssertionError((kind,a,b,error))
        digest.update(struct.pack('<'+'f'*len(x),*x));counts[kind]+=1
    def transform():
        # Include arbitrary affine bases to detect association and accidental
        # normalization. Real rigid transforms are a subset of this domain.
        v=[rng.uniform(-2,2) for _ in range(16)];v[3]=v[7]=v[11]=v[15]=0;return v
    with NativePhysicsWorld() as n:
        for case in range(768):
            values=(F*46)(*(transform()+[rng.uniform(-5,5) for _ in range(6)]+[rng.uniform(.2,3),rng.uniform(0,.3),rng.uniform(0,.3),rng.uniform(.2,2),rng.uniform(0,1)]+[rng.uniform(-1,1) for _ in range(3)]+transform()))
            wheel=C.create_string_buffer(0x160);owner=C.create_string_buffer(0x300);target=C.create_string_buffer(0x300);actor=C.create_string_buffer(0xd0)
            C.memmove(C.addressof(owner)+0x10,values,64);C.memmove(C.addressof(target)+0x10,C.addressof(values)+120,64)
            struct.pack_into('<6f',wheel,0xa0,*values[16:19],0,0,0);struct.pack_into('<3f',wheel,0xb0,*values[19:22]);struct.pack_into('<4f',wheel,0xd0,*values[22:26])
            struct.pack_into('<Q',actor,8,C.addressof(owner));struct.pack_into('<I',wheel,0x138,0)
            # Original SIMD stores require 16-byte outputs. ctypes' small
            # inline float[4] storage can be only 8-byte aligned on Windows.
            point_storage=C.create_string_buffer(32);direction_storage=C.create_string_buffer(32)
            point=(F*4).from_address((C.addressof(point_storage)+15)&~15);direction=(F*4).from_address((C.addressof(direction_storage)+15)&~15);rebuilt=(F*8)()
            n.fn(0x150a10,None,P,P,P,P)(wheel,C.addressof(owner)+0x10,point,direction);lib.fs15_ge6_wheel_frame(values,rebuilt)
            compare(list(point)+list(direction),list(rebuilt),'ray_frame')
            mode=case%3
            # Authored world service supplies a controlled native LocalRayResult;
            # the unchanged GE6 callback and wheel body still execute.
            @C.WINFUNCTYPE(None,P,P,P,P)
            def ray_test(_world,_from,_to,callback):
                if not mode:return
                result=C.create_string_buffer(0x28);struct.pack_into('<Q',result,0,C.addressof(target));struct.pack_into('<3f',result,0x10,*values[27:30]);struct.pack_into('<f',result,0x20,values[26])
                table=P.from_address(callback).value
                C.WINFUNCTYPE(F,P,P,C.c_uint8)(P.from_address(table+0x10).value)(callback,result,mode==1)
            table=(N*8)();table[7]=C.cast(ray_test,P).value;world=(N*1)(C.addressof(table))
            n.fn(0x153330,None,P,P,P)(actor,world,wheel);rebuilt=(F*9)();lib.fs15_ge6_wheel_ray(values,mode,rebuilt)
            original=[float(bool(P.from_address(C.addressof(wheel)+0x28).value)),*struct.unpack_from('<3f',wheel,0x10),*struct.unpack_from('<3f',wheel,0),struct.unpack_from('<f',wheel,0x20)[0],struct.unpack_from('<f',wheel,0x148)[0]]
            compare(original,list(rebuilt),'wheel_ray')
        for case in range(4096):
            group,mask,pg,pm=[rng.choice([0,1,2,4,8,16,255,32768,65535]) for _ in range(4)];flags=rng.choice([0,1,2,4,5,6]);kind=rng.choice([1,2]);own=case%11==0;ignored=case%7==0
            owner=C.create_string_buffer(0x300);other=C.create_string_buffer(0x300);object_=owner if own else other;proxy=C.create_string_buffer(0x40);callback=C.create_string_buffer(0x68)
            @C.WINFUNCTYPE(C.c_uint8,P,P)
            def check(_self,_other):return not ignored
            table=(N*3)(0,0,C.cast(check,P).value);struct.pack_into('<Q',owner,0,C.addressof(table));struct.pack_into('<I',owner,0x124,int(ignored))
            struct.pack_into('<I',object_,0xe8,flags);struct.pack_into('<I',object_,0x108,2 if own else kind)
            struct.pack_into('<QHH',proxy,0,C.addressof(object_),pg,pm);struct.pack_into('<HH',callback,0x18,group,mask);struct.pack_into('<Q',callback,0x60,C.addressof(owner))
            original=n.fn(0x151930,C.c_uint8,P,P)(callback,proxy);rebuilt=lib.fs15_ge6_wheel_filter(group,mask,pg,pm,flags,kind,own,ignored)
            if original!=rebuilt:raise AssertionError(('filter',case,(group,mask,pg,pm,flags,kind,own,ignored),original,rebuilt))
            digest.update(bytes([original]));counts['collision_filter']+=1
        for case in range(1024):
            count=case%17;activation=case%6;active=case%5!=0;dt=F(rng.choice([1/30,1/60,1/120])).value
            impulse,scale,previous=[F(rng.uniform(.2,100)).value for _ in range(3)];values=(F*max(1,count*8))()
            wheel=C.create_string_buffer(max(1,count*0x160));constraints=C.create_string_buffer(max(1,count*0x1a0));body=C.create_string_buffer(0x300);actor=C.create_string_buffer(0xd0)
            struct.pack_into('<I',body,0xf4,activation);struct.pack_into('<Q',actor,8,C.addressof(body));struct.pack_into('<I',actor,0x3c,count);struct.pack_into('<Q',actor,0x48,C.addressof(wheel));struct.pack_into('<Q',actor,0x88,C.addressof(constraints))
            struct.pack_into('<f',actor,0x98,scale);struct.pack_into('<f',actor,0xb8,impulse);struct.pack_into('<f',actor,0xc0,previous);actor[0xc4]=bytes([int(active)])
            for i in range(count):
                row=[rng.uniform(-400,400),rng.uniform(-20,20),rng.uniform(-100,100),rng.uniform(-2,2),rng.uniform(-2,2),71,72,73]
                for j,x in enumerate(row):values[i*8+j]=x
                struct.pack_into('<2f',wheel,i*0x160+0x108,*values[i*8:i*8+2]);struct.pack_into('<3f',wheel,i*0x160+0x14c,*values[i*8+5:i*8+8]);struct.pack_into('<f',constraints,i*0x1a0+0x7c,values[i*8+2]);struct.pack_into('<2f',constraints,i*0x1a0+0x10,*values[i*8+3:i*8+5])
            before=bytes(wheel);n.fn(0x155140,None,P,P,F)(actor,None,dt);rebuilt=(F*(count*4+1))()
            lib.fs15_ge6_wheel_finish(values,count,activation,active,dt,impulse,scale,previous,rebuilt)
            original=[];allowed=set()
            for i in range(count):
                original.extend([struct.unpack_from('<f',wheel,i*0x160+0x10c)[0],*struct.unpack_from('<3f',wheel,i*0x160+0x14c)])
                for offset,size in [(0x10c,4),(0x14c,12)]:allowed.update(range(i*0x160+offset,i*0x160+offset+size))
            if any(x!=y and i not in allowed for i,(x,y) in enumerate(zip(before,bytes(wheel)))):raise AssertionError('Unexpected native wheel mutation')
            original.append(struct.unpack_from('<f',actor,0xc0)[0]);compare(original,list(rebuilt),'finish')
    report=dict(status='passed',original_image_sha256=EXPECTED_IMAGE,states=counts,maximum_scaled_error=maximum,comparison_digest=digest.hexdigest(),
        rebuilt_source_sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in ['src/ge6/vehicle_wheel.h','src/ge6/vehicle_wheel.cpp']},
        scope='Five bounded GE6 suspension frame/filter/closest-hit/raycast/finish bodies. Typed Bullet transport is authored. Controlled ray service tests unchanged original callback and wheel arithmetic; a separate portable integration test uses actual Bullet ray queries. Vehicle constraint setup, friction/velocity and penetration solver, original heightfield collision and playable engine startup remain open.')
    write_json(ROOT/'analysis/ge6-vehicle-wheel-validation.json',report);print(json.dumps(report,indent=2))
if __name__=='__main__':main()
