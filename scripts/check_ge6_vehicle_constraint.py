"""Compare unchanged GE6 wheel Jacobian, spring setup and penetration bodies.

Original image and raw native objects stay local. Typed rebuilt transport uses
serialized fixture bytes only through explicit memcpy field adapters.
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
from native_physics_world import NativePhysicsWorld,P,F

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--dll',required=True);args=ap.parse_args()
    lib=C.CDLL(str((ROOT/args.dll).resolve()))
    lib.fs15_ge6_constraint_setup.argtypes=[C.c_int,P,P,P,P,P,P,P]
    lib.fs15_ge6_constraint_push.argtypes=[P,P,P,P,C.c_size_t,C.c_int,C.c_int,C.c_int]
    rng=random.Random(0x6c057);digest=hashlib.sha256();maximum=0.;counts=dict(jacobian=0,spring=0,penetration_states=0,penetration_iterations=0)
    def floats(size,scale=1):return struct.pack('<'+'f'*size,*[rng.uniform(-scale,scale) for _ in range(size)])
    def compare(a,b,kind):
        nonlocal maximum
        if len(a)!=len(b):raise AssertionError('Fixture length differs')
        for off in range(0,len(a),4):
            if a[off:off+4]==b[off:off+4]:continue
            x=struct.unpack_from('<f',a,off)[0];y=struct.unpack_from('<f',b,off)[0];err=abs(x-y)/max(1,abs(x),abs(y));maximum=max(maximum,err)
            if not math.isfinite(x) or not math.isfinite(y) or err>3e-6:raise AssertionError((kind,case,hex(off),x,y,err))
        digest.update(a)
    def body():
        b=C.create_string_buffer(0x300)
        for off in (0x150,0x160,0x170,0x290):C.memmove(C.addressof(b)+off,floats(3),12)
        struct.pack_into('<f',b,0x1a0,rng.choice([0,.25,.5,1,2,4]));return b
    def solver():
        s=C.create_string_buffer(0x100);C.memmove(s,floats(60,.4),240)
        for off in (0x60,0x70,0x80):struct.pack_into('<3f',s,off,*[rng.uniform(0,1) for _ in range(3)])
        struct.pack_into('<Q',s,0xf0,rng.choice([0,1]));return s
    with NativePhysicsWorld() as n:
        for case in range(2048):
            a,b=body(),body();sa,sb=solver(),solver();q=(F*27)()
            for i in range(16):q[i]=rng.uniform(-2,2)
            for i in (3,7,11,15):q[i]=0
            q[17]=rng.uniform(.2,1.5);q[18]=rng.uniform(.05,.4);q[16]=q[17]-q[18]+rng.choice([-.1,0,.02,.04,.1])
            q[19]=rng.uniform(10,20000);q[20]=rng.uniform(2,2000);q[21]=rng.uniform(2,2000)
            q[22]=rng.choice([1/30,1/60,1/120]);q[23]=rng.choice([.1,.2,.4]);q[24]=rng.choice([.1,.2,.8]);q[25]=case%2;q[26]=rng.choice([-.04,0,.04])
            for method in (0,1):
                seed=floats(36);original=C.create_string_buffer(seed,len(seed));rebuilt=C.create_string_buffer(seed,len(seed));stats=C.create_string_buffer(16);other_stats=C.create_string_buffer(16)
                if method==0:
                    n.fn(0x1511a0,None,*([P]*13))(original,q,C.addressof(q)+16,C.addressof(q)+32,C.addressof(q)+48,a,b,sa,sb,stats,C.addressof(stats)+4,C.addressof(stats)+8,C.addressof(stats)+12)
                else:
                    wheel=C.create_string_buffer(0x160);C.memmove(wheel,C.addressof(q)+32,16);C.memmove(C.addressof(wheel)+0x10,C.addressof(q)+16,16)
                    struct.pack_into('<2f',wheel,0xd0,q[17],q[18]);struct.pack_into('<3f',wheel,0xe0,q[19],q[20],q[21]);info=C.create_string_buffer(0x90)
                    struct.pack_into('<f',info,0xc,q[22]);struct.pack_into('<2f',info,0x20,q[23],q[24]);struct.pack_into('<If',info,0x2c,int(q[25]),q[26])
                    n.fn(0x150f50,None,P,P,P,F,P,P,P,P,P,P)(original,q,C.addressof(q)+48,q[16],wheel,a,b,sa,sb,info)
                lib.fs15_ge6_constraint_setup(method,a,b,sa,sb,q,rebuilt,other_stats)
                compare(bytes(original),bytes(rebuilt),'spring' if method else 'jacobian')
                if not method:compare(bytes(stats),bytes(other_stats),'jacobian stats')
                counts['spring' if method else 'jacobian']+=1
        for case in range(1024):
            count=case%13;activation=case%6;active=case%5!=0;alias=case%7==0;steps=1+case%12
            sa,sb=solver(),solver();ra=C.create_string_buffer(bytes(sa),len(sa));rb=C.create_string_buffer(bytes(sb),len(sb))
            wheels=C.create_string_buffer(max(1,count*0x160));records=C.create_string_buffer(max(1,count*0x1a0))
            for i in range(count):
                C.memmove(C.addressof(wheels)+i*0x160,floats(3,.4),12);struct.pack_into('<Q',wheels,i*0x160+0x28,0 if (i+case)%4==0 else 1)
                C.memmove(C.addressof(records)+i*0x1a0+0x20,floats(36,.4),144);struct.pack_into('<QQ',records,i*0x1a0,C.addressof(sa),C.addressof(sa if alias else sb))
                struct.pack_into('<f',records,i*0x1a0+0xa0,0 if (i+case)%6==0 else rng.uniform(-.4,.4))
            rebuilt=C.create_string_buffer(bytes(records),len(records));body_=C.create_string_buffer(0x300);vehicle=C.create_string_buffer(0xd0)
            struct.pack_into('<I',body_,0xf4,activation);struct.pack_into('<Q',vehicle,8,C.addressof(body_));struct.pack_into('<I',vehicle,0x3c,count);struct.pack_into('<Q',vehicle,0x48,C.addressof(wheels));struct.pack_into('<Q',vehicle,0x88,C.addressof(records));vehicle[0xc4]=bytes([int(active)])
            for step in range(steps):
                n.fn(0x154d70,None,P)(vehicle);lib.fs15_ge6_constraint_push(ra,rb,rebuilt,wheels,count,activation,active,alias)
                compare(bytes(sa),bytes(ra),'push body A');compare(bytes(sb),bytes(rb),'push body B');compare(bytes(records),bytes(rebuilt),'push constraints');counts['penetration_iterations']+=1
            counts['penetration_states']+=1
    report=dict(status='passed',original_image_sha256=EXPECTED_IMAGE,states=counts,maximum_scaled_error=maximum,comparison_digest=digest.hexdigest(),
        rebuilt_source_sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in ['src/ge6/vehicle_constraint.h','src/ge6/vehicle_constraint.cpp']},
        scope='Three bounded GE6 wheel Jacobian, suspension spring setup and complete split-penetration phase bodies. Unchanged native image compared using explicit local fixture objects; authored typed transport uses Bullet public fields/APIs. Includes repeated shared-body constraints, aliased body pairs, static solver bodies, sleeping/inactive guards, contact misses and impulse clamping. Full vehicle setup, velocity/friction/drivetrain, registration, terrain and playable engine startup remain open.')
    write_json(ROOT/'analysis/ge6-vehicle-constraint-validation.json',report);print(json.dumps(report,indent=2))
if __name__=='__main__':main()
