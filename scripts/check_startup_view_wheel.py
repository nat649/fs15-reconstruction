"""Compare native view lifetime and registry-to-tire forces, using owned seams.

Original image remains local. No entry point, GPU driver or world solver runs.
"""
import argparse
import ctypes as C
import hashlib
import json
import math
import random
import struct
from inventory import ROOT
from native_gar import NativeGarCipher, EXPECTED_IMAGE

P=C.c_void_p
N=C.c_size_t
F=C.c_float
CREATE=C.CFUNCTYPE(N,P,C.c_uint32,N,C.c_uint32)
DESTROY=C.CFUNCTYPE(None,P,N,C.c_uint32)
TAN=C.CFUNCTYPE(F,P,F)
class Services(C.Structure):
    _fields_=[('user',P),('create',CREATE),('destroy',DESTROY)]

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--dll',required=True);a=parser.parse_args()
    core=C.CDLL(str((ROOT/a.dll).resolve()))
    core.fs15_scene_view_construct.argtypes=[P,N,P,N,P];core.fs15_scene_view_construct.restype=C.c_int
    core.fs15_scene_view_destruct.argtypes=[P,N,P];core.fs15_scene_view_destruct.restype=C.c_int
    core.fs15_script_wheel_forces.argtypes=[P,P,P,N,P,P];core.fs15_script_wheel_forces.restype=C.c_int
    rng=random.Random(0xf51512ca00);digest=hashlib.sha256();max_error=0.;view_count=wheel_count=0
    def ptr(x):return C.addressof(x)
    with NativeGarCipher(math_helpers=True) as native:
        at=lambda off,result,*types:C.WINFUNCTYPE(result,*types)(native.base+off)
        construct=at(0x12ca00,P,P,P);destroy=at(0x12cc40,None,P)
        forces=at(0xe76c0,None,P,P);tan=at(0x4566ec,F,F)
        driver_global=C.c_void_p.from_address(native.base+0x162ba08);saved_driver=driver_global.value
        manager_global=C.c_void_p.from_address(native.base+0x162d138);saved_manager=manager_global.value
        phase=[0];events=[[],[]];views=[];mask=[0];calls=[0,0]
        def snapshot(name,*values):events[phase[0]].append((name,*values,views[phase[0]].raw))
        def deleted(token,deleting):snapshot('destroy',token,deleting)
        ndestroy=C.WINFUNCTYPE(None,P,C.c_uint32)(deleted)
        buffer_table=(N*1)(C.cast(ndestroy,P).value)
        buffers=[(N*1)(ptr(buffer_table)),(N*1)(ptr(buffer_table))]
        def created(size,data,flags):
            i=calls[phase[0]];calls[phase[0]]+=1
            snapshot('create',size,data,flags)
            return 0 if mask[0]&(1<<i) else ptr(buffers[i])
        def ncreate(_,descriptor):
            size=struct.unpack('<I',C.string_at(descriptor,4))[0]
            data,flags=struct.unpack('<QI',C.string_at(descriptor+8,12))
            return created(size,data,flags)
        ncreate_cb=C.WINFUNCTYPE(P,P,P)(ncreate)
        driver_table=(N*69)();driver_table[68]=C.cast(ncreate_cb,P).value;driver=(N*1)(ptr(driver_table))
        create_cb=CREATE(lambda _,size,data,flags:created(size,data,flags))
        destroy_cb=DESTROY(lambda _,token,deleting:deleted(token,deleting))
        services=Services(None,create_cb,destroy_cb)
        driver_global.value=ptr(driver)
        try:
            for iteration in range(1024):
                payload=rng.randbytes(0x208);views=[C.create_string_buffer(payload,len(payload)) for _ in range(2)]
                context=C.create_string_buffer(rng.randbytes(0x80),0x80)
                struct.pack_into('<II',context,0x78,*rng.choice([(800,600),(1280,720),(1920,1080),(0,0),(0xffffffff,1)]))
                mask[0]=iteration%4;calls[:]=[0,0];events[:]=[[],[]]
                phase[0]=0
                if construct(views[0],context)!=ptr(views[0]):raise AssertionError('Native constructor identity')
                phase[0]=1
                if not core.fs15_scene_view_construct(views[1],0x1e8,context,0x80,C.byref(services)):raise AssertionError('Recovered constructor rejected')
                if views[0].raw!=views[1].raw or events[0]!=events[1]:raise AssertionError(f'Construction/ordered callback mismatch {iteration}')
                phase[0]=0;destroy(views[0]);phase[0]=1
                if not core.fs15_scene_view_destruct(views[1],0x1e8,C.byref(services)):raise AssertionError('Recovered destructor rejected')
                if views[0].raw!=views[1].raw or events[0]!=events[1]:raise AssertionError(f'Destruction/ordered callback mismatch {iteration}')
                canonical=bytearray(views[0].raw)
                for off in (0x1d0,0x1d8,0x1e0):
                    if struct.unpack_from('<Q',canonical,off)[0]:struct.pack_into('<Q',canonical,off,off)
                digest.update(canonical);view_count+=1
            manager=C.create_string_buffer(0x490);manager_global.value=ptr(manager)
            nil=C.create_string_buffer(0x30);nil[0x29]=b'\x01'
            node=C.create_string_buffer(0x30);records=C.create_string_buffer(3*0x160);wheel_set=C.create_string_buffer(0x50)
            struct.pack_into('<i',wheel_set,0x3c,3);struct.pack_into('<Q',wheel_set,0x48,ptr(records))
            struct.pack_into('<QQQQQ',node,0,ptr(nil),ptr(nil),ptr(nil),0x8000000000000000,ptr(wheel_set))
            struct.pack_into('<Q',nil,8,ptr(node));struct.pack_into('<Q',manager,0x250,ptr(nil))
            actor=C.create_string_buffer(0x38);obj=C.create_string_buffer(0x120)
            struct.pack_into('<Q',obj,0x118,ptr(actor));struct.pack_into('<Q',actor,0x30,0x8000000000000000)
            tangent_calls=[0]
            def tangent(_,value):tangent_calls[0]+=1;return tan(value)
            tangent_cb=TAN(tangent)
            for iteration in range(4096):
                for j in range(3):
                    for off,value in zip([0xdc,0x110,0x114,0x118,0x11c],[rng.uniform(.2,1.2),rng.uniform(.5,25),rng.uniform(.5,8),rng.uniform(1,35),rng.uniform(.3,1.5)]):
                        struct.pack_into('<f',records,j*0x160+off,value)
                    struct.pack_into('<i',records,j*0x160+0x13c,j-1)
                # Duplicate ID cases ensure the first matching wheel wins.
                if iteration%11==0:struct.pack_into('<i',records,0x13c,1)
                struct.pack_into('<I',actor,8,8)
                payload=bytearray(rng.randbytes(0x130));wheel_id=rng.choice([0,1] if iteration%11==0 else [-1,0,1]);struct.pack_into('<i',payload,0,wheel_id)
                sx=rng.choice([0.,1e-6,rng.uniform(-1,1)]);angle=rng.choice([0.,-0.5,rng.uniform(-1.2,1.2)])
                load=rng.choice([-1.,0.,1e-5,1.00001e-5,.019,.02,.021,1.,rng.uniform(1,15)])
                for off,v in [(0x10,sx),(0x20,angle),(0x30,load)]:struct.pack_into('<f',payload,off,v)
                frames=[C.create_string_buffer(bytes(payload),len(payload)) for _ in range(2)]
                forces(obj,frames[0]);tangent_calls[0]=0
                backend=C.cast(tangent_cb,P) if iteration%2==0 else None
                if not core.fs15_script_wheel_forces(manager,obj,frames[1],len(payload),None,backend):raise AssertionError('Recovered force adapter rejected')
                actual_load=struct.unpack_from('<f',payload,0x30)[0]
                if tangent_calls[0]!=int(iteration%2==0 and actual_load>struct.unpack('<f',struct.pack('<f',1e-5))[0]):raise AssertionError('Tangent gating differs')
                x,y=[bytearray(f.raw) for f in frames]
                for off in (0xf0,0x100):
                    u,v=struct.unpack_from('<f',x,off)[0],struct.unpack_from('<f',y,off)[0]
                    error=abs(u-v)/max(1.,abs(u));max_error=max(max_error,error)
                    if not math.isfinite(u) or not math.isfinite(v) or error>2e-5:raise AssertionError(f'Force mismatch {iteration}: {u} != {v}')
                    x[off:off+4]=y[off:off+4]=bytes(4)
                if x!=y:raise AssertionError('Frame spares or tags differ')
                digest.update(frames[0].raw);wheel_count+=1
        finally:
            driver_global.value=saved_driver;manager_global.value=saved_manager
    report=dict(status='passed',input_image_sha256=EXPECTED_IMAGE,
        native_entries=['0x14012ca00','0x14012cc40','0x1400e76c0'],
        rebuilt_source_sha256={name:hashlib.sha256((ROOT/name).read_bytes()).hexdigest() for name in
            ['src/core/scene_view_lifecycle.cpp','src/core/scene_view_lifecycle.h','src/core/wheel_forces.cpp','src/core/wheel_forces.h']},
        view_lifetimes=view_count,buffer_failure_masks=4,wheel_force_frames=wheel_count,
        maximum_scaled_force_error=max_error,force_tolerance=2e-5,comparison_digest=digest.hexdigest(),
        excluded_native_branch='Missing-wheel diagnostics require unlinked CRT imports; missing wheel behavior is covered by portable authored fixtures only.',
        tangent_backends={'original_math_boundary':2048,'platform_math':2048},
        scope='Unchanged original view constructor/destructor with synthetic driver/buffer slots, complete records and ordered callback snapshots; unchanged wheel adapter with original lookup/tire math and authored registry fixtures. No actual GPU, world solver or full startup executed.')
    (ROOT/'analysis/startup-view-wheel-validation.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))
if __name__=='__main__':main()
