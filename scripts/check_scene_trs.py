"""Check recovered GE6 TRS math and full scene/Lua update paths.

Platform libm replaces the original CRT trig implementation. Float comparisons
use a recorded tolerance; flags, unknown bytes, frame tags and events stay exact.
The isolated native logger has zero listeners. No game/renderer is started.
"""
import argparse
from collections import Counter
import ctypes as C
import hashlib
import json
import math
from pathlib import Path
import random
import struct
from inventory import ROOT,write_json
from native_gar import NativeGarCipher,EXPECTED_IMAGE
from check_scene_state import Arena,Notification,STRIDE,NONE,fixture,canonical

ENTRIES=[0x9de00,0x9d8c0,0x57980,0x9e520,0x10fd00,0xe51f0,0xe60c0,0xe6610,0xe5260,0xe6130,0xe6750]
TOLERANCE=8e-6
P=C.c_void_p
N=C.c_size_t


def floats(values):return (C.c_float*len(values))(*values)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dll',type=Path,required=True)
    args=parser.parse_args()
    core=C.CDLL(str(args.dll.resolve()))
    signatures={'fs15_rotate_affine_axis':[P,N,C.c_float,C.c_uint,P,N],
        'fs15_compose_trs':[P,P,P,P,N],'fs15_matrix_euler':[P,N,P,P,N],
        'fs15_scene_trs_update':[C.POINTER(Arena),N,C.c_uint,P,N,P,N,C.POINTER(N),C.POINTER(C.c_int)]}
    for name,signature in signatures.items():
        getattr(core,name).argtypes=signature;getattr(core,name).restype=C.c_int
    rng=random.Random(0xf5157a55)
    counts=Counter();errors={};digest=hashlib.sha256();trace=[];active_base=0

    def compare(kind,expected,actual):
        if len(expected)!=len(actual):raise AssertionError('Comparison dimensions differ')
        maximum=errors.get(kind,0)
        for a,b in zip(expected,actual):
            if not math.isfinite(a) or not math.isfinite(b):raise AssertionError('Nonfinite comparison result')
            error=abs(a-b)/max(1,abs(a));maximum=max(maximum,error)
            if error>TOLERANCE:raise AssertionError(f'{kind} differs: {a} vs {b}, scaled error {error}')
        errors[kind]=maximum;counts[kind]+=1
        if struct.pack('<'+'f'*len(expected),*expected)==struct.pack('<'+'f'*len(actual),*actual):counts[kind+'_bit_exact']+=1

    def compare_records(original,rebuilt,count):
        a,b=bytearray(canonical(original,count)),bytearray(canonical(rebuilt,count))
        for i in range(count):
            for offset,length in ((0x98,16),(0xd8,16),(0x148,9)):
                start=i*STRIDE+offset
                compare('node_float_regions',struct.unpack_from('<'+'f'*length,a,start),struct.unpack_from('<'+'f'*length,b,start))
                a[start:start+length*4]=bytes(length*4);b[start:start+length*4]=bytes(length*4)
        if a!=b:raise AssertionError('Non-float native record bytes differ')
        digest.update(canonical(original,count))

    @C.WINFUNCTYPE(None,P,P,C.c_int32)
    def observe(observer,node,origin):trace.append(((node-active_base)//STRIDE,observer,int(origin)))

    with NativeGarCipher(math_helpers=True) as native:
        rotate=C.WINFUNCTYPE(P,P,C.c_float,C.c_int)(native.base+0x9de00)
        compose=C.WINFUNCTYPE(P,P,P,P,P)(native.base+0x9d8c0)
        euler=C.WINFUNCTYPE(None,P,P,P)(native.base+0x57980)
        methods=[C.WINFUNCTYPE(None,P)(native.base+entry) for entry in (0x9e520,0x10fd00)]
        methods += [C.WINFUNCTYPE(None,P,P)(native.base+entry) for entry in ENTRIES[5:]]
        singleton=C.c_void_p.from_address(native.base+0x162cd18);saved_root=singleton.value
        logger_state=C.c_uint32.from_address(native.base+0x1630dc8)
        logger_count=C.c_uint32.from_address(native.base+0x1630d90)
        saved_logger=(logger_state.value,logger_count.value)
        logger_state.value|=1;logger_count.value=0
        try:
            for index in range(6000):
                rotation=floats([rng.uniform(-20*math.pi,20*math.pi) for _ in range(3)])
                scale=floats([rng.uniform(0.2,5) for _ in range(3)])
                position=floats([rng.uniform(-2048,2048) for _ in range(3)])
                if index<12:
                    rotation=floats([0,(-1 if index%2 else 1)*math.pi/2,0])
                    scale=floats([0 if index%3==0 else 1,1,1])
                expected,actual=floats([0]*16),floats([0]*16)
                compose(expected,rotation,scale,position)
                if not core.fs15_compose_trs(rotation,scale,position,actual,16):raise AssertionError('Composition rejected')
                compare('compose_matrices',list(expected),list(actual));digest.update(bytes(expected))
                m=floats([rng.uniform(-20,20) for _ in range(16)])
                angle=C.c_float(rng.uniform(-20*math.pi,20*math.pi));axis=index%3
                original=floats(list(m));rotate(original,angle,axis)
                if not core.fs15_rotate_affine_axis(m,16,angle,axis,actual,16):raise AssertionError('Rotation rejected')
                compare('axis_matrices',list(original),list(actual));digest.update(bytes(original))
                expected_angles,actual_angles=floats([0]*3),floats([0]*3)
                supplied_scale=scale if index%2 else None
                euler(expected,expected_angles,supplied_scale)
                if not core.fs15_matrix_euler(expected,16,supplied_scale,actual_angles,3):raise AssertionError('Euler extraction rejected')
                compare('euler_vectors',list(expected_angles),list(actual_angles));digest.update(bytes(expected_angles))

            for index in range(900):
                for operation in range(8):
                    copies,view,selected,observer_ids,retained=fixture(rng,index,0,native,observe)
                    original,rebuilt=copies;count=len(original)//STRIDE
                    for i in range(count):
                        r=floats([rng.uniform(-1.3,1.3) for _ in range(3)])
                        s=floats([rng.uniform(0.5,2) for _ in range(3)])
                        t=floats([rng.uniform(-100,100) for _ in range(3)])
                        matrix=floats([0]*16);compose(matrix,r,s,t)
                        for copy in copies:
                            C.memmove(C.addressof(copy)+i*STRIDE+0x98,matrix,64)
                            C.memmove(C.addressof(copy)+i*STRIDE+0x148,t,12)
                            C.memmove(C.addressof(copy)+i*STRIDE+0x154,r,12)
                            C.memmove(C.addressof(copy)+i*STRIDE+0x160,s,12)
                    active_base=C.addressof(original)
                    singleton.value=active_base+view.special_root*STRIDE if view.special_root!=NONE else 0
                    payload=bytearray(rng.randbytes(0x120))
                    for i in range(3):struct.pack_into('<f',payload,i*16,rng.uniform(-3,3) if operation in (3,4) else rng.uniform(-1000,1000))
                    a,b=[C.create_string_buffer(bytes(payload),len(payload)) for _ in range(2)]
                    trace.clear();node=active_base+selected*STRIDE
                    if operation<2:methods[operation](node)
                    else:methods[operation](node,a)
                    events=(Notification*(len(observer_ids)+1))();used=N();warning=C.c_int()
                    if not core.fs15_scene_trs_update(C.byref(view),selected,operation,b,len(b),events,len(events),C.byref(used),C.byref(warning)):
                        raise AssertionError(f'Valid TRS fixture rejected {index}/{operation}')
                    if trace!=[(e.node,e.observer,e.origin) for e in events[:used.value]]:raise AssertionError('TRS event order/origin differs')
                    if any(origin!=0 for _,_,origin in trace):raise AssertionError('Native rebuild origin is not zero')
                    counts['notification_traces']+=1;counts['notifications']+=len(trace)
                    digest.update(json.dumps([(n,observer_ids[o],v) for n,o,v in trace]).encode())
                    if operation>=5:
                        af,bf=bytearray(a.raw),bytearray(b.raw)
                        compare('getter_vectors',[struct.unpack_from('<f',af,0xf0+i*16)[0] for i in range(3)],
                            [struct.unpack_from('<f',bf,0xf0+i*16)[0] for i in range(3)])
                        for i in range(3):af[0xf0+i*16:0xf4+i*16]=bytes(4);bf[0xf0+i*16:0xf4+i*16]=bytes(4)
                        if af!=bf:raise AssertionError('Getter frame tags/unrelated bytes differ')
                    elif a.raw!=b.raw:raise AssertionError('Setter/rebuild modified script input frame')
                    compare_records(original,rebuilt,count)
                    counts['scene_arenas']+=1;counts['scene_operation_'+str(operation)]+=1
                    counts['scale_diagnostics']+=warning.value
        finally:
            singleton.value=saved_root;logger_state.value,logger_count.value=saved_logger
    paths=('src/core/scene_trs.cpp','src/core/scene_trs.h','src/core/scene_state.cpp','src/core/scene_state.h',
           'src/core/scene_math.cpp','src/core/scene_math.h','src/core/scene_graph.cpp','src/core/scene_graph.h')
    report=dict(status='passed',reference_sha256=EXPECTED_IMAGE,native_entries=['0x'+format(0x140000000+entry,'x') for entry in ENTRIES],
        rebuilt_source_sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in paths},
        rebuilt_dll_sha256=hashlib.sha256(args.dll.read_bytes()).hexdigest(),comparisons=dict(counts),
        max_scaled_float_errors=errors,float_tolerance=TOLERANCE,original_results_sha256=digest.hexdigest(),
        native_logger_listeners=0,math_backend='Platform C++ sin/cos/atan2/sqrt; original CRT math bodies are not reconstructed',
        scope='Finite TRS matrices, lazy caches, setters/getters, full node records and script frames, side-effect-free notifications. '
              'Numeric fields use the explicit tolerance; other fields and event order/origin match exactly. '
              'No arbitrary derived callbacks, engine object allocation, full engine startup or console execution.')
    write_json(ROOT/'analysis/scene-trs-validation.json',report)
    print(json.dumps(report,indent=2))


if __name__=='__main__':main()
