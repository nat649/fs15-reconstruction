"""Compare a complete four-wheel GE6 native solver loop with the typed class.

Scenario geometry and driver commands are authored fixture data. This is not
an original vehicle XML load or a playable rendering/game loop.
"""
import argparse
import ctypes as C
import hashlib
import json
import math
import struct
from inventory import ROOT,write_json
from native_gar import EXPECTED_IMAGE
from native_physics_world import NativePhysicsWorld,P,F,U,N

def original_course(n,commands,steps):
    world=n.create(-9.81);U.from_address(world+0xa4).value=20;U.from_address(world+0xd0).value|=0x70
    objects=[n.sphere(world,0,1000,-1000),n.sphere(world,1000,.5,1.8)];body=C.addressof(objects[1][0]);U.from_address(body+0xf4).value=4
    actor=C.create_string_buffer(0xd0);wheels=C.create_string_buffer(4*0x160);records=C.create_string_buffer(4*0x1a0);nodes=C.create_string_buffer(36)
    n.fn(0x1524f0,P,P,P,P)(actor,body,world)
    for off,count,data in [(0x38,4,wheels),(0x58,3,nodes),(0x78,4,records)]:
        struct.pack_into('<ii',actor,off+4,count,count);struct.pack_into('<Q',actor,off+16,C.addressof(data));actor[off+24]=b'\0'
    for i,(a,b) in enumerate([(0,1),(2,3),(20,21)]):struct.pack_into('<ffHH',nodes,i*12,.5,2,a,b)
    struct.pack_into('<f',actor,0x98,1);struct.pack_into('<f',actor,0xb0,2000)
    for i in range(4):
        off=i*0x160;x=1 if i%2 else -1;z=1.5 if i<2 else -1.5
        struct.pack_into('<3f',wheels,off+0xa0,x,0,z);struct.pack_into('<3f',wheels,off+0xb0,0,-1,0);struct.pack_into('<3f',wheels,off+0xc0,1,0,0)
        struct.pack_into('<7f',wheels,off+0xd0,1,.2,.5,.5,30000,5000,5000);struct.pack_into('<3f',wheels,off+0xf0,x,-1,z)
        struct.pack_into('<f',wheels,off+0x104,.1);struct.pack_into('<4f',wheels,off+0x110,5,1,5,1);struct.pack_into('<f',wheels,off+0x124,1);struct.pack_into('<hh',wheels,off+0x12c,1,-1)
    vehicle_array=(N*1)(C.addressof(actor));struct.pack_into('<ii',(C.c_char*0x1f0).from_address(world),0x1d4,1,1);P.from_address(world+0x1e0).value=C.addressof(vehicle_array);C.c_uint8.from_address(world+0x1e8).value=0
    # The original fixed solver body is normally initialized by engine startup;
    # this isolated fixture supplies the same zeroed static body explicitly.
    C.memset(n.base+0x162e410,0,0x100);rows=[]
    try:
        for step in range(steps):
            torque,target,ratio,steer,brake=commands[step*5:step*5+5]
            struct.pack_into('<f',actor,0xa8,torque);struct.pack_into('<f',actor,0xa0,target);struct.pack_into('<f',actor,0xac,ratio)
            for i in range(4):struct.pack_into('<f',wheels,i*0x160+0x100,steer if i<2 else 0);struct.pack_into('<f',wheels,i*0x160+0x134,brake)
            n.fn(0x2ed5c0,C.c_int,P,F,C.c_int,F)(world,1/60,20,1/60)
            row=[*struct.unpack('<3f',C.string_at(body+0x40,12)),*struct.unpack('<3f',C.string_at(body+0x180,12)),*struct.unpack('<3f',C.string_at(body+0x190,12)),struct.unpack_from('<f',actor,0x9c)[0],struct.unpack_from('<f',actor,0xb4)[0],struct.unpack_from('<f',actor,0xc0)[0]]
            for i in range(4):
                off=i*0x160;row.extend([struct.unpack_from('<f',wheels,off+0x108)[0],struct.unpack_from('<f',wheels,off+0x10c)[0],*struct.unpack_from('<3f',wheels,off+0x14c),struct.unpack_from('<f',wheels,off+0x148)[0],float(bool(P.from_address(C.addressof(wheels)+off+0x28).value))])
            rows.append(row)
    finally:
        n.fn(0x1525d0,None,P)(actor)
        for b,s,_ in reversed(objects):
            n.fn(0x2ed9c0,None,P,P)(world,b)
            for obj in (b,s):
                table=P.from_address(C.addressof(obj)).value;C.WINFUNCTYPE(P,P,U)(P.from_address(table).value)(obj,0)
        n.destroy(world)
    return rows
def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--dll',required=True);args=ap.parse_args();lib=C.CDLL(str((ROOT/args.dll).resolve()))
    lib.fs15_ge6_vehicle_course.argtypes=[C.c_int,P,P,N];lib.fs15_ge6_vehicle_course.restype=C.c_int
    digest=hashlib.sha256();maximum=0;frames=0;motion=0;readback=0;groups={}
    with NativePhysicsWorld() as n:
        for case in range(6):
            steps=120;commands=(F*(steps*5))()
            for step in range(steps):
                torque=500 if step<90 else 0;ratio=-15 if case%2 else 15;steer=[0,.04,-.04][case//2] if step>=30 else 0;brake=1000 if step>=90 else 0
                commands[step*5:step*5+5]=[torque,300,ratio,steer,brake]
            original=original_course(n,commands,steps);out=(F*(steps*40))()
            if not lib.fs15_ge6_vehicle_course(steps,commands,out,len(out)):raise AssertionError('Recompiled complete vehicle failed')
            for step,row in enumerate(original):
                for lane,x in enumerate(row):
                    y=out[step*40+lane];error=abs(x-y)/max(1,abs(x),abs(y));maximum=max(maximum,error)
                    if not math.isfinite(x) or not math.isfinite(y):raise AssertionError((case,step,lane,x,y))
                    name=('position' if lane<3 else 'linear_velocity' if lane<6 else 'angular_velocity' if lane<9 else 'motor' if lane<12 else ['wheel_speed','wheel_angle','wheel_force','lateral_slip','longitudinal_slip','suspension_length','contact'][(lane-12)%7])
                    group=groups.setdefault(name,dict(maximum_absolute_error=0,maximum_scaled_error=0));group['maximum_absolute_error']=max(group['maximum_absolute_error'],abs(x-y));group['maximum_scaled_error']=max(group['maximum_scaled_error'],error)
                    if lane<9:motion=max(motion,abs(x-y))
                    else:readback=max(readback,error)
                digest.update(struct.pack('<40f',*row));frames+=1
    report=dict(status='observed_differences',comparison_is_parity_pass=False,error_groups=groups,maximum_motion_absolute_error=motion,maximum_readback_scaled_error=readback,original_image_sha256=EXPECTED_IMAGE,vehicle_lifetimes=6,trajectory_frames=frames,wheels_per_vehicle=4,maximum_scaled_error=maximum,comparison_digest=digest.hexdigest(),
        rebuilt_source_sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in ['src/ge6/raycast_vehicle.h','src/ge6/raycast_vehicle.cpp']},
        scope='Diagnostic, not a parity pass: complete native Bt2RaycastVehicle callback loop in original GE6 world versus experimental typed class over compatible upstream Bullet. Four wheels, nested motor/differentials, acceleration from rest, forward/reverse ratios, steering and brakes; scenario sphere bodies/commands authored. Whole-loop motion/readback differences remain unresolved; do not count the class setup/steering paths as native-validated entries. Original vehicle XML/joints/script control, terrain/renderer/startup and playable game remain open.')
    write_json(ROOT/'analysis/ge6-raycast-vehicle-validation.json',report);print(json.dumps(report,indent=2))
if __name__=='__main__':main()
