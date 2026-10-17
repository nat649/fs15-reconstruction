"""Differential evidence for GE6 Light records, XML, virtual state and cloning."""
import argparse
from collections import Counter
import ctypes as C
import hashlib
import json
import math
from pathlib import Path
import random
import struct
import xml.etree.ElementTree as ET
from inventory import ROOT,write_json
from native_gar import EXPECTED_IMAGE
from native_animation_xml import NativeAnimationXml
from check_scene_lifecycle import PortableServices,Services,DESTROY,P,N,read,put,canonical
from check_scene_owned import OwnedView,Vector
from check_scene_groups import GroupServices,Result,CREATED,LINK
from check_scene_clone import CloneServices
from check_scene_clone_tree import Remap,TreeServices,entries
from check_scene_xml import XmlServices,REBUILD,PROPAGATE,INTEGER,REGISTER
from check_animation_xml import attributes
from check_scene_lod import Context,Manager,LodServices,BASE,WORLD,MARK,Queue
from check_scene_attributes import tree
from local_reference_assets import reference_asset

F=C.c_float;U=C.c_uint32;B=C.c_uint8
UNSIGNED=C.CFUNCTYPE(C.c_int,P,C.c_char_p,C.POINTER(U))
class LightParameters(C.Structure):
    _fields_=[('type',U),('diffuse',F*3),('specular',F*3),('emit_diffuse',B),('emit_specular',B),('cast_shadow',B),('perspective',B),('depth_bias',F),('depth_slope_bias',F),('depth_slope_clamp',F),('depth_resolution',U),('range',F),('decay',U),('cone_angle',F),('drop_off',F),('shadow_far',F),('shadow_extrusion',F),('shadow_perspective',F),('shadow_splits',U),('shadow_split_parameter',F)]
class LightContext(C.Structure):_fields_=[('pending',Vector)]
class LightXmlServices(C.Structure):_fields_=[('scene',GroupServices),('unsigned_integer',UNSIGNED)]


def configure(core):
    signatures={
        'fs15_scene_light_construct':[P,N,C.c_char_p,N,P,P,C.POINTER(Services)],'fs15_scene_light_dirty':[P,P,C.POINTER(Services)],
        'fs15_scene_light_enqueue':[P,P,C.POINTER(Services)],'fs15_scene_light_post':[P,U,P,C.POINTER(Services)],'fs15_scene_light_mark_tree':[P,U,P,C.POINTER(Services)],
        'fs15_scene_light_destruct':[P,N,U,P,C.POINTER(Services)],'fs15_scene_light_begin':[P,P,N,P,P,P],
        'fs15_scene_construct':[P,N,C.c_uint,C.c_char_p,N,U,B,C.POINTER(Services)],'fs15_scene_destruct':[P,N,C.c_uint,U,C.POINTER(Services)],
        'fs15_scene_group_begin':[P,P,N,P,P],'fs15_scene_camera_begin':[P,P,N,P,P],'fs15_scene_group_end':[P],
        'fs15_scene_owned_trs_derived':[P,N,C.c_uint,P,N,P,N,P,P,P,P],
        'fs15_scene_owned_state_lod':[P,N,C.c_uint,U,C.c_int32,P,N,P,P],
        'fs15_scene_owned_state_derived':[P,N,C.c_uint,U,C.c_int32,P,N,P,P,P],
        'fs15_scene_owned_link_derived':[P,N,N,U,C.c_int,C.POINTER(Services),P,N,P,P,P,P],
        'fs15_scene_owned_unlink_derived':[P,N,U,C.c_int,P,N,P,P,P],
        'fs15_scene_owned_world':[P,N,P,N],'fs15_scene_owned_matrix':[P,N,P,N],
        'fs15_scene_lod_enqueue':[P,P,C.POINTER(Services)],'fs15_scene_lod_update':[P,P,P],'fs15_scene_lod_destruct':[P,N,U,P,P],
        'fs15_scene_camera_destruct':[P,N,U,C.POINTER(Services)],'fs15_scene_property_flags':[P,U,C.c_int,P],
        'fs15_scene_clone_map_init':[P,C.POINTER(Services)],'fs15_scene_clone_map_clear':[P,C.c_int,C.POINTER(Services)],
        'fs15_scene_clone_tree':[P,P,P,P,P,P],'fs15_scene_clone_node':[P,P,P]}
    for name,signature in signatures.items():getattr(core,name).argtypes=signature;getattr(core,name).restype=C.c_int


def local_cases():
    inventory=ROOT/'analysis/inventory.json';paths=[ROOT/'recovered/gar/data/maps/map01.i3d',ROOT/'recovered/gar/data/maps/map02.i3d']
    if inventory.is_file():
        paths.extend([reference_asset('data/vehicles/steerable/deutz/deutzAgrostar661.i3d'),reference_asset('data/vehicles/steerable/newHolland/newHollandT8320.i3d')])
    cases=[];assets=[]
    for path in paths:
        if not path.is_file():continue
        scene=ET.parse(path).getroot().find('Scene')
        if scene is None:continue
        supported={};light_counts={}
        for node in reversed(list(scene.iter())):
            children=list(node);valid=node.tag in ('TransformGroup','Camera','Light') and all(supported.get(id(c),0)>0 for c in children)
            count=1+sum(supported.get(id(c),0) for c in children) if valid else 0;supported[id(node)]=count if count<=64 else 0
            light_counts[id(node)]=int(node.tag=='Light')+sum(light_counts.get(id(c),0) for c in children)
        selected=[]
        def choose(node):
            if supported.get(id(node),0) and light_counts[id(node)]:selected.append(node);return
            for child in node:choose(child)
        choose(scene);selected=selected[::max(1,len(selected)//18)][:24]
        def visit(node,out):
            out.append((node.tag,list(node.attrib.items())))
            for child in node:visit(child,out)
            out.append(('end',[]))
        for node in selected:
            events=[];visit(node,events);cases.append((path,events))
        assets.append(dict(name=path.name,sha256=hashlib.sha256(path.read_bytes()).hexdigest(),complete_light_subtrees=len(selected),original_lights=sum(light_counts[id(n)] for n in selected)))
    return cases,assets


class Fixture:
    def __init__(self,core,native,seed):
        self.core=core;self.native=native;self.owner=PortableServices(core);self.owner.sequence=seed;put(native.base+0x1630d60,0,seed,'I')
        self.allocate=lambda size:native.callbacks[0](native.heap,0,size);self.release=lambda p:native.callbacks[1](native.heap,0,p)
        self.nodes=[[],[]];self.kinds=[];self.sizes=[];self.lookup={};self.ids={};self.lod=Context();self.lights=LightContext();self.used=N();self.warning=C.c_int();self.attached=U()
        self.manager=native.base+0x162cc78;self.pending=native.base+0x162ccf8;self.light_pending=native.base+0x162ccb8
        C.memset(self.manager,0,0x20);C.memset(self.pending,0,24);C.memset(self.light_pending,0,24)
        self.requests=(N*2048)();self.light_requests=(N*2048)();self.queue=Queue(self.requests,len(self.requests),0);self.light_queue=Queue(self.light_requests,len(self.light_requests),0)
        self.cb={name:kind(getattr(self,name)) for name,kind in [('rebuild',REBUILD),('refresh',REBUILD),('propagate',PROPAGATE),('created',CREATED),('link',LINK),('integer',INTEGER),('registered',REGISTER),('unsigned_integer',UNSIGNED),('world',WORLD),('mark',MARK),('destroy',DESTROY)]}
        self.owner.api.destroy_child=self.cb['destroy'];self.common=XmlServices(None,C.pointer(self.owner.api),self.cb['rebuild'],self.cb['propagate'],self.cb['integer'],self.cb['registered'])
        self.group=GroupServices(self.common,self.cb['created'],self.cb['link']);self.xml=LightXmlServices(self.group,self.cb['unsigned_integer']);self.clone=TreeServices(CloneServices(self.common,self.cb['refresh'],self.cb['created']),self.cb['link']);self.lod_services=LodServices(None,C.pointer(self.owner.api),BASE(),self.cb['world'],self.cb['mark'])
        self.native_integer=C.WINFUNCTYPE(C.c_int32,C.c_char_p)(native.base+0x3fd784);self.native_unsigned=C.WINFUNCTYPE(U,C.c_char_p,P,C.c_int)(native.base+0x3fdb04)
        self.native_ctor=C.WINFUNCTYPE(P,P,C.c_char_p,U)(native.base+0x10fa90);self.native_delete=C.WINFUNCTYPE(P,P,U)(native.base+0x10fb10)
        self.original_begin={tag:C.WINFUNCTYPE(None,P,P,P)(native.base+address) for tag,address in [('TransformGroup',0x18c1a0),('Camera',0x18c430),('Light',0x18c770)]};self.original_end=C.WINFUNCTYPE(None,P)(native.base+0x18c370)
        self.original_recursive=C.WINFUNCTYPE(P,P,P,P,P,P)(native.base+0x1b9d70)
        a,b=self.allocate(0x170),self.owner.allocate(None,0x170);self.native_ctor(a,b'root',0)
        if not core.fs15_scene_construct(b,0x170,3,b'root',5,0,1,C.byref(self.owner.api)):raise AssertionError('Light root constructor rejected')
        self.nodes[0].append(a);self.created(None,b,1,0x170);put(native.base+0x162cd18,0,a);self.state_native(a,2,0);self.propagate(None,b,2,0)
        self.loader=C.create_string_buffer(0x900);self.lp=C.addressof(self.loader);put(self.lp,0x580,a);self.head=self.allocate(0x30)
        for at in (0,8,16):put(self.head,at,self.head)
        put(self.head,0x28,1,'B');put(self.head,0x29,1,'B');put(self.lp,0x120,self.head);self.current=P(b)
    def view(self):
        array=(P*len(self.nodes[1]))(*self.nodes[1]);kinds=(B*len(self.kinds))(*self.kinds);return array,kinds,OwnedView(array,len(array),kinds,0)
    def enqueue(self):
        for i in range(self.queue.count):
            if not self.core.fs15_scene_lod_enqueue(C.byref(self.lod),self.nodes[1][self.requests[i]],C.byref(self.owner.api)):return 0
        self.queue.count=0
        for i in range(self.light_queue.count):
            if not self.core.fs15_scene_light_enqueue(C.byref(self.lights),self.nodes[1][self.light_requests[i]],C.byref(self.owner.api)):return 0
        self.light_queue.count=0;return 1
    def rebuild(self,_,p):
        a,k,v=self.view();ok=self.core.fs15_scene_owned_trs_derived(C.byref(v),self.lookup[p],1,None,0,None,0,C.byref(self.used),C.byref(self.warning),C.byref(self.queue),C.byref(self.light_queue));return ok and self.enqueue()
    def refresh(self,_,p):
        a,k,v=self.view();return self.core.fs15_scene_owned_trs_derived(C.byref(v),self.lookup[p],0,None,0,None,0,C.byref(self.used),C.byref(self.warning),C.byref(self.queue),C.byref(self.light_queue))
    def propagate(self,_,p,mask,origin):
        a,k,v=self.view();ok=self.core.fs15_scene_owned_state_derived(C.byref(v),self.lookup[p],0,mask,origin,None,0,C.byref(self.used),C.byref(self.queue),C.byref(self.light_queue));return ok and self.enqueue()
    def created(self,_,p,kind,size):self.lookup[p]=len(self.nodes[1]);self.nodes[1].append(p);self.kinds.append(kind);self.sizes.append(size);self.owner.kinds[p]=3;return 1
    def link(self,_,parent,child,position):
        a,k,v=self.view();ok=self.core.fs15_scene_owned_link_derived(C.byref(v),self.lookup[parent],self.lookup[child],position,1,C.byref(self.owner.api),None,0,C.byref(self.used),C.byref(self.attached),C.byref(self.queue),C.byref(self.light_queue));return ok and self.enqueue()
    def integer(self,_,text,out):out[0]=self.native_integer(text)&0xffffffff;return 1
    def unsigned_integer(self,_,text,out):out[0]=self.native_unsigned(text,None,0);return 1
    def registered(self,_,key,p):self.ids.setdefault(key,p);return 1
    def world(self,_,p,out):
        a,k,v=self.view();return self.core.fs15_scene_owned_world(C.byref(v),self.lookup[p],out,3)
    def mark(self,_,p,token):return self.core.fs15_scene_light_mark_tree(p,token,C.byref(self.lights),C.byref(self.owner.api))
    def destroy(self,_,p):
        table=read(p)
        if table==0x1404c1d98:return self.core.fs15_scene_light_destruct(p,0x200,1,C.byref(self.lights),C.byref(self.owner.api))
        if table==0x1404c1c48:return self.core.fs15_scene_lod_destruct(p,0x188,1,C.byref(self.lod),C.byref(self.lod_services))
        if table==0x1404c1078:return self.core.fs15_scene_camera_destruct(p,0x190,1,C.byref(self.owner.api))
        return self.core.fs15_scene_destruct(p,0x170,3,1,C.byref(self.owner.api))
    def state_native(self,p,mask,origin):C.WINFUNCTYPE(None,P,U,C.c_int32)(read(read(p),0x20))(p,mask,origin)
    def load(self,events,counts):
        for tag,pairs in events:
            if tag=='end':self.original_end(self.loader);ok=self.core.fs15_scene_group_end(C.byref(self.current))
            else:
                na,pa=attributes(pairs);previous=read(self.lp,0x580);self.original_begin[tag](self.loader,None,na);p=read(self.lp,0x580)
                if p!=previous:self.nodes[0].append(p)
                result=Result();fn={'TransformGroup':self.core.fs15_scene_group_begin,'Camera':self.core.fs15_scene_camera_begin,'Light':self.core.fs15_scene_light_begin}[tag]
                ok=fn(C.byref(self.current),pa,len(pa),C.byref(self.xml),C.byref(self.lights),C.byref(result)) if tag=='Light' else fn(C.byref(self.current),pa,len(pa),C.byref(self.group),C.byref(result))
                counts['xml_'+tag+'_begins']+=1
            if not ok:raise AssertionError(f'Light forest callback rejected: {tag}/{pairs}')
    def compare(self,counts,digest):
        if len(self.nodes[0])!=len(self.nodes[1]):raise AssertionError('Light forest allocation count differs')
        maps=[{p:i+1 for i,p in enumerate(side)} for side in self.nodes]
        for i,(a,b) in enumerate(zip(*self.nodes)):
            size=self.sizes[i];x,y=bytearray(canonical(C.string_at(a,size),3,self.nodes[0])),bytearray(canonical(C.string_at(b,size),3,self.nodes[1]))
            for at in (0x30,0x38,0x40):x[at:at+8]=y[at:at+8]=bytes(8)
            for at,length in [(0x98,16),(0xd8,16),(0x148,9)]+([(0x19c,2)] if size==0x200 else []):
                av,bv=struct.unpack_from('<'+'f'*length,x,at),struct.unpack_from('<'+'f'*length,y,at)
                if any(not math.isfinite(a) or not math.isfinite(b) or abs(a-b)>1e-5*max(1,abs(a)) for a,b in zip(av,bv)):raise AssertionError(f'Light transform/cone differs {i}/{at:x}: {av} != {bv}')
                x[at:at+length*4]=y[at:at+length*4]=bytes(length*4)
            if x!=y:raise AssertionError(f'Light full record differs {i}: {[hex(j) for j in range(size) if x[j]!=y[j]][:16]}')
            def children(p,mapping):
                start,stop,cap=(read(p,o) for o in (0x30,0x38,0x40));return stop-start,cap-start,tuple(mapping.get(read(start,j),read(start,j)) for j in range(0,cap-start,8))
            if children(a,maps[0])!=children(b,maps[1]) or C.string_at(read(a,0x10))!=C.string_at(read(b,0x10)):raise AssertionError('Light child vectors/names differ')
            if tree(a)!=tree(b):raise AssertionError('Light copied attribute trees differ')
            pa,pb=read(a,0x118),read(b,0x118);ps={0x1404c0ca8:0x10,0x1404c7a90:0x88,0x1404c8158:0x90}[read(pa)];av,bv=bytearray(C.string_at(pa,ps)),bytearray(C.string_at(pb,ps))
            if ps>0x10:at=0x28 if ps==0x88 else 0x20;av[at:at+8]=bv[at:at+8]=bytes(8)
            if av!=bv:raise AssertionError('Light forest property metadata differs')
            digest.update(x);digest.update(av);counts['complete_light_forest_records']+=1
        def vector(v,mapping):return v.end-v.begin,v.capacity-v.begin,tuple(mapping.get(read(v.begin,j),read(v.begin,j)) for j in range(0,v.capacity-v.begin,8))
        if vector(Vector.from_address(self.pending),maps[0])!=vector(self.lod.pending,maps[1]) or vector(Vector.from_address(self.light_pending),maps[0])!=vector(self.lights.pending,maps[1]):raise AssertionError('LOD/Light pending allocation/order differs')
        ma,mb=Manager.from_address(self.manager),self.lod.manager;ah,bh=bytearray(C.string_at(self.manager,0x20)),bytearray(bytes(mb));ah[:8]=bh[:8]=bytes(8)
        if ah!=bh:raise AssertionError('Mixed Light LOD manager header differs')
        av,bv=bytearray(C.string_at(ma.entries,ma.capacity*0x28) if ma.entries else b''),bytearray(C.string_at(mb.entries,mb.capacity*0x28) if mb.entries else b'')
        for payload,mapping in ((av,maps[0]),(bv,maps[1])):
            for i in range(ma.capacity):
                value=struct.unpack_from('<Q',payload,i*0x28)[0];struct.pack_into('<Q',payload,i*0x28,mapping.get(value,value))
        for i in range(ma.count):
            x,y=struct.unpack_from('<3f',av,i*0x28+8),struct.unpack_from('<3f',bv,i*0x28+8)
            if any(abs(a-b)>1e-5*max(1,abs(a)) for a,b in zip(x,y)):raise AssertionError('Mixed Light LOD manager position differs')
            av[i*0x28+8:i*0x28+20]=bv[i*0x28+8:i*0x28+20]=bytes(12)
        if av!=bv:raise AssertionError('Mixed Light LOD manager allocation differs')
        counts['complete_mixed_lod_manager_allocations']+=1
        counts['complete_light_forest_states']+=1
    def clear_light_queue(self):
        for side in self.nodes:
            for p in side:
                if read(p)==0x1404c1d98:put(p,0x50,read(p,0x50,'I')&~0x800,'I')
        v=Vector.from_address(self.light_pending);v.end=v.begin;self.lights.pending.end=self.lights.pending.begin
    def flush_lod(self):
        v=Vector.from_address(self.pending);update=C.WINFUNCTYPE(None,P)(self.native.base+0xd7ea0)
        for i in range((v.end-v.begin)//8):
            update(read(v.begin,i*8))
            if not self.core.fs15_scene_lod_update(read(self.lod.pending.begin,i*8),C.byref(self.lod),C.byref(self.lod_services)):raise AssertionError('Mixed Light LOD update rejected')
        v.end=v.begin;self.lod.pending.end=self.lod.pending.begin
    def close(self):
        disable=C.WINFUNCTYPE(None,P,U)(self.native.base+0x9e5f0)
        for a,b in zip(*self.nodes):
            pa,pb=read(a,0x118),read(b,0x118)
            if read(pa)==0x1404c7a90 and read(pa,8,'I')&0x1000:disable(pa,0x1000);self.core.fs15_scene_property_flags(pb,0x1000,0,None)
        for a,b in [(a,b) for a,b in zip(*self.nodes) if not read(a,0x28)]:
            C.WINFUNCTYPE(P,P,U)(read(read(a)))(a,1)
            if not self.destroy(None,b):raise AssertionError('Light forest destruction rejected')
        if Manager.from_address(self.manager).count or self.lod.manager.count or Vector.from_address(self.light_pending).end!=Vector.from_address(self.light_pending).begin or self.lights.pending.end!=self.lights.pending.begin:raise AssertionError('Light destruction retained manager ownership')
        maps=[{p:i+1 for i,p in enumerate(side)} for side in self.nodes]
        def pending_bytes(v,mapping):return v.end-v.begin,v.capacity-v.begin,tuple(mapping.get(read(v.begin,i),read(v.begin,i)) for i in range(0,v.capacity-v.begin,8))
        if pending_bytes(Vector.from_address(self.light_pending),maps[0])!=pending_bytes(self.lights.pending,maps[1]):raise AssertionError('Light destruction pending spare bytes differ')
        def free_ids(p):
            if read(p,0x29,'B'):return
            free_ids(read(p));free_ids(read(p,16));self.release(p)
        free_ids(read(self.head,8));self.release(self.head)
        for p in (read(self.manager),read(self.pending),read(self.light_pending)):
            if p:self.release(p)
        for p in (self.lod.manager.entries,self.lod.pending.begin,self.lights.pending.begin):
            if p:self.owner.release(None,p)
        if self.owner.blocks or self.owner.registry or self.native.registry() or len(self.native.allocations)!=1:raise AssertionError('Light forest lifetime leaked')


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--dll',type=Path,required=True);args=parser.parse_args();core=C.CDLL(str(args.dll.resolve()));configure(core)
    rng=random.Random(0xf515d9bc0);counts=Counter();digest=hashlib.sha256()
    with NativeAnimationXml() as native:
        ctor=C.WINFUNCTYPE(P,P,C.c_char_p,U,P,B,P,B,B,F,F,F,U,B,F,U,F,F,F,F,F,U,F)(native.base+0xd9bc0)
        deleting=C.WINFUNCTYPE(P,P,U)(native.base+0xd9de0)
        for case in range(300):
            owner=PortableServices(core);owner.sequence=case*10;put(native.base+0x1630d60,0,owner.sequence,'I');p=LightParameters();p.type=case%4
            for i in range(3):p.diffuse[i]=rng.uniform(-1,2);p.specular[i]=rng.uniform(-1,2)
            p.emit_diffuse,p.emit_specular,p.cast_shadow,p.perspective=(case%2,(case//2)%2,(case//4)%2,(case//8)%2)
            p.depth_bias,p.depth_slope_bias,p.depth_slope_clamp=[rng.uniform(-1,1) for _ in range(3)];p.depth_resolution=case%3*512;p.range=rng.uniform(-10,100);p.decay=case%5;p.cone_angle=rng.uniform(-360,720);p.drop_off=rng.uniform(-3,9);p.shadow_far=80;p.shadow_extrusion=200;p.shadow_perspective=5;p.shadow_splits=case%7;p.shadow_split_parameter=0.4
            payload=rng.randbytes(0x200);a=C.create_string_buffer(payload,len(payload));b=C.create_string_buffer(payload,len(payload));context=LightContext()
            ctor(a,b'light',p.type,p.diffuse,p.emit_diffuse,p.specular,p.emit_specular,p.cast_shadow,p.depth_bias,p.depth_slope_bias,p.depth_slope_clamp,p.depth_resolution,p.perspective,p.range,p.decay,p.cone_angle,p.drop_off,p.shadow_far,p.shadow_extrusion,p.shadow_perspective,p.shadow_splits,p.shadow_split_parameter)
            if not core.fs15_scene_light_construct(b,0x200,b'light',6,C.byref(p),C.byref(context),C.byref(owner.api)):raise AssertionError('Light constructor rejected')
            x,y=bytearray(canonical(a.raw,3)),bytearray(canonical(b.raw,3))
            for at in (0x19c,0x1a0):
                av,bv=struct.unpack_from('<f',x,at)[0],struct.unpack_from('<f',y,at)[0]
                if abs(av-bv)>1e-6:raise AssertionError('Light constructor cone factors differ')
                x[at:at+4]=y[at:at+4]=bytes(4)
            if x!=y:raise AssertionError(f'Light constructor full bytes differ: {[hex(i) for i in range(512) if x[i]!=y[i]][:16]}')
            digest.update(x);counts['complete_light_constructors']+=1
            # Direct post-phase API versus a complete original virtual call on
            # a valid single-node root; the portable base call intentionally
            # selects only the inherited base body before the Light post phase.
            aa,bb=C.addressof(a),C.addressof(b);mask=[0,1,2,4,8,0x3f][case%6]
            for address in (aa,bb):put(address,0x50,read(address,0x50,'I')|(2 if case%2 else 0),'I')
            put(native.base+0x162cd18,0,aa);C.WINFUNCTYPE(None,P,U,C.c_int32)(native.base+0xda110)(aa,mask,0)
            array=(P*1)(bb);kinds=(B*1)(1);view=OwnedView(array,1,kinds,0);used=N()
            if not core.fs15_scene_owned_state_lod(C.byref(view),0,0,mask,0,None,0,C.byref(used),None) or not core.fs15_scene_light_post(bb,mask,C.byref(context),C.byref(owner.api)):raise AssertionError('Light post-phase rejected')
            if canonical(a.raw,3)!=canonical(b.raw,3):raise AssertionError('Light direct post-phase record differs')
            va=Vector.from_address(native.base+0x162ccb8)
            def queued(v,pointer):return v.end-v.begin,v.capacity-v.begin,tuple(1 if read(v.begin,j)==pointer else read(v.begin,j) for j in range(0,v.capacity-v.begin,8))
            if queued(va,aa)!=queued(context.pending,bb):raise AssertionError('Direct Light post queue differs')
            counts['complete_direct_light_post_states']+=1;deleting(a,0)
            if not core.fs15_scene_light_destruct(b,0x200,0,C.byref(context),C.byref(owner.api)) or canonical(a.raw,3)!=canonical(b.raw,3):raise AssertionError('Light destructor full bytes differ')
            if queued(va,aa)!=queued(context.pending,bb) or va.end!=va.begin:raise AssertionError('Direct Light pending destructor differs')
            if va.begin:native.callbacks[1](native.heap,0,va.begin)
            C.memset(native.base+0x162ccb8,0,24)
            if context.pending.begin:owner.release(None,context.pending.begin)
            if owner.blocks or owner.registry or native.registry() or len(native.allocations)!=1:raise AssertionError('Light constructor lifetime leaked')
            counts['nondeleting_light_destructors']+=1
        local,assets=local_cases();cases=[]
        for case in range(40):
            root=[('name','source'),('translation','5 1 -2')]
            if case%2:root.append(('lodDistance','0 10 20 30'))
            pairs=[('name','light'),('type',['point','directional','SpOt','unknown'][case%4]),('diffuseColor','0.5 1 0.25'),('specularColor','0.2 0.3 0.4'),('emitDiffuse',['true','FALSE','1',''][case%4]),('emitSpecular','TrUe'),('castShadowMap',['true','false','1'][case%3]),('depthMapBias','0.01'),('depthMapSlopeScaleBias','0.02'),('depthMapSlopeClamp','0.03'),('depthMapResolution','-1' if case%3 else '1024'),('range','25'),('decayRate',['0','2','-1',' 4','4294967298'][case%5]),('coneAngle','75'),('dropOff','9'),('shadowPerspective','true'),('shadowPerspectiveParameter','3'),('shadowFarDistance','100'),('shadowExtrusionDistance','250'),('numShadowMapSplits',['0','3','99',' 2','4294967297'][case%5]),('shadowMapSplitDistancesParameter','0.7'),('affectedLightMasks',['0xff','-1','0777','4294967296','0x1g','-0x10'][case%6]),('dynamic','true'),('joint','true')]
            if case%2:pairs.reverse()
            cases.append((None,[('TransformGroup',root),('Light',pairs),('Light',[('name','nested'),('type','spot')]),('end',[]),('TransformGroup',[('name','light-child'),('translation','1 2 3')]),('end',[]),('end',[]),('Camera',[('name','camera'),('fov','60')]),('end',[]),('end',[]),('Light',[('name',None),('name','ignored')]),('Light',[('Name','ignored')])]))
        cases.extend(local)
        for case,(path,events) in enumerate(cases):
            f=Fixture(core,native,case*1000);f.load(events,counts);f.compare(counts,digest)
            lights=[i for i,kind in enumerate(f.kinds) if kind==3]
            for step in range(18):
                i=lights[step%len(lights)] if step%3==0 else 1;a,b=f.nodes[0][i],f.nodes[1][i]
                if step%6==0:
                    angle,drop=rng.uniform(-180,360),rng.uniform(-2,8)
                    for p in (a,b):put(p,0x198,angle,'f');put(p,0x194,drop,'f')
                    C.WINFUNCTYPE(None,P)(native.base+0xda210)(a);ok=core.fs15_scene_light_dirty(b,C.byref(f.lights),C.byref(f.owner.api));counts['light_cone_dirty_updates']+=1
                elif step%6==1:
                    mask=[0,1,2,4,8,0x3f][(case+step)%6];f.state_native(a,mask,step%2);ok=f.propagate(None,b,mask,step%2);counts['mixed_virtual_state_updates']+=1
                elif step%6==2:
                    token=rng.getrandbits(32);C.WINFUNCTYPE(None,P,U)(read(read(a),0x28))(a,token)
                    if step==8:
                        array,kind,v=f.view();ok=core.fs15_scene_owned_state_derived(C.byref(v),i,2,token,0,None,0,C.byref(f.used),C.byref(f.queue),C.byref(f.light_queue));ok=ok and f.enqueue();counts['staged_inherited_marker_updates']+=1
                    else:ok=f.mark(None,b,token)
                    counts['mixed_inherited_marker_updates']+=1
                elif step%6==3:
                    frame=C.create_string_buffer(48);position=[rng.uniform(-20,20) for _ in range(3)]
                    for j,value in enumerate(position):put(C.addressof(frame),j*16,value,'f')
                    C.WINFUNCTYPE(None,P,P)(native.base+0xe51f0)(a,frame);array,kind,v=f.view();ok=core.fs15_scene_owned_trs_derived(C.byref(v),i,2,frame,len(frame),None,0,C.byref(f.used),C.byref(f.warning),C.byref(f.queue),C.byref(f.light_queue));ok=ok and f.enqueue();counts['mixed_light_trs_updates']+=1
                elif step%6==4:
                    child_a,child_b=f.nodes[0][lights[0]],f.nodes[1][lights[0]];parent_a,parent_b=read(child_a,0x28),read(child_b,0x28)
                    start,stop=read(parent_a,0x30),read(parent_a,0x38);slot=next(j for j in range((stop-start)//8) if read(start,j*8)==child_a)
                    C.WINFUNCTYPE(None,P,U,C.c_int)(native.base+0xb19c0)(parent_a,slot,0);array,kind,v=f.view();ok=core.fs15_scene_owned_unlink_derived(C.byref(v),f.lookup[parent_b],slot,0,None,0,C.byref(f.used),C.byref(f.queue),C.byref(f.light_queue));ok=ok and f.enqueue()
                    if not ok:raise AssertionError('Light unlink rejected')
                    f.compare(counts,digest);C.WINFUNCTYPE(None,P,P,U)(native.base+0xb17b0)(parent_a,child_a,0xffffffff);ok=f.link(None,parent_b,child_b,0xffffffff)
                    f.flush_lod();counts['mixed_light_unlink_relinks']+=1;counts['mixed_lod_flushes']+=1
                else:
                    f.clear_light_queue();ok=1;counts['authored_pending_consumer_fixtures']+=1
                if not ok:raise AssertionError(f'Light operation rejected {case}/{step}')
                f.compare(counts,digest)
            remap=Remap();created=Vector();result=Result();unfinished=Result();native_remap=Remap(0,f.allocate(0x30),0);native_list=Vector()
            for at in (0,8,16):put(native_remap.head,at,native_remap.head)
            put(native_remap.head,0x28,1,'B');put(native_remap.head,0x29,1,'B');core.fs15_scene_clone_map_init(C.byref(remap),C.byref(f.owner.api))
            f.original_recursive(f.nodes[0][1],C.byref(native_remap),C.byref(native_list),None,None)
            if not core.fs15_scene_clone_tree(f.nodes[1][1],C.byref(remap),C.byref(created),C.byref(f.clone),C.byref(result),C.byref(unfinished)):raise AssertionError('Light recursive clone rejected')
            f.nodes[0].extend(read(native_list.begin,i) for i in range(0,native_list.end-native_list.begin,8));counts['recursive_light_clone_nodes']+=(created.end-created.begin)//8;f.compare(counts,digest);f.flush_lod();f.compare(counts,digest)
            maps=[{p:i+1 for i,p in enumerate(side)} for side in f.nodes]
            if {maps[0][key]:maps[0][read(p,0x20)] for key,p in entries(native_remap).items()}!={maps[1][key]:maps[1][read(p,0x20)] for key,p in entries(remap).items()}:raise AssertionError('Light recursive remap associations differ')
            def cloned_list(v,mapping):return v.end-v.begin,v.capacity-v.begin,tuple(mapping.get(read(v.begin,i),read(v.begin,i)) for i in range(0,v.capacity-v.begin,8))
            if cloned_list(native_list,maps[0])!=cloned_list(created,maps[1]):raise AssertionError('Light recursive clone-list allocation differs')
            C.WINFUNCTYPE(None,P,P)(native.base+0x96190)(C.byref(native_remap),read(native_remap.head,8));f.release(native_remap.head);core.fs15_scene_clone_map_clear(C.byref(remap),1,C.byref(f.owner.api))
            if native_list.begin:f.release(native_list.begin)
            if created.begin:f.owner.release(None,created.begin)
            f.close();counts['complete_light_forest_lifetimes']+=1
            if path:counts['complete_original_light_subtree_lifetimes']+=1
        put(native.base+0x162cd18,0,0);C.memset(native.base+0x162ccb8,0,24);C.memset(native.base+0x162cc78,0,0x20);C.memset(native.base+0x162ccf8,0,24)
    paths=sorted(p for p in (ROOT/'src/core').glob('*') if p.suffix in ('.cpp','.h'))
    report=dict(status='passed',reference_sha256=EXPECTED_IMAGE,native_entries=['0x1400d9bc0','0x1400d9de0','0x1400d9e10','0x1400d9f30','0x1400da110','0x1400da1b0','0x1400da210','0x140087cf0','0x14018c770'],rebuilt_source_sha256={p.relative_to(ROOT).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in paths},tested_dll_sha256=hashlib.sha256(args.dll.read_bytes()).hexdigest(),comparisons=dict(counts),original_results_sha256=digest.hexdigest(),local_assets=assets,
        scope='Light construction, pending-only lifetime, cone dirty/virtual state/inherited marker and pending append, decoded XML begin, and recursive clone integration with known TRS/Camera/LOD/Light dispatch. Original code unchanged in isolated fixture; typed unsigned/signed CRT parsing and owned staging/callback transport are seams. Complete records/properties/attributes/child and pending allocations checked, cone tolerance 1e-6 direct and transform/libm tolerance 1e-5 in forests. Authored consumer fixture clears queued flags/list without executing original renderer updates. Active spatial registration, render/GPU resources, queue consumption, Shape/reference resources, full top-level finalization and engine startup remain open.')
    write_json(ROOT/'analysis/scene-light-validation.json',report);print(json.dumps(report,indent=2))


if __name__=='__main__':main()
