"""Compare the original GE6 motor/clutch/differential bodies with typed ports."""
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
    lib=C.CDLL(str((ROOT/args.dll).resolve()));lib.fs15_ge6_drivetrain.argtypes=[P,P,P,N,F,C.c_int]
    rng=random.Random(0x6d1f);digest=hashlib.sha256();maximum=0.;states=1536;iterations=0
    def compare(a,b,kind):
        nonlocal maximum
        for off in range(0,len(a),4):
            if a[off:off+4]==b[off:off+4]:continue
            x=struct.unpack_from('<f',a,off)[0];y=struct.unpack_from('<f',b,off)[0];error=abs(x-y)/max(1,abs(x),abs(y));maximum=max(maximum,error)
            if not math.isfinite(x) or not math.isfinite(y) or error>3e-5:raise AssertionError((kind,case,step,hex(off),x,y,error))
        canonical=bytearray(a)
        if kind=='motor':canonical[0x48:0x50]=bytes(8);canonical[0x68:0x70]=bytes(8)
        digest.update(canonical)
    with NativePhysicsWorld() as n:
        for case in range(states):
            count=case%21;nodes=C.create_string_buffer(max(4,count*12));wheels=C.create_string_buffer(20*0x160);actor=C.create_string_buffer(0xd0)
            for i in range(20):
                struct.pack_into('<f',wheels,i*0x160+0xdc,rng.uniform(.1,2));struct.pack_into('<f',wheels,i*0x160+0x108,rng.choice([0.,rng.uniform(-30,30)]));struct.pack_into('<f',wheels,i*0x160+0x124,rng.uniform(.01,2));struct.pack_into('<f',wheels,i*0x160+0x144,-99)
            for i in range(count):
                # Include shared/repeated wheel children and nested nodes. Every
                # differential reference points backwards, as native setup expects.
                children=list(range(20))+list(range(20,20+i));a=rng.choice(children);b=rng.choice(children)
                struct.pack_into('<ffHH',nodes,i*12,rng.choice([0.,.0001,.5,.9999,1.,rng.uniform(.01,.99)]),rng.uniform(1,5),a,b)
            struct.pack_into('<Q',actor,0x48,C.addressof(wheels));struct.pack_into('<I',actor,0x5c,count);struct.pack_into('<Q',actor,0x68,C.addressof(nodes))
            struct.pack_into('<10f',actor,0x98,rng.choice([.1,1,1000]),rng.uniform(0,300),rng.uniform(100,400),.002,rng.choice([0,rng.uniform(-50,200)]),rng.choice([0,1,-1,.3,-3]),rng.uniform(.1,300),rng.uniform(-10,10),0,0)
            rebuilt_actor=C.create_string_buffer(bytes(actor),len(actor));rebuilt_wheels=C.create_string_buffer(bytes(wheels),len(wheels));dt=F(rng.choice([1/30,1/60,1/120])).value
            step=-1;n.fn(0x155940,None,P)(actor);lib.fs15_ge6_drivetrain(rebuilt_actor,rebuilt_wheels,nodes,count,dt,1);compare(bytes(wheels),bytes(rebuilt_wheels),'weights');compare(bytes(actor),bytes(rebuilt_actor),'motor')
            for step in range(1+case%16):
                n.fn(0x155d50,None,P,F)(actor,dt);lib.fs15_ge6_drivetrain(rebuilt_actor,rebuilt_wheels,nodes,count,dt,0)
                compare(bytes(wheels),bytes(rebuilt_wheels),'wheel speeds');compare(bytes(actor),bytes(rebuilt_actor),'motor');iterations+=1
    report=dict(status='passed',original_image_sha256=EXPECTED_IMAGE,network_states=states,successive_drive_iterations=iterations,maximum_scaled_error=maximum,comparison_digest=digest.hexdigest(),digest_excludes_native_pointers=True,
        rebuilt_source_sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in ['src/ge6/vehicle_drive.h','src/ge6/vehicle_drive.cpp']},
        scope='Original 0x140155940 differential weight preparation and 0x140155d50 motor/clutch/differential update. Native bodies execute unchanged with 0..20 nodes, backward nested references and shared wheel children, both speed/ratio signs, clutch/torque clamps and successive updates. Typed transport authored. Full vehicle creation, original XML/script control and gameplay remain separate.')
    write_json(ROOT/'analysis/ge6-vehicle-drive-validation.json',report);print(json.dumps(report,indent=2))
if __name__=='__main__':main()
