"""Differential GE6 wheel velocity/friction phase with empty differential network.

The original driver helper executes unchanged with zero differential nodes;
the rebuilt caller supplies an explicit no-op driver service. This validates
the wheel phase, not recovery of the drivetrain/motor network itself.
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
    lib.fs15_ge6_constraint_velocity.argtypes=[P,P,P,P,N,C.c_int,C.c_int,C.c_int,F,P,P]
    rng=random.Random(0x6be10c);digest=hashlib.sha256();maximum=0.;states=1024;iterations=0;gravity_calls=0;brake_states=0;contact_states=0
    def fill(buffer,offset,count,scale):struct.pack_into('<'+'f'*count,buffer,offset,*[rng.uniform(-scale,scale) for _ in range(count)])
    def compare(a,b,kind):
        nonlocal maximum
        for offset in range(0,len(a),4):
            if a[offset:offset+4]==b[offset:offset+4]:continue
            x=struct.unpack_from('<f',a,offset)[0];y=struct.unpack_from('<f',b,offset)[0];error=abs(x-y)/max(1,abs(x),abs(y));maximum=max(maximum,error)
            if not math.isfinite(x) or not math.isfinite(y) or error>2e-5:raise AssertionError((kind,case,step,hex(offset),x,y,error))
        canonical=bytearray(a)
        if kind=='constraints':
            # Compare the actual borrowed pointers above, but exclude their
            # process-specific addresses from the reproducible result digest.
            for i in range(len(a)//0x1a0):canonical[i*0x1a0:i*0x1a0+16]=bytes(16)
        digest.update(canonical)
    def solver():
        result=C.create_string_buffer(0x100);fill(result,0,60,.3)
        for off in (0x60,0x70,0x80):struct.pack_into('<3f',result,off,*[rng.uniform(.1,1) for _ in range(3)])
        struct.pack_into('<Q',result,0xf0,rng.choice([0,1,1]));return result
    with NativePhysicsWorld() as native:
        for case in range(states):
            count=case%7;activation=case%6;active=case%5!=0;alias=case%9==0;dt=F(rng.choice([1/30,1/60,1/120])).value;steps=1+case%12
            sa,sb=solver(),solver();ra=C.create_string_buffer(bytes(sa),len(sa));rb=C.create_string_buffer(bytes(sb),len(sb))
            wheels=C.create_string_buffer(max(4,count*0x160));records=C.create_string_buffer(max(4,count*0x1a0))
            for i in range(count):
                w=i*0x160;r=i*0x1a0;fill(wheels,w,3,1);struct.pack_into('<Q',wheels,w+0x28,0 if (case+i)%5==0 else 1)
                struct.pack_into('<f',wheels,w+0xdc,rng.uniform(.2,1));struct.pack_into('<f',wheels,w+0x108,rng.uniform(-20,20))
                struct.pack_into('<4f',wheels,w+0x110,rng.uniform(.5,10),rng.uniform(.2,2),rng.uniform(.5,10),rng.uniform(.2,1.5))
                struct.pack_into('<2f',wheels,w+0x120,rng.uniform(0,5),rng.uniform(.1,2));struct.pack_into('<f',wheels,w+0x134,0 if case%3==0 else rng.uniform(.1,100));struct.pack_into('<f',wheels,w+0x158,rng.uniform(-.1,.1))
                fill(records,r+0x10,3,.3);struct.pack_into('<f',records,r+0x18,rng.uniform(0,.5))
                for off in (0x20,0xb0,0x120):
                    fill(records,r+off,20,.3)
                    for vector in range(5):struct.pack_into('<f',records,r+off+vector*16+12,0)
                struct.pack_into('<QQ',records,r,C.addressof(sa),C.addressof(sa if alias else sb))
                struct.pack_into('<4f',records,r+0x70,rng.uniform(.1,.5),rng.uniform(-.2,0),rng.uniform(0,.3),rng.uniform(-1,0));struct.pack_into('<f',records,r+0x80,rng.uniform(-1,-.2))
                struct.pack_into('<4f',records,r+0x90,rng.uniform(-2,-1),rng.uniform(-.2,0),0 if case%3==0 else rng.uniform(.1,.4),rng.uniform(-.2,0))
                for off in (0x100,0x170):
                    struct.pack_into('<2f',records,r+off,rng.uniform(.1,1),rng.uniform(-.1,.1));struct.pack_into('<f',records,r+off+12,rng.uniform(-.1,.1))
            rebuilt_wheels=C.create_string_buffer(bytes(wheels),len(wheels));rebuilt_records=C.create_string_buffer(bytes(records),len(records))
            body=C.create_string_buffer(0x300);vehicle=C.create_string_buffer(0xd0);struct.pack_into('<I',body,0xf4,activation)
            struct.pack_into('<Q',vehicle,8,C.addressof(body));struct.pack_into('<I',vehicle,0x3c,count);struct.pack_into('<Q',vehicle,0x48,C.addressof(wheels));struct.pack_into('<Q',vehicle,0x88,C.addressof(records));vehicle[0xc4]=bytes([int(active)])
            gravity=(F*4)(*rng.choice([(0,-9.81,0,0),(0,9.81,0,0),(0,0,0,0),(-2,-3,1,0)]));events=[0]
            @C.WINFUNCTYPE(P,P,P)
            def get_gravity(_self,out):events[0]+=1;C.memmove(out,gravity,16);return out
            table=(N*19)();table[18]=C.cast(get_gravity,P).value;world=(N*1)(C.addressof(table));struct.pack_into('<Q',vehicle,0xc8,C.addressof(world))
            for step in range(steps):
                events[0]=0;native.fn(0x1540b0,None,P,P,F)(vehicle,None,dt);counters=(C.c_int*2)()
                lib.fs15_ge6_constraint_velocity(ra,rb,rebuilt_records,rebuilt_wheels,count,activation,active,alias,dt,gravity,counters)
                if counters[0]!=int(activation!=2 and active) or counters[1]!=events[0]:raise AssertionError(('services',case,step,list(counters),events[0]))
                compare(bytes(sa),bytes(ra),'solver A');compare(bytes(sb),bytes(rb),'solver B');compare(bytes(records),bytes(rebuilt_records),'constraints');compare(bytes(wheels),bytes(rebuilt_wheels),'wheels')
                iterations+=1;gravity_calls+=events[0]
            brake_states+=case%3!=0 and count>0;contact_states+=count>0 and active and activation!=2
    report=dict(status='passed',original_image_sha256=EXPECTED_IMAGE,vehicle_states=states,successive_velocity_iterations=iterations,original_gravity_queries=gravity_calls,configured_brake_states=brake_states,active_nonempty_states=contact_states,maximum_scaled_error=maximum,comparison_digest=digest.hexdigest(),digest_excludes_native_solver_pointers=True,
        rebuilt_source_sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in ['src/ge6/vehicle_velocity.h','src/ge6/vehicle_velocity.cpp','src/ge6/vehicle_constraint.h','src/core/tire_forces.h']},
        scope='Bounded original 0x1401540b0 wheel velocity/friction phase, valid finite positive tire parameters, empty differential network. Original driver helper executes with zero nodes; typed rebuilt driver callback is explicit no-op transport. Compares complete wheel/constraint/solver records across repeated shared-body/aliased/static cases, sleeping/inactive/contact guards, brakes and gravity-query order. Drivetrain network, full vehicle setup/registration and playable startup remain open.')
    write_json(ROOT/'analysis/ge6-vehicle-velocity-validation.json',report);print(json.dumps(report,indent=2))
if __name__=='__main__':main()
