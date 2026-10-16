"""Differential evidence for the native culling grid and active Light updates."""
import argparse
from collections import Counter
import ctypes as C
import hashlib
import json
from pathlib import Path
import random
import struct
from inventory import ROOT,write_json
from native_gar import EXPECTED_IMAGE
from native_animation_xml import NativeAnimationXml
from check_scene_lifecycle import PortableServices,Services,P,N,read,put,DESTROY
from check_scene_light import Fixture,configure as configure_lights,local_cases

U=C.c_uint32;F=C.c_float;WORLD=C.CFUNCTYPE(C.c_int,P,P,P)
class Sphere(C.Structure):_fields_=[('vtable',N),('unknown08',U),('center',F*3),('radius',F),('unknown1c',U)]
class Entry(C.Structure):_fields_=[('token',P),('center',F*3),('radius',F),('clip',F*2),('lod_token',U),('unknown24',U)]
class Cell(C.Structure):_fields_=[('entries',P),('count',C.c_uint16),('capacity',C.c_uint16),('center_x',F),('center_z',F),('half_x',F),('half_z',F),('skip',U)]
class Grid(C.Structure):_fields_=[('cells',P),('dimension',U),('levels',U),('count',U),('min_x',F),('max_x',F),('min_z',F),('max_z',F),('min_y',F),('max_y',F),('scale_x',F),('scale_z',F),('unknown34',U)]
class Update(C.Structure):_fields_=[('user',P),('memory',C.POINTER(Services)),('world',WORLD)]

def configure(core):
    configure_lights(core)
    for name,signature in {
        'index':[P,U,U,U,P],'init':[P,P,P,C.c_int32,P],'select':[P,P,P,P,P,P],
        'cell_append':[P,P,P,F,F,U,P,P],'cell_move':[P,U,P,F,F,U],'cell_remove':[P,U],
        'add':[P,P,P,F,F,U,P],'move':[P,P,P,F,F,U,P],'remove':[P,P],'release':[P,P]
    }.items():
        f=getattr(core,'fs15_scene_spatial_'+name);f.argtypes=signature;f.restype=C.c_int
    for name,signature in {'bounds':[P,P],'update':[P,P,P],'spatial_destruct':[P,N,U,P,P,P]}.items():
        f=getattr(core,'fs15_scene_light_'+name);f.argtypes=signature;f.restype=C.c_int
    core.fs15_scene_lights_flush.argtypes=[P,P,P,P,P];core.fs15_scene_lights_flush.restype=C.c_int

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--dll',type=Path,required=True);args=parser.parse_args()
    core=C.CDLL(str(args.dll.resolve()));configure(core);rng=random.Random(0xf515d6660);counts=Counter();digest=hashlib.sha256()
    with NativeAnimationXml() as native:
        baseline=set(native.allocations);nf=lambda address,result,*types:C.WINFUNCTYPE(result,*types)(native.base+address)
        initialize=nf(0xd6660,None,P,F,F,F,F,F,F,C.c_int32);get_index=nf(0xd6170,U,P,U,U,U);select=nf(0xd6d90,P,P,P,P,P,P)
        append=nf(0xd62b0,U,P,P,P,F,F,U);cell_move=nf(0xd64e0,None,P,U,P,F,F,U);cell_remove=nf(0xd6440,None,P,U)
        add=nf(0xd6f00,None,P,P,P,F,F,U);move=nf(0xd7060,None,P,P,P,F,F,U);remove=nf(0xd6fc0,None,P,P);release=nf(0x3fabc8,None,P)
        def pointer(p):return C.addressof(p) if not isinstance(p,int) else p
        def equal_cell(a,b,maps):
            a,b=Cell.from_address(pointer(a)),Cell.from_address(pointer(b));x,y=bytearray(bytes(a)),bytearray(bytes(b));x[:8]=y[:8]=bytes(8)
            if x!=y:raise AssertionError('Spatial cell metadata differs')
            x,y=bytearray(C.string_at(a.entries,a.capacity*40) if a.entries else b''),bytearray(C.string_at(b.entries,b.capacity*40) if b.entries else b'')
            for i in range(a.capacity):
                for data,mapping in [(x,maps[0]),(y,maps[1])]:
                    value=struct.unpack_from('<Q',data,i*40)[0];struct.pack_into('<Q',data,i*40,mapping.get(value,value))
            if x!=y:raise AssertionError(f'Spatial complete cell allocation differs: {[hex(i) for i in range(len(x)) if x[i]!=y[i]][:16]}')
            digest.update(x);counts['complete_cell_allocations']+=1
        def equal_grid(a,b,maps):
            a,b=Grid.from_address(pointer(a)),Grid.from_address(pointer(b));x,y=bytearray(bytes(a)),bytearray(bytes(b));x[:8]=y[:8]=bytes(8)
            if x!=y:raise AssertionError(f'Spatial grid header differs: {x.hex()} != {y.hex()}')
            for i in range(a.count):equal_cell(a.cells+i*32,b.cells+i*32,maps)
            digest.update(x);counts['complete_grid_states']+=1
        def free_grid(a):
            g=Grid.from_address(pointer(a))
            if g.cells:
                for i in range(g.count):
                    c=Cell.from_address(g.cells+i*32)
                    if c.entries:release(c.entries)
                release(g.cells)
            g.cells=0
        def clean_retained(owner,maps=None):
            # Native grid reinitialization retains old per-cell buffers. This
            # explicit fixture reclamation is not part of the recovered path.
            extra=[p for p in native.allocations if p not in baseline]
            def buffers(side,mapping):
                result=[]
                for p,length in side:
                    data=bytearray(C.string_at(p,length))
                    if length%40:raise AssertionError('Unexpected retained allocation shape')
                    for at in range(0,length,40):
                        value=struct.unpack_from('<Q',data,at)[0];struct.pack_into('<Q',data,at,mapping.get(value,value))
                    result.append(bytes(data))
                return sorted(result)
            if maps:
                left=buffers([(p,native.allocations[p]) for p in extra],maps[0]);right=buffers([(p,len(block.raw)) for p,block in owner.blocks.items()],maps[1])
                if left!=right:raise AssertionError('Native retained cell allocations differ')
                for data in left:digest.update(data)
                counts['complete_retained_cell_allocations']+=len(left)
            elif extra or owner.blocks:raise AssertionError('Unexpected retained allocation')
            for p in extra:native.callbacks[1](native.heap,0,p)
            for p in list(owner.blocks):owner.release(None,p)
            counts['authored_retained_buffer_releases']+=len(extra)
        # Dense cell growth crosses both native allocation increments; random
        # opaque words verify that swap removal copies only the first 36 bytes.
        for case in range(24):
            owner=PortableServices(core);a,b=Cell(),Cell();keys=[(U*512)(),(U*512)()];maps=[{C.addressof(k)+i*4:i+1 for i in range(len(k))} for k in keys]
            for i in range(96+case*7):
                s=Sphere();s.center[:]=[rng.uniform(-500,500) for _ in range(3)];s.radius=rng.uniform(-3,100)
                sp=C.byref(s) if i%5 else None;near,far=rng.uniform(0,100),rng.uniform(100,1000);lod=rng.getrandbits(32);result=U()
                ap,bp=C.addressof(keys[0])+i*4,C.addressof(keys[1])+i*4;expected=append(C.byref(a),ap,sp,near,far,lod)
                if not core.fs15_scene_spatial_cell_append(C.byref(b),bp,sp,near,far,lod,C.byref(result),C.byref(owner.api)) or expected!=result.value:raise AssertionError('Dense cell append index differs')
                keys[0][i]=keys[1][i]=(i<<17)|(case*31&0x1ffff);opaque=rng.getrandbits(32)
                put(a.entries+i*40,36,opaque,'I');put(b.entries+i*40,36,opaque,'I');equal_cell(a,b,maps);counts['dense_cell_appends']+=1
            for step in range(36):
                slot=rng.randrange(a.count)
                if step%2:
                    cell_remove(C.byref(a),slot);ok=core.fs15_scene_spatial_cell_remove(C.byref(b),slot);counts['dense_cell_swap_removals']+=1
                else:
                    s=Sphere();s.center[:]=[rng.uniform(-500,500) for _ in range(3)];s.radius=30;sp=C.byref(s) if step%4 else None
                    cell_move(C.byref(a),slot,sp,50,100,step);ok=core.fs15_scene_spatial_cell_move(C.byref(b),slot,sp,50,100,step);counts['dense_cell_moves']+=1
                if not ok or bytes(keys[0])!=bytes(keys[1]):raise AssertionError('Dense cell owner tokens differ')
                equal_cell(a,b,maps)
            release(a.entries);owner.release(None,b.entries);clean_retained(owner)
        # Exact Morton/depth-first cell storage, including all 128x128 leaves.
        for case,dimension in enumerate([-5,0,1,2,3,7,16,31,64,127,128,129]):
            owner=PortableServices(core);a,b=Grid(),Grid();a.unknown34=b.unknown34=0xa5a5a5a5
            lo=(F*3)(-1024.25+case,-500,-2048.75-case);hi=(F*3)(1024.5,800,2048.125)
            initialize(C.byref(a),*lo,*hi,dimension)
            if not core.fs15_scene_spatial_init(C.byref(b),lo,hi,dimension,C.byref(owner.api)):raise AssertionError('Grid init rejected')
            equal_grid(a,b,[{},{}]);counts['grid_initializations']+=1
            for level in range(a.levels):
                for z in range(1<<level):
                    for x in range(1<<level):
                        actual=U();expected=get_index(C.byref(a),x,z,level)
                        if not core.fs15_scene_spatial_index(C.byref(b),x,z,level,C.byref(actual)) or actual.value!=expected:raise AssertionError('Grid index differs')
                        counts['cell_index_queries']+=1
            for step in range(100):
                s=Sphere();s.center[:]=[rng.uniform(-10000,10000),rng.uniform(-800,800),rng.uniform(-10000,10000)];s.radius=rng.uniform(-2,1500)
                al,ax,az,bl,bx,bz,bi=[U() for _ in range(7)];expected=select(C.byref(a),C.byref(s),C.byref(al),C.byref(ax),C.byref(az))
                if not core.fs15_scene_spatial_select(C.byref(b),C.byref(s),C.byref(bl),C.byref(bx),C.byref(bz),C.byref(bi)) or (al.value,ax.value,az.value,(expected-a.cells)//32)!=(bl.value,bx.value,bz.value,bi.value):raise AssertionError('Grid sphere selection differs')
                counts['sphere_cell_queries']+=1
            # Same parameters are an allocation-free no-op.
            old=(a.cells,b.cells);initialize(C.byref(a),*lo,*hi,dimension);core.fs15_scene_spatial_init(C.byref(b),lo,hi,dimension,C.byref(owner.api))
            if old!=(a.cells,b.cells):raise AssertionError('Equal grid init reallocated')
            equal_grid(a,b,[{},{}]);free_grid(a);core.fs15_scene_spatial_release(C.byref(b),C.byref(owner.api));clean_retained(owner)
        # Full mutable grids with owner tokens, swaps, migration and spare bytes.
        for case in range(24):
            owner=PortableServices(core);a,b=Grid(),Grid();lo=(F*3)(-256,-100,-256);hi=(F*3)(256,300,256);dimension=[1,2,4,8,16][case%5]
            initialize(C.byref(a),*lo,*hi,dimension);core.fs15_scene_spatial_init(C.byref(b),lo,hi,dimension,C.byref(owner.api))
            keys=[(U*80)(),(U*80)()];maps=[{C.addressof(k)+i*4:i+1 for i in range(80)} for k in keys];live=[]
            for step in range(240):
                s=Sphere();s.center[:]=[rng.uniform(-500,500),rng.uniform(-300,300),rng.uniform(-500,500)];s.radius=rng.uniform(0,90)
                sp=C.byref(s) if step%7 else None;near,far=rng.uniform(0,100),rng.uniform(100,1000);lod=rng.getrandbits(32)
                available=[i for i in range(80) if i not in live]
                if available and (not live or step%4==0):
                    i=rng.choice(available);live.append(i);op='add'
                else:i=rng.choice(live);op='remove' if step%4==1 else 'move'
                ap,bp=C.addressof(keys[0])+i*4,C.addressof(keys[1])+i*4
                if op=='remove':remove(C.byref(a),ap);ok=core.fs15_scene_spatial_remove(C.byref(b),bp);live.remove(i)
                else:
                    {'add':add,'move':move}[op](C.byref(a),ap,sp,near,far,lod);ok=getattr(core,'fs15_scene_spatial_'+op)(C.byref(b),bp,sp,near,far,lod,C.byref(owner.api))
                if not ok or bytes(keys[0])!=bytes(keys[1]):raise AssertionError(f'Spatial {op} tokens differ')
                equal_grid(a,b,maps);counts['grid_'+op+'_operations']+=1
                if step in (70,170):
                    dimension=4 if step==70 else 16;lo[0]-=100;hi[2]+=200
                    initialize(C.byref(a),*lo,*hi,dimension)
                    if not core.fs15_scene_spatial_init(C.byref(b),lo,hi,dimension,C.byref(owner.api)) or bytes(keys[0])!=bytes(keys[1]):raise AssertionError('Migrated owner tokens differ')
                    equal_grid(a,b,maps);counts['nonempty_grid_reinitializations']+=1
            for i in live:remove(C.byref(a),C.addressof(keys[0])+i*4);core.fs15_scene_spatial_remove(C.byref(b),C.addressof(keys[1])+i*4)
            equal_grid(a,b,maps);free_grid(a);core.fs15_scene_spatial_release(C.byref(b),C.byref(owner.api));clean_retained(owner,maps);counts['complete_mutable_grid_lifetimes']+=1
        # Packed slot 0x7fff means a linear owner search, not an ordinary index.
        owner=PortableServices(core);a,b=Grid(),Grid();lo=(F*3)(-1,-1,-1);hi=(F*3)(1,1,1);initialize(C.byref(a),*lo,*hi,1);core.fs15_scene_spatial_init(C.byref(b),lo,hi,1,C.byref(owner.api))
        keys=[(U*32771)(),(U*32771)()];maps=[{C.addressof(k)+i*4:i+1 for i in range(len(k))} for k in keys]
        for g,k,allocate in [(a,keys[0],lambda n:native.callbacks[0](native.heap,0,n)),(b,keys[1],lambda n:owner.allocate(None,n))]:
            c=Cell.from_address(g.cells);c.entries=allocate(32800*40);c.capacity=32800;c.count=len(k)
            for i in range(len(k)):
                k[i]=min(i,0x7fff)<<17;e=Entry.from_address(c.entries+i*40);e.token=C.addressof(k)+i*4;e.center[:]=[1,2,3];e.radius=4;e.clip[:]=[5,6];e.lod_token=i
        for i in (32767,32768,32770,0,32769):
            ap,bp=C.addressof(keys[0])+i*4,C.addressof(keys[1])+i*4
            move(C.byref(a),ap,None,7,8,9)
            if not core.fs15_scene_spatial_move(C.byref(b),bp,None,7,8,9,C.byref(owner.api)):raise AssertionError('Overflow-slot move rejected')
            equal_grid(a,b,maps);remove(C.byref(a),ap)
            if not core.fs15_scene_spatial_remove(C.byref(b),bp) or bytes(keys[0])!=bytes(keys[1]):raise AssertionError('Overflow-slot removal differs')
            equal_grid(a,b,maps);counts['overflow_slot_move_remove_pairs']+=1
        free_grid(a);core.fs15_scene_spatial_release(C.byref(b),C.byref(owner.api));clean_retained(owner)
        # Full original manager consumer with Shape/audio queues explicitly empty.
        local,assets=local_cases();cases=[]
        for case in range(20):
            cases.append((None,[('TransformGroup',[('name','lod'),('lodDistance','0 30 100'),('translation','5 0 5')]),('Light',[('name','point'),('range','35')]),('Light',[('name','spot'),('type','spot'),('range','40'),('coneAngle','75')]),('end',[]),('end',[]),('Light',[('name','directional'),('type','directional')]),('end',[]),('end',[])]))
        cases.extend(local)
        original_update=nf(0xda450,None,P);original_bounds=nf(0xda2e0,None,P);original_flush=nf(0x87f00,None,P)
        for case,(path,events) in enumerate(cases):
            class SpatialFixture(Fixture):
                def destroy(self,_,p):
                    if read(p)==0x1404c1d98:
                        if read(p,0x50,'I')&0x200:counts['active_spatial_light_destructions']+=1
                        return core.fs15_scene_light_spatial_destruct(p,0x200,1,C.byref(g),C.byref(self.lights),C.byref(self.owner.api))
                    ok=super().destroy(_,p)
                    if p==self.nodes[1][0] and ok:
                        maps=[{p+0x170:i+1 for i,p in enumerate(side) if self.kinds[i]==3} for side in self.nodes];equal_grid(ng,g,maps)
                        free_grid(ng);core.fs15_scene_spatial_release(C.byref(g),C.byref(self.owner.api))
                    return ok
            C.memset(native.base+0x162cbd0,0,0x140);f=SpatialFixture(core,native,case*1000);g=Grid();ng=Grid.from_address(native.base+0x162cc40);lo=(F*3)(-1024,-500,-1024);hi=(F*3)(1024,500,1024)
            initialize(C.byref(ng),*lo,*hi,16);core.fs15_scene_spatial_init(C.byref(g),lo,hi,16,C.byref(f.owner.api))
            @WORLD
            def world(_,p,out):
                array,kinds,view=f.view();position=(F*3)()
                ok=core.fs15_scene_owned_world(C.byref(view),f.lookup[p],position,3)
                if ok:C.memmove(out,p+0xd8,64)
                return ok
            services=Update(None,C.pointer(f.owner.api),world);f.load(events,counts)
            def compare():
                f.compare(counts,digest);maps=[{p+0x170:i+1 for i,p in enumerate(side) if f.kinds[i]==3} for side in f.nodes];equal_grid(ng,g,maps)
            def flush():
                original_flush(native.base+0x162cbd0)
                if not core.fs15_scene_lights_flush(C.byref(f.lod),C.byref(f.lod_services),C.byref(f.lights),C.byref(g),C.byref(services)):raise AssertionError('Native Light consumer rejected')
                compare();counts['complete_native_manager_flushes']+=1
            flush();lights=[i for i,k in enumerate(f.kinds) if k==3]
            for step in range(24):
                i=lights[step%len(lights)];a,b=f.nodes[0][i],f.nodes[1][i]
                if step%4==0:
                    position=[rng.uniform(-1000,1000) for _ in range(3)];frame=C.create_string_buffer(48)
                    for j,value in enumerate(position):put(C.addressof(frame),j*16,value,'f')
                    nf(0xe51f0,None,P,P)(a,frame);array,k,v=f.view()
                    if not core.fs15_scene_owned_trs_derived(C.byref(v),i,2,frame,len(frame),None,0,C.byref(f.used),C.byref(f.warning),C.byref(f.queue),C.byref(f.light_queue)) or not f.enqueue():raise AssertionError('Spatial Light transform rejected')
                    counts['active_light_transform_updates']+=1
                else:
                    for p in (a,b):
                        flags=read(p,0x50,'I');flags=flags|0xd00
                        if step%4==1:flags^=2
                        if step%4==2:flags^=4
                        if step%4==3:put(p,0x1d0,read(p,0x1d0,'I')^1,'I')
                        put(p,0x50,flags,'I')
                    original_update(a)
                    if not core.fs15_scene_light_update(b,C.byref(g),C.byref(services)):raise AssertionError('Active Light update rejected')
                    counts['active_light_visibility_spatial_updates']+=1
                if step%4==0:flush()
                else:compare()
            flush()
            # Leave every Light registered for the real recursive deleting
            # chain. The fixture releases the grids after the root is destroyed.
            for i in lights:
                for p in (f.nodes[0][i],f.nodes[1][i]):put(p,0x50,read(p,0x50,'I')|0x502,'I');put(p,0x1d0,read(p,0x1d0,'I')|1,'I')
                original_update(f.nodes[0][i]);core.fs15_scene_light_update(f.nodes[1][i],C.byref(g),C.byref(services))
            compare();f.close();counts['complete_active_light_forest_lifetimes']+=1
            if path:counts['complete_original_light_spatial_subtrees']+=1
        if set(native.allocations)!=baseline:raise AssertionError('Spatial fixture retained unexpected native storage')
    paths=sorted(p for p in (ROOT/'src/core').glob('*') if p.suffix in ('.cpp','.h'))
    report=dict(status='passed',reference_sha256=EXPECTED_IMAGE,native_entries=['0x1400d6170','0x1400d62b0','0x1400d6440','0x1400d64e0','0x1400d6660','0x1400d6d90','0x1400d6f00','0x1400d6fc0','0x1400d7060','0x1400da2e0','0x1400da450','0x140087f00'],rebuilt_source_sha256={p.relative_to(ROOT).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in paths},tested_dll_sha256=hashlib.sha256(args.dll.read_bytes()).hexdigest(),comparisons=dict(counts),original_results_sha256=digest.hexdigest(),local_assets=assets,scope='Native grid initialization/migration, Morton cell selection, allocation, packed owner slots, move/swap-removal and active Light bounds/update. Original manager consumer runs with Shape/audio queues empty; known LOD and Light queues are processed in native order. Full record/cell capacity/spares and owner tokens compared. Native old per-cell allocations retained at reinitialization are explicitly reclaimed only by the authored fixture. World callback/owned transport and guards are authored; spatial queries/frustum culling, Shape/audio dispatch, GPU lighting/shadows and complete startup remain open.')
    write_json(ROOT/'analysis/scene-spatial-validation.json',report);print(json.dumps(report,indent=2))

if __name__=='__main__':main()
