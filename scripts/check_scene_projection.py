"""Compare recovered projective matrices and Camera basis conditioning with GE6."""
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

F=C.c_float;P=C.c_void_p;N=C.c_size_t
class Frustum(C.Structure):_fields_=[('table',N),('planes',(F*4)*6)]

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--dll',type=Path,required=True);args=parser.parse_args()
    core=C.CDLL(str(args.dll.resolve()));counts=Counter();digest=hashlib.sha256();rng=random.Random(0xf51542800)
    for name,signature in {'perspective':[F]*6+[P,N],'orthographic':[F]*6+[P,N],
                          'projective_multiply':[P,N,P,N,P,N],'camera_basis':[P,N],
                          'frustum_matrix':[P,P,N],'frustum_point':[P,P,P]}.items():
        fn=getattr(core,'fs15_scene_'+name);fn.argtypes=signature;fn.restype=C.c_int
    core.fs15_affine_inverse.argtypes=[P,N,P,N];core.fs15_affine_inverse.restype=C.c_int
    with NativeGarCipher() as native:
        nf=lambda at,result,*signature:C.WINFUNCTYPE(result,*signature)(native.base+at)
        nf(0x524b0,P)();basis=nf(0xb2130,P,P);multiply=nf(0x1b060,P,P,P,P);inverse=nf(0x9e160,None,P,P)
        extract=nf(0x289cc0,None,P,P);point=nf(0x289e90,C.c_uint8,P,P)
        originals=[nf(at,None,*([F]*6+[P])) for at in (0x42800,0x42930)]
        for at,expected in [(0x49f344,2.0),(0x4db3e0,1.0),(0x4db594,-2.0),(0x4dc6e8,F(1.00001).value),(0x4dc6ec,F(0.99999).value)]:
            if C.string_at(native.base+at,4)!=struct.pack('<f',expected):raise AssertionError('Projection constant pin changed')
        def equal(a,b,label):
            if bytes(a)!=bytes(b):
                raise AssertionError(f'{label} differs: {list(a)[:16]} != {list(b)[:16]}')
            digest.update(bytes(a));counts[label]+=1
        for case in range(4000):
            extents=[rng.uniform(-100,100) for _ in range(4)]+[rng.uniform(-10,10),rng.uniform(10.01,10000)]
            if case<8:extents=[-2,2,-1,1,1,9];extents[case%6]=[-0.0,0.0,-2,2,0,9,1,-1][case]
            # Both extent orders, off-axis views, negative/zero distances and
            # signed zeros. No division by zero is supplied to the original.
            if any(extents[i]==extents[i+1] for i in (0,2,4)):continue
            for kind,name in enumerate(('perspective','orthographic')):
                a,b=(F*20)(*[123.25]*20),(F*20)(*[123.25]*20);originals[kind](*extents,a)
                if not getattr(core,'fs15_scene_'+name)(*extents,b,20):raise AssertionError('Valid projection rejected')
                equal(a,b,'exact_'+name+'_matrices_with_spares')
        for case in range(5000):
            a,b=(F*16)(*[rng.uniform(-100,100) for _ in range(16)]),(F*16)(*[rng.uniform(-100,100) for _ in range(16)])
            x,y=(F*20)(*[555.5]*20),(F*20)(*[555.5]*20);multiply(x,a,b)
            if not core.fs15_scene_projective_multiply(a,16,b,16,y,20):raise AssertionError('Projective product rejected')
            equal(x,y,'exact_full_projective_products_with_spares')
        for case in range(5000):
            values=[rng.uniform(-4,4) for _ in range(20)]
            if case<40:
                values=[0.0]*20
                for k in range(3):values[5*k]=[0.0,-0.0,1.0,-1.0,0.99999,1.00001,2,1e-12,1e12,0.5][case%10]
                values[4]=[0.0,1.0,0.99999,1.00001][case//10]
            x,y=(F*20)(*values),(F*20)(*values);basis(x)
            if not core.fs15_scene_camera_basis(y,20):raise AssertionError('Finite basis rejected')
            equal(x,y,'exact_camera_basis_records_with_spares')
        # Original XML supplies real projection parameters; extents calculation
        # and camera transforms below are authored fixtures, not a recovery of
        # the complete 0x1400b3e20 view/GPU/stereo path.
        cameras,assets=local_cases()
        for index,attributes in enumerate(cameras):
            values=dict(attributes);near=float(values.get('nearClip',0.1));far=float(values.get('farClip',10000))
            ortho=values.get('orthographic','false').lower()=='true';fov=float(values.get('fov',60));height=float(values.get('orthographicHeight',1))
            for aspect in (1.0,4/3,16/9):
                h=height if ortho else F(math.tan(math.radians(fov)*0.5)*near).value;w=F(h*aspect).value
                params=(-w,w,-h,h,near,far);projection=(F*16)();rebuilt_projection=(F*16)();kind=int(ortho)
                originals[kind](*params,projection)
                if not getattr(core,'fs15_scene_'+('orthographic' if ortho else 'perspective'))(*params,rebuilt_projection,16):raise AssertionError('I3D parameter projection rejected')
                equal(projection,rebuilt_projection,'original_i3d_parameter_projections')
                world=[0.0]*16
                for i in range(4):world[5*i]=1.0
                world[0]=1.5;world[4]=0.15;world[5]=0.75;world[10]=2.0;world[12:15]=[index%7*12.0,3.0,-11.0]
                a,b=(F*16)(*world),(F*16)(*world);basis(a)
                if not core.fs15_scene_camera_basis(b,16):raise AssertionError('View basis rejected')
                av,bv=(F*16)(),(F*16)();inverse(av,a)
                if not core.fs15_affine_inverse(b,16,bv,16):raise AssertionError('View inverse rejected')
                ap,bp=(F*16)(),(F*16)();multiply(ap,projection,av)
                if not core.fs15_scene_projective_multiply(rebuilt_projection,16,bv,16,bp,16):raise AssertionError('Projected view rejected')
                equal(ap,bp,'exact_i3d_projection_view_chains')
                af,bf=Frustum(),Frustum();af.table=bf.table=0x1404c7cc0;extract(C.byref(af),ap)
                if not core.fs15_scene_frustum_matrix(C.byref(bf),bp,16) or bytes(af)!=bytes(bf):raise AssertionError('Projected view planes differ')
                digest.update(bytes(af));counts['exact_i3d_projection_frustums']+=1
                for _ in range(8):
                    p=(F*3)(*[rng.uniform(-200,200) for _ in range(3)]);result=C.c_int();expected=point(C.byref(af),p)
                    if not core.fs15_scene_frustum_point(C.byref(bf),p,C.byref(result)) or bool(expected)!=bool(result.value):raise AssertionError('Projected point classification differs')
                    counts['i3d_projected_point_predicates']+=1
        paths=[ROOT/'src/core'/name for name in ['scene_projection.cpp','scene_projection.h','scene_rsqrt.h','scene_math.cpp','scene_math.h','scene_visibility.cpp','scene_visibility.h']]
        report=dict(status='passed',reference_sha256=EXPECTED_IMAGE,native_entries=['0x140042800','0x140042930','0x14001b060','0x1400b2130'],
            rebuilt_source_sha256={p.relative_to(ROOT).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in paths},tested_dll_sha256=hashlib.sha256(args.dll.read_bytes()).hexdigest(),
            comparisons=dict(counts),original_results_sha256=digest.hexdigest(),local_assets=assets,
            scope='Off-axis perspective and orthographic matrices, full projective multiplication and native Camera basis conditioning. Full records/spare lanes and original-I3D-parameter projection/inverse/frustum chains match original functions. FOV-to-extents calculation and world transforms are authored fixtures. Full view construction, stereo handling, resource lifetime, GPU upload, renderer and engine startup remain open.')
        write_json(ROOT/'analysis/scene-projection-validation.json',report);print(json.dumps(report,indent=2))

if __name__=='__main__':main()
