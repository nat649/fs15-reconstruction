"""Differential Camera view/stereo preparation and 196-byte shader-block upload.

Original math/view bodies execute unchanged. Synthetic map/unmap virtual slots
provide owned CPU storage, not a recovered GPU implementation. A separately
labelled tangent callback isolates view logic from platform CRT differences.
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
from check_scene_camera import local_cases
from check_scene_spatial import Grid,Cell,Entry

F=C.c_float;P=C.c_void_p;N=C.c_size_t
WORLD=C.CFUNCTYPE(C.c_int,P,P,P);MAP=C.CFUNCTYPE(P,P,N,C.POINTER(N));UNMAP=C.CFUNCTYPE(C.c_int,P,N);TAN=C.CFUNCTYPE(F,P,F)
class Services(C.Structure):_fields_=[('user',P),('world',WORLD),('map',MAP),('unmap',UNMAP),('tangent',TAN)]
def put(p,o,v,fmt='Q'):C.memmove(p+o,struct.pack('<'+fmt,v),struct.calcsize('<'+fmt))
def read(p,o,fmt='Q'):return struct.unpack('<'+fmt,C.string_at(p+o,struct.calcsize('<'+fmt)))[0]
def matrix(rng):
    a=[0.0]*16
    for i in range(3):a[i*5]=rng.uniform(0.5,3)
    a[1]=rng.uniform(-0.2,0.2);a[4]=rng.uniform(-0.2,0.2);a[6]=rng.uniform(-0.2,0.2);a[9]=rng.uniform(-0.2,0.2)
    a[12:15]=[rng.uniform(-100,100) for _ in range(3)];a[15]=1;return a

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--dll',type=Path,required=True);args=parser.parse_args()
    core=C.CDLL(str(args.dll.resolve()));rng=random.Random(0xf51512cca0);counts=Counter();digest=hashlib.sha256();max_error=0.0
    for name,types in {'fs15_scene_view_upload':[P,N,C.c_double,P],'fs15_scene_view_prepare':[P,N,P,F,C.c_uint8,C.c_uint8,C.c_double,P],
        'fs15_scene_update_world':[P,N,N,N],'fs15_scene_spatial_volume_query':[P,P,P,F,P,P,C.c_uint32,P]}.items():getattr(core,name).argtypes=types;getattr(core,name).restype=C.c_int
    with NativeGarCipher(math_helpers=True) as native:
        nf=lambda at,result,*types:C.WINFUNCTYPE(result,*types)(native.base+at)
        nf(0x524b0,P)();upload=nf(0x12cca0,None,P);prepare=nf(0xb3e20,None,P,P,P,C.c_uint8);native_tan=nf(0x4566ec,F,F)
        query=nf(0xd7250,C.c_uint32,P,P,P,F,P,P,C.c_uint32)
        if C.string_at(native.base+0x4db650,8)!=struct.pack('<d',0.001):raise AssertionError('Time scale changed')
        def fixture():
            payload=rng.randbytes(0x208);views=[C.create_string_buffer(payload,len(payload)) for _ in range(2)];vp=[C.addressof(v) for v in views]
            settings=C.create_string_buffer(bytes([0xa5])*0x68,0x68);sp=C.addressof(settings)
            for off in [*range(0x20,0x2c,4),0x30,0x34,*range(0x38,0x44,4),*range(0x48,0x68,4)]:put(sp,off,rng.uniform(-100,100),'f')
            gpu=[C.create_string_buffer(bytes([0x5a])*256,256) for _ in range(2)];objects=[(N*1)(),(N*1)()];traces=[[],[]];errors=[];camera_maps=[{},{}];arenas=[None,None]
            def canonical(data,side):
                data=bytearray(data)
                for off,token in [(0x1a8,vp[side]),(0x1d0,sp),(0x1d8,C.addressof(objects[side]))]:
                    if struct.unpack_from('<Q',data,off)[0]==token:struct.pack_into('<Q',data,off,off)
                at=struct.unpack_from('<Q',data,0x1a0)[0];struct.pack_into('<Q',data,0x1a0,camera_maps[side].get(at,at));return bytes(data)
            def event(side,name,token):
                if token!=C.addressof(objects[side]):errors.append('Buffer identity changed')
                traces[side].append((name,canonical(views[side].raw,side),gpu[side].raw))
            nmap=C.WINFUNCTYPE(P,P)(lambda token:(event(0,'map',token),C.addressof(gpu[0]))[1])
            nunmap=C.WINFUNCTYPE(None,P)(lambda token:event(0,'unmap',token))
            table=(N*3)(0,C.cast(nmap,P).value,C.cast(nunmap,P).value)
            objects[0][0]=objects[1][0]=C.addressof(table)
            def world(_,camera,out):
                arena=arenas[1];base=C.addressof(arena);index=(camera-base)//0x190
                if not core.fs15_scene_update_world(base,len(arena),0x190,index):return 0
                C.memmove(out,camera+0xd8,64);return 1
            def pmap(_,token,capacity):event(1,'map',token);capacity[0]=64;return C.addressof(gpu[1])
            callbacks=[WORLD(world),MAP(pmap),UNMAP(lambda _,token:(event(1,'unmap',token),1)[1]),TAN(lambda _,angle:native_tan(angle))]
            services=Services(None,*callbacks)
            for v,token in zip(vp,objects):
                put(v,0,0x1404c7cc0);put(v,0x1a0,0x11223344);put(v,0x1a8,0x55667788);put(v,0x1d0,sp);put(v,0x1d8,C.addressof(token))
            # Keep callbacks/objects alive for the full original tail-call.
            return dict(views=views,vp=vp,settings=settings,settings_before=settings.raw,gpu=gpu,objects=objects,traces=traces,errors=errors,camera_maps=camera_maps,arenas=arenas,
                native_callbacks=[nmap,nunmap,table],callbacks=callbacks,services=services,canonical=canonical)
        def verify(f,label,tolerance=False):
            nonlocal max_error
            if f['settings'].raw!=f['settings_before']:raise AssertionError('Read-only render settings changed')
            a,b=[f['canonical'](f['views'][i].raw,i) for i in range(2)]
            # Platform tanf may round differently. Compare only matrix/plane
            # floats with tolerance; every other record byte remains exact.
            def compare(x,y,offsets):
                nonlocal max_error
                x,y=bytearray(x),bytearray(y)
                for off in offsets:
                    u,v=struct.unpack_from('<f',x,off)[0],struct.unpack_from('<f',y,off)[0]
                    if not math.isfinite(u) or not math.isfinite(v):raise AssertionError('Nonfinite fixture output')
                    error=abs(u-v)/max(1.0,abs(u));max_error=max(max_error,error)
                    if error>1e-5:raise AssertionError(f'Platform tangent output differs at {off:x}: {u} != {v}')
                    x[off:off+4]=y[off:off+4]=bytes(4)
                if x!=y:
                    at=next(i for i,(u,v) in enumerate(zip(x,y)) if u!=v);raise AssertionError(f'{label} differs at +{at:x}')
            offsets=list(range(8,0x68,4))+list(range(0xe8,0x168,4)) if tolerance else []
            compare(a,b,offsets);compare(f['gpu'][0].raw,f['gpu'][1].raw,list(range(48,112,4)) if tolerance else [])
            if f['errors'] or [e[0] for e in f['traces'][0]]!=['map','unmap'] or [e[0] for e in f['traces'][1]]!=['map','unmap']:raise AssertionError('Map/unmap transport/order differs')
            for x,y in zip(*f['traces']):
                compare(x[1],y[1],offsets);compare(x[2],y[2],list(range(48,112,4)) if tolerance else [])
                if struct.unpack_from('<Q',x[1],0x1a0)[0]!=0x11223344:raise AssertionError('Camera identity changed before unmap')
            digest.update(a);digest.update(f['gpu'][0].raw);counts[label]+=1;counts['complete_map_unmap_trace_pairs']+=1
        for case in range(3000):
            f=fixture();world=matrix(rng);projection=[rng.uniform(-3,3) for _ in range(16)]
            if case%100==0:world[:12]=[0.0]*12
            for p in f['vp']:C.memmove(p+0x68,struct.pack('<16f',*world),64);C.memmove(p+0xe8,struct.pack('<16f',*projection),64)
            milliseconds=[-0.0,0.0,1.0,1000.0,2**40+0.1][case%5] if case%3==0 else rng.uniform(-1e9,1e9)
            put(native.base+0x162b9d8,0,milliseconds,'d');upload(f['vp'][0])
            if not core.fs15_scene_view_upload(f['vp'][1],0x1e8,milliseconds,C.byref(f['services'])):raise AssertionError('Upload rejected')
            verify(f,'exact_shader_upload_records_and_spares')
        local,assets=local_cases();cases=[]
        for case in range(800):
            cases.append(dict(fov=rng.uniform(5,165),nearClip=rng.uniform(0.05,5),farClip=rng.uniform(500,10000),orthographicHeight=rng.uniform(1,100),orthographic='true' if case%2 else 'false'))
        cases.extend(dict(attrs) for attrs in local)
        for case,params in enumerate(cases):
            for mode in range(3):
                f=fixture();depth=case%3+1;payload=bytes([0xa5])*(0x190*depth);f['arenas'][:]=[C.create_string_buffer(payload,len(payload)) for _ in range(2)]
                for i in range(depth):
                    local_matrix=matrix(rng);cached=matrix(rng)
                    if case<4 and mode==0 and i==depth-1:local_matrix[:12]=[0]*12;cached[:12]=[0]*12
                    for side,arena in enumerate(f['arenas']):
                        p=C.addressof(arena)+i*0x190;put(p,0,0x1404c1078);put(p,0x28,C.addressof(arena)+(i-1)*0x190 if i else 0);put(p,0x50,0x40 if case%4 else 0,'I')
                        C.memmove(p+0x98,struct.pack('<16f',*local_matrix),64);C.memmove(p+0xd8,struct.pack('<16f',*cached),64);f['camera_maps'][side][p]=0x1000+i
                near=float(params.get('nearClip',0.1));far=float(params.get('farClip',10000));height=float(params.get('orthographicHeight',1));fov=float(params.get('fov',60));ortho=params.get('orthographic','false').lower()=='true'
                for side,arena in enumerate(f['arenas']):
                    p=C.addressof(arena)+(depth-1)*0x190
                    for off,value in [(0x170,fov),(0x174,near),(0x178,far),(0x17c,height),(0x180,0.2 if case%3 else -0.1),(0x184,near*0.5 if case%4 else 10.0)]:put(p,off,value,'f')
                    put(p,0x188,int(ortho),'B')
                aspect=[1.0,4/3,16/9][case%3];stereo=mode!=0;eye=mode==2;milliseconds=case*123.125
                context=C.create_string_buffer(0x2a8);put(C.addressof(context),0x2a4,int(stereo),'B');input_record=C.create_string_buffer(0x30);camera=C.addressof(f['arenas'][0])+(depth-1)*0x190
                put(C.addressof(input_record),0,camera);put(C.addressof(input_record),0x28,aspect,'f');put(native.base+0x162b9d8,0,milliseconds,'d')
                for p in f['vp']:
                    ident=[0.0]*16
                    for j in range(4):ident[j*5]=1
                    C.memmove(p+0xa8,struct.pack('<16f',*ident),64)
                prepare(context,input_record,f['vp'][0],eye)
                if f['settings'].raw!=f['settings_before']:raise AssertionError('Original mutated render settings')
                candidate=C.addressof(f['arenas'][1])+(depth-1)*0x190
                # Two passes: explicit original tangent transport proves exact
                # view assembly; a separate platform backend run measures it.
                services=f['services'];platform=case%5==0 and not ortho
                if platform:services.tangent=TAN()
                if not core.fs15_scene_view_prepare(f['vp'][1],0x1e8,candidate,aspect,stereo,eye,milliseconds,C.byref(services)):raise AssertionError(f'View prepare rejected {case}/{mode}')
                for side,arena in enumerate(f['arenas']):
                    data=bytearray(arena.raw)
                    for i in range(depth):
                        parent=struct.unpack_from('<Q',data,i*0x190+0x28)[0]
                        if parent:struct.pack_into('<Q',data,i*0x190+0x28,(parent-C.addressof(arena))//0x190+1)
                    if side==0:reference=data
                    elif data!=reference:raise AssertionError('Complete refreshed Camera/parent arena differs')
                counts['complete_camera_parent_cache_states']+=1
                verify(f,'platform_tangent_view_chains' if platform else 'exact_view_stereo_chains',platform)
                # The self-volume emitted by both upload paths is consumed by
                # the actual native/recovered ordered culling query. Owned
                # single-cell rows are fixtures; no render callback is faked.
                keys=(C.c_uint32*12)();entries=(Entry*12)()
                world=struct.unpack('<16f',C.string_at(f['vp'][0]+0x68,64));viewer=(F*3)(*world[12:15])
                for j,e in enumerate(entries):
                    e.token=C.addressof(keys)+j*4;e.center[:]=[world[12+i]-world[8+i]*(j+1)*2+world[i]*(j%3-1) for i in range(3)];e.radius=0.5;e.clip[:]=[100000,0];e.lod_token=0xffffffff
                cell=Cell(C.addressof(entries),12,12,0,0,10000,10000,1);grid=Grid(C.addressof(cell),1,1,1,-10000,10000,-10000,10000,-10000,10000,0.00005,0.00005,0)
                before=(bytes(grid),bytes(cell),bytes(entries),bytes(keys));out=[(N*16)(*[0xabcd]*16),(N*16)(*[0xabcd]*16)];capacity=case%13;total=C.c_uint32()
                expected=query(C.byref(grid),read(f['vp'][0],0x1a8),viewer,1,None,out[0],capacity)
                if not core.fs15_scene_spatial_volume_query(C.byref(grid),read(f['vp'][1],0x1a8),viewer,1,None,out[1],capacity,C.byref(total)) or total.value!=expected or bytes(out[0])!=bytes(out[1]):raise AssertionError('Prepared Camera volume culling differs')
                if before!=(bytes(grid),bytes(cell),bytes(entries),bytes(keys)):raise AssertionError('Camera culling changed source grid')
                counts['prepared_view_volume_queries']+=1;counts['prepared_view_query_matches']+=expected
                if expected>capacity:counts['truncated_prepared_view_queries']+=1
                digest.update(struct.pack('<I',expected));digest.update(bytes((out[0][j]-C.addressof(keys))//4 for j in range(min(expected,capacity))))
                counts['stereo_view_chains' if stereo else 'mono_view_chains']+=1
                if case<4 and mode==0:counts['singular_inverse_retained_chains']+=1
        paths=[p for p in (ROOT/'src/core').glob('*') if p.suffix in ('.cpp','.h')]
        report=dict(status='passed',reference_sha256=EXPECTED_IMAGE,native_entries=['0x14012cca0','0x1400b3e20'],
            rebuilt_source_sha256={p.relative_to(ROOT).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(paths)},tested_dll_sha256=hashlib.sha256(args.dll.read_bytes()).hexdigest(),
            comparisons=dict(counts),maximum_scaled_platform_error=max_error,platform_tolerance='1e-5 * max(1,abs(original)) on projection/combined/frustum floats; other bytes exact',original_results_sha256=digest.hexdigest(),local_assets=assets,
            scope='Native Camera view/basis/projection/stereo/inverse/product/frustum assembly and 49-float shader-block packing, self-volume/Camera-identity order, complete records/spares and map/unmap traces; prepared self-volumes consumed by unchanged native/recovered ordered grid queries. Authored world/owned-buffer callback transport replaces platform context and GPU virtual calls. Exact labelled chains reuse original tangent as a math boundary; separate platform-tanf chains use tolerance. Render-view construction, real GPU mapping/upload/driver, render consumers, native startup and Switch execution remain open.')
        write_json(ROOT/'analysis/scene-view-validation.json',report);print(json.dumps(report,indent=2))

if __name__=='__main__':main()
