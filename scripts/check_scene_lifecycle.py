"""Native comparisons for recovered constructors, rename and lifetime chains.

Original CRT allocation and registry code execute in the native reference.
The portable component uses authored allocator/registry/dispatch services.
Only supported plain/base/TRS objects and empty attribute maps are compared.
"""
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
from native_scene_lifecycle import NativeSceneLifecycle

P=C.c_void_p;N=C.c_size_t
ALLOC=C.CFUNCTYPE(P,P,N);FREE=C.CFUNCTYPE(None,P,P);NEXT=C.CFUNCTYPE(C.c_uint32,P)
REGISTER=C.CFUNCTYPE(None,P,C.c_uint32,P)
UNLINK=C.CFUNCTYPE(C.c_int,P,P,C.c_uint32,C.c_int);DESTROY=C.CFUNCTYPE(C.c_int,P,P)


class Services(C.Structure):
    _fields_=[('user',P),('allocate',ALLOC),('deallocate',FREE),('next_id',NEXT),
              ('registered',REGISTER),('unregistered',REGISTER),('unlink',UNLINK),('destroy_child',DESTROY)]


def read(pointer,offset=0,fmt='Q'):return struct.unpack(fmt,C.string_at(pointer+offset,struct.calcsize(fmt)))[0]
def put(pointer,offset,value,fmt='Q'):C.memmove(pointer+offset,struct.pack(fmt,value),struct.calcsize(fmt))


class PortableServices:
    def __init__(self,core):
        self.core=core;self.blocks={};self.retired={};self.registry={};self.sequence=0;self.kinds={};self.delete_order=[]
        self.callbacks=[ALLOC(self.allocate),FREE(self.release),NEXT(self.identifier),REGISTER(self.register),REGISTER(self.unregister),
                        UNLINK(self.unlink),DESTROY(self.destroy_child)]
        self.api=Services(None,*self.callbacks)
    def allocate(self,_,size):
        block=C.create_string_buffer(bytes([0xa5])*size,size);pointer=C.addressof(block);self.blocks[pointer]=block;return pointer
    def release(self,_,pointer):
        if pointer:self.retired[pointer]=self.blocks.pop(pointer).raw
    def identifier(self,_):self.sequence=(self.sequence+1)&0xffffffff;return self.sequence
    def register(self,_,identifier,pointer):self.registry.setdefault(identifier,pointer)
    def unregister(self,_,identifier,pointer):
        self.registry.pop(identifier,None);self.delete_order.append(identifier)
    def unlink(self,_,parent,slot,suppress):
        begin,end=read(parent,0x30),read(parent,0x38)
        child=read(begin,slot*8);put(child,0x28,0)
        if slot*8+8<end-begin:C.memmove(begin+slot*8,begin+(slot+1)*8,end-begin-(slot+1)*8)
        put(parent,0x38,end-8)
        # During recursive destruction the parent dispatch has already become
        # the hierarchy-base table, which does not rescan observers.
        if read(parent)!=0x1404c7dd0 and read(child,0x50,'I')&0x10:
            cursor=parent
            while cursor:
                flags=read(cursor,0x50,'I');a,b=read(cursor,0x30),read(cursor,0x38)
                present=read(cursor,0x58)!=read(cursor,0x60) or any(read(read(a,j),0x50,'I')&0x10 for j in range(0,b-a,8))
                updated=flags|0x10 if present else flags&~0x10
                if flags==updated:break
                put(cursor,0x50,updated,'I');cursor=read(cursor,0x28)
        return 1
    def destroy_child(self,_,pointer):return self.core.fs15_scene_destruct(pointer,0x170,self.kinds[pointer],1,C.byref(self.api))


def canonical(data,kind,objects=None,child_slots=None):
    result=bytearray(data)
    for offset in ((0x10,) if kind==0 else (0x10,0x80) if kind==1 else (0x10,0x80,0x118)):
        struct.pack_into('<Q',result,offset,bool(struct.unpack_from('<Q',result,offset)[0]))
    if kind:
        parent=struct.unpack_from('<Q',result,0x28)[0]
        if parent:struct.pack_into('<Q',result,0x28,objects.index(parent)+1)
        if child_slots:
            begin=child_slots
            for offset in (0x30,0x38,0x40):
                value=struct.unpack_from('<Q',result,offset)[0]
                if value:struct.pack_into('<Q',result,offset,(value-begin)//8+1)
    return bytes(result)


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--dll',required=True,type=Path);args=parser.parse_args()
    core=C.CDLL(str(args.dll.resolve()))
    core.fs15_scene_construct.argtypes=[P,N,C.c_uint,C.c_char_p,N,C.c_uint32,C.c_uint8,C.POINTER(Services)]
    core.fs15_scene_construct.restype=C.c_int
    core.fs15_scene_rename.argtypes=[P,N,C.c_char_p,N,C.POINTER(Services)];core.fs15_scene_rename.restype=C.c_int
    core.fs15_scene_destruct.argtypes=[P,N,C.c_uint,C.c_uint32,C.POINTER(Services)];core.fs15_scene_destruct.restype=C.c_int
    rng=random.Random(0xf51511fe);counts=Counter();digest=hashlib.sha256()
    with NativeSceneLifecycle() as native:
        ctors=([C.WINFUNCTYPE(P,P,C.c_char_p,C.c_uint8,C.c_uint32)(native.base+0x4d640)]+
            [C.WINFUNCTYPE(P,P,C.c_char_p,C.c_uint32)(native.base+entry) for entry in (0xb5ec0,0xb15e0,0x10fa90,0x10fb40)])
        destructors=[C.WINFUNCTYPE(None,P)(native.base+entry) for entry in (0x4d760,0xb5f90,0xb1710,0x10fbb0,0x10fbb0)]
        # Node destructor is a deleting wrapper, so pass explicit flags=0.
        destructors[1]=C.WINFUNCTYPE(P,P,C.c_uint32)(native.base+0xb5f90)
        rename=C.WINFUNCTYPE(None,P,C.c_char_p)(native.base+0x4d7a0)
        for case in range(500):
            for kind in range(5):
                portable=PortableServices(core);seed=(0,0x7ffffffe,0xfffffffe,case*7)[case%4]
                portable.sequence=seed;C.c_uint32.from_address(native.base+0x1630d60).value=seed
                payload=rng.randbytes(0x170)
                original=C.create_string_buffer(payload,len(payload));rebuilt=C.create_string_buffer(payload,len(payload))
                names=[b'',b'node',b'Farm vehicle / component',b'\xc3\xa9quipe',b'x'*257]
                name=names[case%len(names)];tag=rng.getrandbits(32);registration=(0,1,7,255)[case%4]
                if kind==0:ctors[kind](original,name,registration,tag)
                else:ctors[kind](original,name,tag)
                if not core.fs15_scene_construct(rebuilt,len(payload),kind,name,len(name)+1,tag,registration,C.byref(portable.api)):raise AssertionError('Constructor rejected')
                expected_id=(seed+1)&0xffffffff
                if native.registry()!=({expected_id:C.addressof(original)} if kind or registration else {}):raise AssertionError('Original constructor registry differs')
                if portable.registry!=({expected_id:C.addressof(rebuilt)} if kind or registration else {}):raise AssertionError('Portable constructor registry differs')
                counts['registry_snapshots']+=1
                a,b=canonical(original.raw,kind),canonical(rebuilt.raw,kind)
                if a!=b:raise AssertionError(f'Constructor bytes differ for {case}/{kind}: {[hex(i) for i,(x,y) in enumerate(zip(a,b)) if x!=y][:16]}')
                for offset,size in ((0x80,0x50),(0x118,0x10)):
                    if kind==0 or (offset==0x118 and kind==1):continue
                    p,q=read(C.addressof(original),offset),read(C.addressof(rebuilt),offset)
                    aa,bb=bytearray(C.string_at(p,size)),bytearray(C.string_at(q,size))
                    if offset==0x80:
                        aa[:24]=bb[:24]=bytes(24)
                    if aa!=bb:raise AssertionError('Constructor auxiliary allocation differs')
                    counts['auxiliary_allocations_compared']+=1
                counts['constructors']+=1;digest.update(a)
                for step in range(3):
                    name=names[(case+step+1)%len(names)];rename(original,name)
                    if not core.fs15_scene_rename(rebuilt,len(payload),name,len(name)+1,C.byref(portable.api)):raise AssertionError('Rename rejected')
                    if C.string_at(read(C.addressof(original),0x10))!=C.string_at(read(C.addressof(rebuilt),0x10)):raise AssertionError('Name differs')
                    if canonical(original.raw,kind)!=canonical(rebuilt.raw,kind):raise AssertionError('Renamed record differs')
                    counts['renames']+=1
                if kind==1:destructors[kind](original,0)
                else:destructors[kind](original)
                if not core.fs15_scene_destruct(rebuilt,len(payload),kind,0,C.byref(portable.api)):raise AssertionError('Destructor rejected')
                a,b=canonical(original.raw,kind),canonical(rebuilt.raw,kind)
                if a!=b:raise AssertionError('Destructor record differs')
                if native.registry() or portable.registry or portable.blocks or len(native.allocations)!=1:raise AssertionError('Object lifetime leaked registry or owned allocations')
                counts['nondeleting_destructors']+=1;digest.update(a)
        # Real recursive deletion uses original vtables, registry removal and
        # original CRT free. Portable callbacks dispatch the recovered chain.
        for case in range(250):
            portable=PortableServices(core);portable.sequence=case*100
            C.c_uint32.from_address(native.base+0x1630d60).value=portable.sequence
            count=2+case%17;kinds=[rng.randrange(1,5) for _ in range(count)]
            objects=[[],[]];children=[[],[]];parents=[None]+[rng.randrange(i) for i in range(1,count)]
            for i,kind in enumerate(kinds):
                pointer=native.callbacks[0](native.heap,0,0x170);q=portable.allocate(None,0x170)
                objects[0].append(pointer);objects[1].append(q);portable.kinds[q]=kind
                payload=rng.randbytes(0x170);C.memmove(pointer,payload,len(payload));C.memmove(q,payload,len(payload))
                ctors[kind](pointer,b'tree',0x1234)
                if not core.fs15_scene_construct(q,0x170,kind,b'tree',5,0x1234,1,C.byref(portable.api)):raise AssertionError('Tree constructor rejected')
                expected_ids={case*100+j+1 for j in range(i+1)}
                if set(native.registry())!=expected_ids or set(portable.registry)!=expected_ids:raise AssertionError('Tree registry differs during creation')
                counts['registry_snapshots']+=1
            for i in range(count):
                kids=[j for j,p in enumerate(parents) if p==i]
                for side in range(2):
                    pointer=objects[side][i];put(pointer,0x28,objects[side][parents[i]] if parents[i] is not None else 0)
                    slots=(native.callbacks[0](native.heap,0,len(kids)*8) if side==0 else portable.allocate(None,len(kids)*8)) if kids else 0
                    children[side].append(slots)
                    for slot,j in enumerate(kids):put(slots,slot*8,objects[side][j])
                    put(pointer,0x30,slots);put(pointer,0x38,slots+len(kids)*8);put(pointer,0x40,slots+len(kids)*8)
            root=objects[0][0];wrapper=read(read(root));freed_before=len(native.freed)
            C.WINFUNCTYPE(P,P,C.c_uint32)(wrapper)(root,1)
            if not core.fs15_scene_destruct(objects[1][0],0x170,kinds[0],1,C.byref(portable.api)):raise AssertionError('Recursive destruction rejected')
            native_order=[objects[0].index(pointer) for pointer in native.freed[freed_before:] if pointer in objects[0]]
            actual_order=[identifier-case*100-1 for identifier in portable.delete_order]
            if native_order!=actual_order:raise AssertionError('Recursive deletion/registry-removal order differs')
            counts['deletion_order_traces']+=1
            for i,kind in enumerate(kinds):
                a=canonical(native.retired[objects[0][i]],kind,objects[0],children[0][i])
                b=canonical(portable.retired[objects[1][i]],kind,objects[1],children[1][i])
                if a!=b:raise AssertionError(f'Recursive destroyed record differs {case}/{i}: {[hex(j) for j,(x,y) in enumerate(zip(a,b)) if x!=y][:10]}')
                digest.update(a);counts['recursively_deleted_objects']+=1
            if native.registry() or portable.registry or portable.blocks or len(native.allocations)!=1:raise AssertionError('Recursive destruction leaked')
            counts['recursive_forests']+=1
    paths=('src/core/scene_lifecycle.cpp','src/core/scene_lifecycle.h','src/core/scene_state.h')
    report=dict(status='passed',reference_sha256=EXPECTED_IMAGE,
        native_entries=['0x14004d640','0x14004d760','0x14004d7a0','0x1400b5ec0','0x1400b15e0',
            '0x14010fa90','0x14010fb40','0x1400b1710','0x1401362a0','0x14010fbb0','0x1400b5f90','0x1400b16e0','0x14010fb10'],
        rebuilt_source_sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in paths},
        tested_dll_sha256=hashlib.sha256(args.dll.read_bytes()).hexdigest(),cases=dict(counts),original_results_sha256=digest.hexdigest(),
        native_reference_services='Private Windows heap, five rebound inspected kernel imports, explicit already-initialized singleton; original CRT and registry/tree bodies execute unchanged',
        portable_services='Authored allocator, wrapping-ID sequence, registry and known unlink/deleting-dispatch callbacks',
        scope='Supported base/node/matrix/TRS constructors, bounded names, empty attribute maps, no shared resource or active trigger, normal allocation success. Complete record/auxiliary bytes and recursive owned-object cleanup. OS/CRT/container internals, arbitrary derived classes, failures/exceptions, singleton startup and concurrent registry semantics are not recovered by these paths. No full engine or console execution.')
    write_json(ROOT/'analysis/scene-lifecycle-validation.json',report);print(json.dumps(report,indent=2))


if __name__=='__main__':main()
