"""Compare native-layout attribute trees, script frames and owned cleanup.

Runs original code only in the pinned isolated private-heap reference. The
portable component executes independently through authored allocation services.
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
from check_scene_lifecycle import PortableServices,Services,P,N,read,put,canonical


class Strings(C.Structure):
    _fields_=[('key',P),('key_bytes',N),('type',P),('type_bytes',N),('value',P),('value_bytes',N)]


def tree(pointer):
    head=read(pointer,0x80);pending=[read(head,8)];nodes={};ids={head:b'#sentinel'}
    while pending:
        node=pending.pop()
        if node==head:continue
        if node in nodes:raise AssertionError('Cyclic native attribute tree')
        size,capacity=read(node,0x28),read(node,0x30)
        key=C.string_at(node+0x18 if capacity<=15 else read(node,0x18),size)
        nodes[node]=key;ids[node]=key
        pending.extend((read(node),read(node,0x10)))
    if len(nodes)!=read(pointer,0x88):raise AssertionError('Attribute count differs')
    rows={}
    for node,key in [(head,b'#sentinel'),*nodes.items()]:
        payload=bytearray(C.string_at(node,0x50))
        for at in (0,8,16):struct.pack_into('<Q',payload,at,0)
        links=[ids[read(node,at)] for at in (0,8,16)]
        value=None;text=None;key_heap=None
        if node!=head:
            capacity=read(node,0x30)
            if capacity>15:
                key_heap=C.string_at(read(node,0x18),capacity+1);struct.pack_into('<Q',payload,0x18,0)
            address=read(node,0x40);struct.pack_into('<Q',payload,0x40,0)
            if address:
                value=bytearray(C.string_at(address,16));kind=read(address,8,'I')
                if kind in (3,4):
                    text=C.string_at(read(address));struct.pack_into('<Q',value,0,1)
        rows[key]=(bytes(payload),links,bytes(value) if value else None,text,key_heap)
    return rows


def owned_roles(pointer):
    head=read(pointer,0x80)
    roles={head:('header',b''),read(pointer,0x10):('name',b''),read(pointer,0x118):('properties',b'')}
    pending=[read(head,8)]
    while pending:
        node=pending.pop()
        if node==head:continue
        size,capacity=read(node,0x28),read(node,0x30)
        key=C.string_at(node+0x18 if capacity<=15 else read(node,0x18),size)
        roles[node]=('entry',key);value=read(node,0x40)
        if value:
            roles[value]=('value',key)
            if read(value,8,'I') in (3,4) and read(value):roles[read(value)]=('text',key)
        if capacity>15:roles[read(node,0x18)]=('key-buffer',key)
        pending.extend((read(node),read(node,0x10)))
    return roles


def compare_trees(a,b,counts,digest,residues):
    first,second=tree(a),tree(b)
    # Scalar writes can retain upper bytes of a previous string pointer. Check
    # those against their actual allocation, then canonicalize the addresses.
    for key,(p,q,width) in residues.items():
        for rows,pointer in ((first,p),(second,q)):
            row=list(rows[key]);value=bytearray(row[2]);expected=struct.pack('<Q',pointer)
            if value[width:8]!=expected[width:]:raise AssertionError('Native pointer residue differs')
            value[width:8]=bytes(8-width);row[2]=bytes(value);rows[key]=tuple(row)
    if first!=second:
        for key in first.keys()|second.keys():
            if first.get(key)!=second.get(key):raise AssertionError(f'Attribute tree differs at {key!r}: {first.get(key)} vs {second.get(key)}')
    for key in sorted(first):
        row=first[key];digest.update(key);digest.update(row[0]);digest.update(row[2] or b'');digest.update(row[3] or b'')
    counts['complete_tree_comparisons']+=1;counts['tree_nodes_compared']+=len(first)-1


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--dll',type=Path,required=True);args=parser.parse_args()
    core=C.CDLL(str(args.dll.resolve()))
    core.fs15_scene_construct.argtypes=[P,N,C.c_uint,C.c_char_p,N,C.c_uint32,C.c_uint8,C.POINTER(Services)];core.fs15_scene_construct.restype=C.c_int
    core.fs15_scene_destruct.argtypes=[P,N,C.c_uint,C.c_uint32,C.POINTER(Services)];core.fs15_scene_destruct.restype=C.c_int
    core.fs15_scene_attribute_frame.argtypes=[P,N,C.c_uint,C.POINTER(Strings),P,N,C.POINTER(Services)];core.fs15_scene_attribute_frame.restype=C.c_int
    core.fs15_scene_attribute_ensure.argtypes=[P,N,C.c_char_p,N,C.c_uint32,C.POINTER(Services),C.POINTER(P)];core.fs15_scene_attribute_ensure.restype=C.c_int
    rng=random.Random(0xf515a77);counts=Counter();digest=hashlib.sha256()
    with NativeSceneLifecycle() as native:
        ctor=C.WINFUNCTYPE(P,P,C.c_char_p,C.c_uint32)(native.base+0x10fa90)
        setter=C.WINFUNCTYPE(None,P,P)(native.base+0xe69b0);getter=C.WINFUNCTYPE(None,P,P)(native.base+0xe6900)
        ensure=C.WINFUNCTYPE(P,P,C.c_char_p,C.c_uint32)(native.base+0x136870)
        destroy=C.WINFUNCTYPE(None,P)(native.base+0x10fbb0)
        for case in range(180):
            portable=PortableServices(core);seed=case*10;portable.sequence=seed
            C.c_uint32.from_address(native.base+0x1630d60).value=seed
            payload=rng.randbytes(0x170);a=C.create_string_buffer(payload,len(payload));b=C.create_string_buffer(payload,len(payload))
            x,y=C.addressof(a),C.addressof(b);ctor(a,b'attributes',0)
            if not core.fs15_scene_construct(b,0x170,3,b'attributes',11,0,1,C.byref(portable.api)):raise AssertionError('Constructor failed')
            keys=[b'',b'a',b'A',b'farm',b'farm.root',b'key_15_bytes___',b'key_16_bytes____',b'\x80tree',b'\xc3\xa9quipe']
            keys += [f'key_{i:03}_'.encode()+b'x'*(i%40) for i in range(case%31)]
            rng.shuffle(keys)
            orphaned=[[],[]];residues={}
            for step in range(70):
                key=keys[step%len(keys)];key_buffer=C.create_string_buffer(key)
                kind=(case+step)%5;type_name=(b'Boolean',b'Integer',b'Float',b'String',b'Unsupported')[kind]
                type_buffer=C.create_string_buffer(type_name);text=C.create_string_buffer((b'' if step%3==0 else b'value-'+bytes([65+step%26])*step))
                views=Strings(C.addressof(key_buffer),len(key)+1,C.addressof(type_buffer),len(type_name)+1,C.addressof(text),len(text))
                frame=C.create_string_buffer(rng.randbytes(0x110),0x110)
                put(C.addressof(frame),0,C.addressof(key_buffer));put(C.addressof(frame),0x10,C.addressof(type_buffer))
                tag=(1,3)[step%2];put(C.addressof(frame),0x28,tag,'H')
                if kind==3:put(C.addressof(frame),0x20,C.addressof(text))
                elif kind==0:put(C.addressof(frame),0x20,rng.randrange(256),'B')
                elif kind==1 and tag==1:put(C.addressof(frame),0x20,rng.getrandbits(32),'I')
                else:put(C.addressof(frame),0x20,(0,0x7fc00000,0x7f800000,0xff800000,0x4f000000,0xcf000000,rng.getrandbits(32))[step%7],'I')
                # Track observed original String->scalar retention separately;
                # tree/frame equivalence must not hide its allocation effect.
                if kind==3:residues.pop(key,None)
                if kind<3:
                    previous_pointers=[]
                    for side,pointer in enumerate((x,y)):
                        before=tree(pointer).get(key)
                        if before and before[2] and struct.unpack_from('<I',before[2],8)[0]==3:
                            out=(ensure(pointer+0x78,key,3) if side==0 else None)
                            if side==1:
                                p=P();core.fs15_scene_attribute_ensure(y,0x170,key,len(key)+1,3,C.byref(portable.api),C.byref(p));out=p.value
                            orphaned[side].append(read(out))
                            previous_pointers.append(read(out))
                            if side==0:counts['native_string_to_scalar_retained_allocations']+=1
                    width=1 if kind==0 else 4
                    if previous_pointers:residues[key]=(*previous_pointers,width)
                    elif key in residues:
                        p,q,old_width=residues[key];residues[key]=(p,q,max(old_width,width))
                original_frame=frame.raw
                setter(x,frame)
                if not core.fs15_scene_attribute_frame(y,0x170,1,C.byref(views),C.create_string_buffer(original_frame,len(original_frame)),len(original_frame),C.byref(portable.api)):raise AssertionError('Setter failed')
                if frame.raw!=original_frame:raise AssertionError('Native setter changed frame')
                counts['setter_frames']+=1;compare_trees(x,y,counts,digest,residues)
                for query in (key,b'missing-key'):
                    query_buffer=C.create_string_buffer(query);views.key=C.addressof(query_buffer);views.key_bytes=len(query)+1
                    initial=rng.randbytes(0x110);f=C.create_string_buffer(initial,len(initial));g=C.create_string_buffer(initial,len(initial))
                    put(C.addressof(f),0,views.key);put(C.addressof(g),0,views.key)
                    getter(x,f)
                    if not core.fs15_scene_attribute_frame(y,0x170,0,C.byref(views),g,len(g),C.byref(portable.api)):raise AssertionError('Getter failed')
                    left,right=bytearray(f.raw),bytearray(g.raw)
                    if read(C.addressof(f),0xf8,'H')==4:
                        if C.string_at(read(C.addressof(f),0xf0))!=C.string_at(read(C.addressof(g),0xf0)):raise AssertionError('Getter text differs')
                        left[0xf0:0xf8]=right[0xf0:0xf8]=bytes(8)
                    if left!=right:raise AssertionError('Complete getter frames differ')
                    counts['getter_frames']+=1;digest.update(left)
                    # The original getter can also consume the reconstructed
                    # tree directly: this checks real memory-layout reuse.
                    h=C.create_string_buffer(initial,len(initial));put(C.addressof(h),0,views.key);getter(y,h)
                    if h.raw!=g.raw:raise AssertionError('Original getter cannot consume reconstructed tree')
                    counts['original_getters_on_reconstructed_trees']+=1
            # Additional observed kinds and the ensure-existing path are not
            # normally produced by the four public setter type names.
            for initial_kind in (4,42):
                key=b'kind-edge-'+str(initial_kind).encode();key_buffer=C.create_string_buffer(key)
                p=ensure(x+0x78,key,initial_kind);q=P()
                if not core.fs15_scene_attribute_ensure(y,0x170,key,len(key)+1,initial_kind,C.byref(portable.api),C.byref(q)):raise AssertionError('Ensure failed')
                if initial_kind==4:
                    text=b'kind-four-text\0';r=native.callbacks[0](native.heap,0,len(text));s=portable.allocate(None,len(text))
                    C.memmove(r,text,len(text));C.memmove(s,text,len(text));put(p,0,r);put(q.value,0,s)
                    orphaned[0].append(r);orphaned[1].append(s)
                again=ensure(x+0x78,key,99);other=P()
                if not core.fs15_scene_attribute_ensure(y,0x170,key,len(key)+1,99,C.byref(portable.api),C.byref(other)) or p!=again or q.value!=other.value:raise AssertionError('Ensure replaced existing value')
                views.key=C.addressof(key_buffer);views.key_bytes=len(key)+1
                initial=rng.randbytes(0x110);f=C.create_string_buffer(initial,len(initial));g=C.create_string_buffer(initial,len(initial))
                put(C.addressof(f),0,views.key);put(C.addressof(g),0,views.key);getter(x,f)
                if not core.fs15_scene_attribute_frame(y,0x170,0,C.byref(views),g,len(g),C.byref(portable.api)):raise AssertionError('Edge-kind getter failed')
                left,right=bytearray(f.raw),bytearray(g.raw)
                if initial_kind==4:left[0xf0:0xf8]=right[0xf0:0xf8]=bytes(8)
                if left!=right:raise AssertionError('Edge-kind frame differs')
                counts['ensure_existing_and_edge_kind_frames']+=1
            compare_trees(x,y,counts,digest,residues)
            initial=rng.randbytes(0x110);f=C.create_string_buffer(initial,len(initial));g=C.create_string_buffer(initial,len(initial))
            getter(None,f)
            if not core.fs15_scene_attribute_frame(None,0,0,None,g,len(g),None) or f.raw!=g.raw:raise AssertionError('Null owner getter differs')
            setter(None,None)
            if not core.fs15_scene_attribute_frame(None,0,1,None,None,0,None):raise AssertionError('Null owner setter differs')
            counts['null_owner_frame_pairs']+=1
            # Clear and the complete matrix/TRS chain must now handle nonempty
            # maps without rejecting them or silently dropping member cleanup.
            original_roles,portable_roles=owned_roles(x),owned_roles(y)
            first_free=len(native.freed);first_release=len(portable.release_order)
            destroy(a)
            if not core.fs15_scene_destruct(b,0x170,3,0,C.byref(portable.api)):raise AssertionError('Nonempty attribute destruction failed')
            if canonical(a.raw,3)!=canonical(b.raw,3):raise AssertionError('Destructor owner record differs')
            expected_order=[original_roles[p] for p in native.freed[first_free:] if p in original_roles]
            actual_order=[portable_roles[p] for p in portable.release_order[first_release:] if p in portable_roles]
            if expected_order!=actual_order:raise AssertionError('Owned attribute/member free order differs')
            counts['complete_member_free_order_traces']+=1
            if set(native.allocations)!={read(native.base+0x1630d48),*orphaned[0]} or set(portable.blocks)!=set(orphaned[1]):raise AssertionError('Owned map cleanup has unexpected residual allocations')
            for pointer in orphaned[0]:native.callbacks[1](native.heap,0,pointer)
            for pointer in orphaned[1]:portable.release(None,pointer)
            if native.registry() or portable.registry or len(native.allocations)!=1 or portable.blocks:raise AssertionError('Fixture cleanup leaked')
            counts['nonempty_map_destructors']+=1
    paths=('src/core/scene_attributes.cpp','src/core/scene_attributes.h','src/core/scene_lifecycle.cpp','src/core/scene_lifecycle.h','src/core/scene_state.h')
    report=dict(status='passed',reference_sha256=EXPECTED_IMAGE,
        native_entries=['0x1400246f0','0x140136870','0x140136980','0x14027e1c0','0x1400e6900','0x1400e69b0','0x14016edc0','0x1401365f0'],
        rebuilt_source_sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in paths},tested_dll_sha256=hashlib.sha256(args.dll.read_bytes()).hexdigest(),
        cases=dict(counts),original_results_sha256=digest.hexdigest(),
        scope='Valid owned native-layout RB maps, short/heap byte keys, complete pointer-normalized node/value/padding topology, Lua frames, integer conversion, original getter reuse on reconstructed trees and nonempty-map cleanup/free order. Pointer residues from scalar overwrites are checked against their originating allocation before normalization. Retains String-to-scalar orphan allocations; fixture contexts reclaim those separately. Platform allocator, guards and failure cleanup are authored. No native exception/unwind, full resources/triggers, complete engine or console execution.')
    write_json(ROOT/'analysis/scene-attributes-validation.json',report);print(json.dumps(report,indent=2))


if __name__=='__main__':main()
