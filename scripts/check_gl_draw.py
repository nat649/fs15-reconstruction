"""Compare original GE6 OpenGL draw dispatch with recording driver transport."""
import argparse
import ctypes as C
import hashlib
import json
import random
import struct
from inventory import ROOT,write_json
from native_gar import EXPECTED_IMAGE
from native_physics_world import NativePhysicsWorld,P,F,N,U
def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--dll',required=True);args=ap.parse_args();lib=C.CDLL(str((ROOT/args.dll).resolve()))
    lib.fs15_ge6_gl_draw.argtypes=[P,C.c_int,C.c_int,U,U,U,P];lib.fs15_ge6_gl_draw.restype=C.c_int
    events=[[],[]];phase=[0];rng=random.Random(0x6d2a);digest=hashlib.sha256();calls=0;cases=4096
    def emit(kind,*args):events[phase[0]].append((kind,*args))
    native_callbacks=[C.WINFUNCTYPE(None,F)(lambda v:emit(1,v)),C.WINFUNCTYPE(None,U,C.c_int,C.c_int)(lambda m,f,n:emit(2,m,f,n)),C.WINFUNCTYPE(None,U,C.c_int,U,N)(lambda m,n,t,o:emit(3,m,n,t,o)),C.WINFUNCTYPE(None,U,U,U,C.c_int,U,N)(lambda m,a,b,n,t,o:emit(4,m,a,b,n,t,o))]
    callback_types=[C.CFUNCTYPE(None,P,F),C.CFUNCTYPE(None,P,U,C.c_int,C.c_int),C.CFUNCTYPE(None,P,U,C.c_int,U,N),C.CFUNCTYPE(None,P,U,U,U,C.c_int,U,N)]
    port_callbacks=[callback_types[0](lambda _,v:emit(1,v)),callback_types[1](lambda _,m,f,n:emit(2,m,f,n)),callback_types[2](lambda _,m,n,t,o:emit(3,m,n,t,o)),callback_types[3](lambda _,m,a,b,n,t,o:emit(4,m,a,b,n,t,o))]
    service=(N*5)(0,*[C.cast(c,P).value for c in port_callbacks])
    with NativePhysicsWorld() as native:
        for off,callback in zip([0x4726f0,0x4726e0,0x4726e8,0x162b258],native_callbacks):native._slot(native.base+off,C.cast(callback,P).value)
        for case in range(cases):
            state=C.create_string_buffer(bytes([0xa5])*0x1d0,0x1d0);index=C.create_string_buffer(0x20)
            struct.pack_into('<I',index,8,rng.choice([0x1403,0x1405,0x1401]));struct.pack_into('<Q',state,0x160,C.addressof(index) if case%3 else 0);struct.pack_into('<Q',state,0x188,int(case%5!=0));struct.pack_into('<I',state,0x1cc,rng.choice([0,0x40a00000,0x7fc00000]))
            rebuilt=C.create_string_buffer(bytes(state),len(state));count=rng.randint(-10,1000);first=rng.choice([-2147483648,-1,0,1,37,2147483647]);primitive=case%5;minimum=rng.getrandbits(32);range_count=rng.choice([0,0,1,2,100,rng.getrandbits(32)])
            events[0].clear();events[1].clear();phase[0]=0;native.fn(0x65540,None,P,C.c_int,C.c_int,C.c_int,U,U)(state,count,first,primitive,minimum,range_count)
            phase[0]=1
            if not lib.fs15_ge6_gl_draw(rebuilt,count,first,primitive,minimum,range_count,service):raise AssertionError('Valid draw rejected')
            if bytes(state)!=bytes(rebuilt) or events[0]!=events[1]:raise AssertionError((case,events))
            calls+=len(events[0]);digest.update(json.dumps(events[0]).encode());canonical=bytearray(state);canonical[0x160:0x168]=bytes(8);digest.update(canonical)
    report=dict(status='passed',original_image_sha256=EXPECTED_IMAGE,driver_states=cases,ordered_gl_calls=calls,comparison_digest=digest.hexdigest(),
        rebuilt_source_sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in ['src/core/gl_draw.h','src/core/gl_draw.cpp']},
        scope='Native 0x140065540 array/index/range draw dispatch, point-size cache and 32-bit byte-offset/range wrapping. Ordered recording driver calls and complete state compared; original GL imports rebound only in local isolated image. Bound layout, context, program, vertex attributes and scene submission remain caller services, not a recovered complete renderer.')
    write_json(ROOT/'analysis/gl-draw-validation.json',report);print(json.dumps(report,indent=2))
if __name__=='__main__':main()
