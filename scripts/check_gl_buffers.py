"""Compare nine recovered GL buffer paths against unchanged local GE6 code.

GL entry points are owned recording callbacks, not a real graphics context.
The separate SDL/Mesa test checks actual GL byte transfers on Linux.
"""
import argparse
import ctypes as C
import hashlib
import json
import random
import struct
from inventory import ROOT, write_json
from native_animation_xml import NativeAnimationXml
from native_gar import EXPECTED_IMAGE

P=C.c_void_p;N=C.c_size_t;U=C.c_uint32;I=C.c_int
GEN=C.CFUNCTYPE(None,P,I,P);BIND=C.CFUNCTYPE(None,P,U,U)
DATA=C.CFUNCTYPE(None,P,U,N,N,U);SUB=C.CFUNCTYPE(None,P,U,N,N,N)
MAP=C.CFUNCTYPE(N,P,U,U);UNMAP=C.CFUNCTYPE(None,P,U)
ALLOC=C.CFUNCTYPE(P,P,N);FREE=C.CFUNCTYPE(None,P,P)
class Services(C.Structure):
    _fields_=[('user',P),('gen',GEN),('bind',BIND),('data',DATA),('subdata',SUB),
              ('map',MAP),('unmap',UNMAP),('erase',GEN),('allocate',ALLOC),('release',FREE),
              ('scratch',N),('scratch_bytes',N),('vertex_bytes',U),('index_bytes',U),('scratch_busy',C.c_uint8)]
class Desc(C.Structure):_fields_=[('data',N),('bytes',U),('flags',U),('format',U)]

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--dll',required=True);a=ap.parse_args()
    core=C.CDLL(str((ROOT/a.dll).resolve()))
    signatures={'vertex_create':(P,[P,N,P,P]),'index_construct':(I,[P,P,P]),
                'vertex_destroy':(I,[P,U,P]),'index_destroy':(I,[P,U,P]),
                'vertex_map':(N,[P,U,U,I,P]),'index_map':(N,[P,U,I,P]),
                'vertex_unmap':(I,[P,P]),'index_unmap':(I,[P,P]),'vertex_upload':(I,[P,U,U,C.c_uint8,P])}
    for name,(result,args) in signatures.items():f=getattr(core,'fs15_gl_'+name);f.restype=result;f.argtypes=args
    rng=random.Random(0xf515074ab0);digest=hashlib.sha256();cases=1024;events=[[],[]];phase=[0];owned=[]
    scratch=C.create_string_buffer(0x1000000);mapped=C.create_string_buffer(256)
    def ptr(x):return C.addressof(x)
    def record(*args):events[phase[0]].append(args)
    def gen(count,out):C.c_uint32.from_address(out).value=73;record('gen',count)
    def bind(target,name):record('bind',target,name)
    def data(target,size,source,usage):record('data',target,size,source or 0,usage)
    def sub(target,offset,size,source):record('subdata',target,offset,size,source)
    def map_buffer(target,access):record('map',target,access);return ptr(mapped) if map_success[0] else 0
    def unmap(target):record('unmap',target)
    def erase(count,source):record('erase',count,C.c_uint32.from_address(source).value)
    def allocate(_,size):b=C.create_string_buffer(size);owned.append(b);return ptr(b)
    callbacks=[GEN(lambda _,n,p:gen(n,p)),BIND(lambda _,t,n:bind(t,n)),DATA(lambda _,t,n,p,u:data(t,n,p,u)),SUB(lambda _,t,o,n,p:sub(t,o,n,p)),MAP(lambda _,t,a:map_buffer(t,a)),UNMAP(lambda _,t:unmap(t)),GEN(lambda _,n,p:erase(n,p)),ALLOC(allocate),FREE(lambda _,p:record('free'))]
    s=Services(None,*callbacks,ptr(scratch),len(scratch),0,0,0);map_success=[True]
    with NativeAnimationXml() as native:
        nf=lambda off,r,*args:C.WINFUNCTYPE(r,*args)(native.base+off)
        funcs=[nf(0x65c60,P,P,P),nf(0x74850,P,P,P),nf(0x74920,P,P,U),nf(0x74990,N,P,U,U,I),nf(0x74a10,None,P),nf(0x74a40,P,P,U),nf(0x74ab0,N,P,U,U,I),nf(0x74b70,None,P),nf(0x74bd0,None,P,U,U,C.c_uint8)]
        ng=C.WINFUNCTYPE(None,I,P)(gen);nb=C.WINFUNCTYPE(None,U,U)(bind);nd=C.WINFUNCTYPE(None,U,N,N,U)(data)
        ns=C.WINFUNCTYPE(None,U,N,N,N)(sub);nm=C.WINFUNCTYPE(N,U,U)(map_buffer)
        nu=C.WINFUNCTYPE(C.c_uint8,U)(lambda t:(unmap(t),1)[1]);ne=C.WINFUNCTYPE(None,I,P)(erase)
        for off,cb in [(0x162b3e8,ng),(0x162b3c8,nb),(0x162b3d0,nd),(0x162b3d8,ns),(0x162b410,nm),(0x162b418,nu),(0x162b3e0,ne)]:C.c_void_p.from_address(native.base+off).value=C.cast(cb,P).value
        C.c_void_p.from_address(native.base+0x162b818).value=ptr(scratch)
        vc=U.from_address(native.base+0x162b244);ic=U.from_address(native.base+0x162b248);busy=C.c_uint8.from_address(native.base+0x162b24c)
        def compare(x,y,size):
            if C.string_at(x,size)!=C.string_at(y,size) or events[0]!=events[1] or (vc.value,ic.value,busy.value)!=(s.vertex_bytes,s.index_bytes,s.scratch_busy):
                raise AssertionError(f'GE6 buffer state/event mismatch case {case}: {events}')
            digest.update(C.string_at(x,size));digest.update(json.dumps(events[0]).replace(str(ptr(scratch)),'scratch').replace(str(ptr(mapped)),'mapped').encode());events[:]=[[],[]]
        for case in range(cases):
            owned.clear();events[:]=[[],[]];vc.value=ic.value=s.vertex_bytes=s.index_bytes=rng.getrandbits(32);busy.value=s.scratch_busy=0
            driver=C.create_string_buffer(0x190);d=Desc(0 if case%2 else ptr(mapped),rng.choice([0,4,64,256]),rng.getrandbits(32),case%3)
            phase[0]=0;x=funcs[0](driver,C.byref(d));phase[0]=1;y=core.fs15_gl_vertex_create(driver,len(driver),C.byref(d),C.byref(s))
            if not x or not y:raise AssertionError('Vertex allocation')
            # Spare allocator bytes are unspecified; compare only constructor writes.
            for off,n in [(0,22),(36,4)]:
                if C.string_at(x+off,n)!=C.string_at(y+off,n):raise AssertionError('Vertex constructor fields')
            if events[0]!=events[1] or vc.value!=s.vertex_bytes:raise AssertionError('Vertex creation GL/counter')
            events[:]=[[],[]]
            payload=rng.randbytes(40);C.memmove(x,payload,40);C.memmove(y,payload,40)
            # Make the buffer a valid, idle native record while preserving opaque bytes.
            for z in (x,y):struct.pack_into('<QIII', (C.c_char*40).from_address(z),0,0x1404bd480,73,0x88e8,d.flags);C.memset(z+20,0,2);U.from_address(z+36).value=d.bytes
            mode=rng.choice([-1,0,1,2,3]);offset=rng.choice([0,4,128,0xffffffff]);length=rng.choice([0,4,64,0x1000000,0x1000001]);busy.value=s.scratch_busy=case%2;map_success[0]=case%7!=0
            phase[0]=0;p=funcs[6](x,offset,length,mode);phase[0]=1;q=core.fs15_gl_vertex_map(y,offset,length,mode,C.byref(s))
            if p!=q:raise AssertionError('Vertex mapped pointer')
            compare(x,y,40)
            phase[0]=0;funcs[7](x);phase[0]=1
            if not core.fs15_gl_vertex_unmap(y,C.byref(s)):raise AssertionError('Vertex unmap rejected')
            compare(x,y,40)
            phase[0]=0;funcs[8](x,12,64,case%2);phase[0]=1;core.fs15_gl_vertex_upload(y,12,64,case%2,C.byref(s));compare(x,y,40)
            phase[0]=0;funcs[5](x,0);phase[0]=1;core.fs15_gl_vertex_destroy(y,0,C.byref(s));compare(x,y,40)
            native.callbacks[1](native.heap,0,x)
            payload=rng.randbytes(32);ix=C.create_string_buffer(payload,32);iy=C.create_string_buffer(payload,32)
            phase[0]=0;funcs[1](ix,C.byref(d));phase[0]=1;core.fs15_gl_index_construct(iy,C.byref(d),C.byref(s));compare(ptr(ix),ptr(iy),32)
            phase[0]=0;p=funcs[3](ix,offset,777,mode);phase[0]=1;q=core.fs15_gl_index_map(iy,offset,mode,C.byref(s))
            if p!=q:raise AssertionError('Index mapped pointer')
            compare(ptr(ix),ptr(iy),32)
            phase[0]=0;funcs[4](ix);phase[0]=1;core.fs15_gl_index_unmap(iy,C.byref(s));compare(ptr(ix),ptr(iy),32)
            phase[0]=0;funcs[2](ix,0);phase[0]=1;core.fs15_gl_index_destroy(iy,0,C.byref(s));compare(ptr(ix),ptr(iy),32)
    report=dict(status='passed',original_image_sha256=EXPECTED_IMAGE,lifetimes=cases,compared_native_entries=9,
        comparison_digest=digest.hexdigest(),rebuilt_source_sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in ['src/core/gl_buffers.cpp','src/core/gl_buffers.h']},
        scope='Nine bounded OpenGL vertex/index buffer paths. Unchanged original machine code with owned recording GL callbacks and isolated CRT heap. Includes staging size boundary, occupied scratch, read/write/discard, null map plus offset, usage flags, counter wrap and nondeleting lifetime. Constructor allocator spare bytes excluded. Native CRT allocation failure/throws, deleting free flag, live driver, shaders, materials, draw submission and engine startup are separate; no full renderer claim.')
    write_json(ROOT/'analysis/gl-buffers-validation.json',report);print(json.dumps(report,indent=2))
if __name__=='__main__':main()
