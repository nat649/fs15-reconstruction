"""Unchanged native physics dispatch/worker bodies against owned backend seams.

No original engine entry point or physics solver is started. Observation
collection uses its native empty-list path; nonempty contact processing is open.
"""
import argparse
from collections import Counter
import ctypes as C
import hashlib
import json
import random
import struct
from inventory import ROOT,PE
from native_gar import NativeGarCipher,DEFAULT_IMAGE,EXPECTED_IMAGE
P=C.c_void_p;N=C.c_size_t;F=C.c_float;U=C.c_uint32
SIM=C.CFUNCTYPE(C.c_int,P,N,F,U,F)
WORLD=C.CFUNCTYPE(N,P,N)
COLLECT=C.CFUNCTYPE(C.c_int,P,P,N)
WAIT=C.CFUNCTYPE(U,P,N,U,C.c_uint8)
SIGNAL=C.CFUNCTYPE(C.c_int,P,N)
class Services(C.Structure):_fields_=[('user',P),('simulate',SIM),('world',WORLD),('collect',COLLECT),('wait',WAIT),('signal',SIGNAL)]

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--dll',required=True);a=ap.parse_args()
    core=C.CDLL(str((ROOT/a.dll).resolve()))
    for name,args in [('step',[P,N,F,P]),('dispatch',[P,N,F,P]),('worker',[P,N,P,P])]:
        fn=getattr(core,'fs15_physics_'+name);fn.argtypes=args;fn.restype=C.c_int
    rng=random.Random(0xf515c3b90);counts=Counter();digest=hashlib.sha256()
    def ptr(v):return C.addressof(v)
    with NativeGarCipher() as native:
        at=lambda off,result,*args:C.WINFUNCTYPE(result,*args)(native.base+off)
        run=at(0xc3c50,None,P,F);dispatch=at(0xc7a60,None,P,F);worker=at(0xc3b90,None,P,P)
        phase=[0];records=[];stops=[];events=[[],[]];waits=[0,0];deltas=[];collections=[0,0]
        def snapshot(name,*args):
            payload=bytearray(records[phase[0]].raw);struct.pack_into('<Q',payload,0x38,0x38)
            events[phase[0]].append((name,*args,bytes(payload)))
        def simulate(scene,seconds,iterations,step):
            snapshot('simulate',scene,seconds,iterations,step)
            p=ptr(records[phase[0]])+0x240;C.c_uint32.from_address(p).value+=1
            return 1
        def world(scene):snapshot('world',scene);return 0x778899
        def collect(_,observations,token):
            if observations!=ptr(records[1])+0x48 or token!=0x778899:raise AssertionError('Collection identity differs')
            collections[1]+=1;return 1
        def wait(token,timeout,alertable):
            snapshot('wait',token,timeout,alertable)
            i=waits[phase[0]];waits[phase[0]]+=1
            if i==len(deltas):stops[phase[0]][0]=b'\x01'
            else:struct.pack_into('<f',records[phase[0]],0x488,deltas[i])
            return 0
        def signal(token):snapshot('signal',token);return 1
        # Original scene -> backend virtual slots are real callbacks; only the
        # native collector's empty observation path follows the world getter.
        native_sim=C.WINFUNCTYPE(None,P,F,U,F)(lambda *args:simulate(*args))
        native_world=C.WINFUNCTYPE(P,P)(lambda _:world(ptr(scene)))
        backend_table=(N*10)();backend_table[9]=C.cast(native_world,P).value;backend=(N*1)(ptr(backend_table))
        scene_table=(N*13)();scene_table[12]=C.cast(native_sim,P).value
        scene=C.create_string_buffer(0x68);struct.pack_into('<Q',scene,0,ptr(scene_table));struct.pack_into('<Q',scene,0x60,ptr(backend))
        native_wait=C.WINFUNCTYPE(U,P,U,U)(wait);native_signal=C.WINFUNCTYPE(C.c_int,P)(signal)
        slots={}
        for library in PE(DEFAULT_IMAGE.read_bytes()).imports():
            if library['dll'].lower()!='kernel32.dll':continue
            for row in library['functions']:
                if row['name'] in ('WaitForSingleObjectEx','SetEvent'):slots[row['name']]=int(row['iat_va'],16)
        if len(slots)!=2:raise AssertionError('Native event IAT changed')
        for name,callback in [('WaitForSingleObjectEx',native_wait),('SetEvent',native_signal)]:
            old=U();address=slots[name]
            if not native.kernel.VirtualProtect(address,8,4,C.byref(old)):raise C.WinError(C.get_last_error())
            C.c_void_p.from_address(address).value=C.cast(callback,P).value
            restored=U();native.kernel.VirtualProtect(address,8,old.value,C.byref(restored))
        callbacks=[SIM(lambda _,*args:simulate(*args)),WORLD(lambda _,scene:world(scene)),COLLECT(collect),WAIT(lambda _,*args:wait(*args)),SIGNAL(lambda _,token:signal(token))]
        services=Services(None,*callbacks)
        def fixture():
            nonlocal records,stops
            payload=bytearray(rng.randbytes(0x4b0))
            struct.pack_into('<Q',payload,0x38,ptr(scene));struct.pack_into('<I',payload,0x54,0)
            struct.pack_into('<fI',payload,0xb0,rng.choice([0.,-0.01,0.01,1/60,1/120]),rng.choice([0,1,2,20,0x80000000,0xffffffff]))
            for off in (0xc0,0xc8,0x488):struct.pack_into('<f',payload,off,rng.uniform(-1000,1000))
            payload[0xa8]=rng.choice([0,1,2]);payload[0x448]=rng.choice([0,1,2]);payload[0x48c]=rng.choice([0,1,2])
            struct.pack_into('<QQ',payload,0x478,31,32)
            records=[C.create_string_buffer(bytes(payload),len(payload)) for _ in range(2)];stops=[C.create_string_buffer(b'\0',1) for _ in range(2)]
            events[:]=[[],[]];waits[:]=[0,0];collections[:]=[0,0]
        def verify(label):
            if records[0].raw!=records[1].raw or events[0]!=events[1]:raise AssertionError(label+' record/callback trace differs')
            payload=bytearray(records[0].raw);struct.pack_into('<Q',payload,0x38,0x38);digest.update(payload)
            counts[label]+=1
        for i in range(3072):
            fixture();delta=rng.choice([-1.,-0.,0.,1e-6,16.,16.666666,1000.,rng.uniform(.001,10000)])
            phase[0]=0;dispatch(records[0],delta);phase[0]=1
            if not core.fs15_physics_dispatch(records[1],0x490,delta,C.byref(services)):raise AssertionError('Dispatch rejected')
            verify('dispatch_states')
            if not events[0]:counts['dispatch_ignored']+=1
            elif events[0][0][0]=='signal':counts['dispatch_async_signals']+=1
            else:counts['dispatch_inline_steps']+=1
        for i in range(1024):
            fixture();delta=rng.uniform(-1,1)
            phase[0]=0;run(records[0],delta);phase[0]=1
            if not core.fs15_physics_step(records[1],0x490,delta,C.byref(services)):raise AssertionError('Inline step rejected')
            verify('inline_states')
        for i in range(512):
            fixture();deltas[:]=[rng.uniform(.001,1) for _ in range(i%9)]
            phase[0]=0;worker(records[0],stops[0]);phase[0]=1
            if not core.fs15_physics_worker(records[1],0x490,stops[1],C.byref(services)):raise AssertionError('Worker rejected')
            if waits[0]!=len(deltas)+1 or waits[1]!=waits[0] or collections[1]!=len(deltas):raise AssertionError('Worker completion counts differ')
            verify('worker_lifetimes');counts['worker_completed_steps']+=len(deltas)
    report={'status':'passed','input_image_sha256':EXPECTED_IMAGE,'native_entries':['0x1400c7a60','0x1400c3c50','0x1400c3b90'],
        'rebuilt_source_sha256':{name:hashlib.sha256((ROOT/name).read_bytes()).hexdigest() for name in ['src/core/physics_dispatch.cpp','src/core/physics_dispatch.h']},
        'comparison_counts':dict(counts),'comparison_digest':digest.hexdigest(),
        'scope':'Unchanged original dispatch, inline and worker bodies; complete manager/guard bytes and ordered backend/event traces. Native observation collection runs only with an empty list. Real OS threading is validated separately through authored transport. Full manager/scene creation, nonempty contacts, post-step finalization and solver remain unported.',
        'authored_guards':'Finite/size/callback checks and fail-stop behavior on service errors; native event return values are ignored in original code.'}
    (ROOT/'analysis/physics-dispatch-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
if __name__=='__main__':main()
