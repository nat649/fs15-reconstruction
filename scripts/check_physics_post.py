"""Native wheel readback/contact transitions/post-step differential fixtures.

Original code stays local. Solver/controller creation and native cancellation
are excluded; callbacks and caller-owned graphs provide inspected boundaries.
"""
import argparse
from collections import Counter
import ctypes as C
import hashlib
import json
import math
import random
import struct
from inventory import ROOT
from native_gar import NativeGarCipher,EXPECTED_IMAGE
P=C.c_void_p;N=C.c_size_t;U=C.c_uint32;F=C.c_float
WAIT=C.CFUNCTYPE(U,P,N,U,C.c_uint8);OBJECT=C.CFUNCTYPE(C.c_int,P,N)
FRAME=C.CFUNCTYPE(C.c_int,P,P,C.c_int32,P);ATAN=C.CFUNCTYPE(F,P,F)
ALLOC=C.CFUNCTYPE(P,P,N,N);FREE=C.CFUNCTYPE(None,P,P)
OBS=C.CFUNCTYPE(C.c_int,P,N,N,N,N,U,U)
PENDING=C.CFUNCTYPE(C.c_int,P,N,U,F,P);CANCEL=C.CFUNCTYPE(C.c_int,P,P,U)
LOOKUP=C.CFUNCTYPE(N,P,N,N,N);GROUPS=C.CFUNCTYPE(C.c_int,P,N,P)
class Services(C.Structure):_fields_=[('user',P),('wait',WAIT),('object',OBJECT),('frame',FRAME),('atan',ATAN),('allocate',ALLOC),('release',FREE),('observer',OBS),('pending',PENDING),('cancel',CANCEL),('part_limit',N),('lookup',LOOKUP),('groups',GROUPS),('contact_row_limit',N)]
def put(p,o,v,fmt='Q'):struct.pack_into('<'+fmt,p,o,v)
def read(p,o,fmt='Q'):return struct.unpack_from('<'+fmt,p,o)[0]
def ptr(p):return C.addressof(p)
def main():
    ap=argparse.ArgumentParser();ap.add_argument('--dll',required=True);a=ap.parse_args();core=C.CDLL(str((ROOT/a.dll).resolve()))
    for name,args in [('wheel_refresh',[P,N,P,P]),('wheels',[P,N,P]),('contact_notify',[P,U,U,P,N,P]),('contacts_finish',[P,N,P,N,P]),('contacts_collect',[P,N,N,P]),('post_step',[P,N,P])]:
        fn=getattr(core,'fs15_physics_'+name);fn.argtypes=args;fn.restype=C.c_int
    rng=random.Random(0xf515c7b20);counts=Counter();digest=hashlib.sha256();max_error=0.
    with NativeGarCipher(math_helpers=True) as native:
        nf=lambda off,result,*args:C.WINFUNCTYPE(result,*args)(native.base+off)
        n_wheel=nf(0xca150,None,P,P);n_wheels=nf(0xca080,None,P);n_notify=nf(0x145190,None,P,U,U,P);n_finish=nf(0x144ef0,None,P,P);n_post=nf(0xc7b20,None,P)
        n_atan=nf(0x456b20,F,F)
        phase=[0];events=[[],[]];kept=[];allocated=[[],[]];contacts=[];managers=[];mode=['notify'];handled=[1]
        def canonical_header(data):
            value=bytearray(data)
            for off in (0x18,0x28,0x38):
                if read(value,off):put(value,off,off)
            return bytes(value)
        def snapshot():
            if mode[0]=='notify':return b''
            value=canonical_header(contacts[phase[0]].raw)
            if mode[0]=='post':
                m=bytearray(managers[phase[0]].raw);m[0x48:0xa8]=value[:0x60];value=bytes(m)
            return value
        def observer(listener,first,second,target,event,part):
            listener=listener_ids[listener];events[phase[0]].append(('observe',listener,first,second,target,event,part,snapshot()))
            return int(listener==handled[0])
        n_observe=C.WINFUNCTYPE(C.c_uint8,P,P,P,P,U,U)(observer)
        table=(N*2)(0,C.cast(n_observe,P).value);listeners=[(N*1)(ptr(table)) for _ in range(3)];listener_ids={ptr(p):i+1 for i,p in enumerate(listeners)}
        listener_array=(N*3)(*[ptr(p) for p in listeners]);listener_vector=(N*3)(ptr(listener_array),ptr(listener_array)+24,ptr(listener_array)+24)
        def allocate(size,alignment):
            if alignment!=16:raise AssertionError('Alignment changed')
            result=C.create_string_buffer(bytes([0xa5])*size,size);allocated[phase[0]].append(result);return ptr(result)
        def release(_):pass # Owned ctypes buffers stay alive through comparison.
        n_alloc=C.WINFUNCTYPE(P,N,N)(allocate);n_free=C.WINFUNCTYPE(None,P)(release)
        for address,callback in [(0x140575110,n_alloc),(0x140575118,n_free)]:
            old=U();native.kernel.VirtualProtect(address,8,4,C.byref(old));C.c_void_p.from_address(address).value=C.cast(callback,P).value;restored=U();native.kernel.VirtualProtect(address,8,old.value,C.byref(restored))
        def wait(token,timeout,alertable):events[phase[0]].append(('wait',token,timeout,alertable,snapshot()));return 0
        n_wait=C.WINFUNCTYPE(U,P,U,U)(wait)
        old=U();native.kernel.VirtualProtect(native.base+0x472248,8,4,C.byref(old));C.c_void_p.from_address(native.base+0x472248).value=C.cast(n_wait,P).value;restored=U();native.kernel.VirtualProtect(native.base+0x472248,8,old.value,C.byref(restored))
        matrix=C.create_string_buffer(64)
        def frame(_,wheel_set,index,out):
            if index!=0:raise AssertionError('Unexpected frame index')
            C.memmove(out,matrix,64);return 1
        def pending(_,callback,owner,item,cancel):
            events[1].append(('pending',owner,item,snapshot()));C.c_uint8.from_address(cancel).value=int(owner%2==0);return 1
        callbacks=[WAIT(lambda _,*args:wait(*args)),OBJECT(lambda *_:1),FRAME(frame),ATAN(lambda _,v:n_atan(v)),ALLOC(lambda _,n,a:allocate(n,a)),FREE(lambda _,p:release(p)),OBS(lambda _,*args:observer(*args)),PENDING(pending),CANCEL(lambda *_:1)]
        services=Services(None,*callbacks,32)
        def endpoint(index,flags):
            metadata=C.create_string_buffer(0x20);body=C.create_string_buffer(0x118);handle=C.create_string_buffer(0x20);parts=C.create_string_buffer(32*16)
            put(metadata,0,flags,'I');put(metadata,8,0x1000+index);put(metadata,0x10,ptr(parts));put(metadata,0x18,index+700,'I')
            for bit in range(32):put(parts,bit*16,0x10000+index*100+bit)
            put(body,0x110,ptr(metadata));put(handle,0,ptr(body));put(handle,0x18,index,'I');kept.extend([metadata,body,handle,parts]);return handle
        # Full notify policy and per-part short-circuit order.
        mode[0]='notify'
        for i in range(1536):
            first=endpoint(1,rng.choice([0,0x40,0x100,0x140]));second=endpoint(2,rng.choice([0,8,0x80,0x88,0x100]));pair=(N*2)(ptr(first),ptr(second))
            mask=rng.choice([0,1,3,0x80000000,rng.getrandbits(32)]);event=rng.choice([1,2,4]);handled[0]=rng.randrange(0,4);events[:]=[[],[]]
            phase[0]=0;n_notify(pair,mask,event,listener_vector);phase[0]=1
            if not core.fs15_physics_contact_notify(pair,mask,event,listener_array,3,C.byref(services)):raise AssertionError('Notify rejected')
            if events[0]!=events[1]:raise AssertionError('Notify trace mismatch')
            counts['notify_states']+=1;counts['notify_callback_calls']+=len(events[0]);digest.update(repr(events[0]).encode());kept.clear()
        def cache_fixture(count,growth):
            nonlocal contacts
            shapes=[endpoint(i+1,rng.choice([0,8,0x40,0x80,0xc0])) for i in range(10)]
            rows=[];buckets=[-1]*16
            for i in range(count):
                aa,bb=1,i+2;h=(bb<<16)|aa;h=(h+~(h<<15))&0xffffffff;h=((h^(h>>10))*9)&0xffffffff;h^=h>>6;h=(h+~(h<<11))&0xffffffff;h^=h>>16;bucket=h&15
                rows.append(struct.pack('<QQIIiI',ptr(shapes[0]),ptr(shapes[i+1]),rng.getrandbits(8),rng.getrandbits(32),buckets[bucket],rng.getrandbits(32)));buckets[bucket]=i
            prevcount=0 if growth else rng.randrange(0,8)
            prev=[]
            for j in range(prevcount):prev.append(struct.pack('<QQII',ptr(shapes[0]),ptr(shapes[j+1]),rng.getrandbits(8),rng.getrandbits(32)))
            current=[C.create_string_buffer(b''.join(rows)+bytes([0x5a])*(16-count)*32,512) for _ in range(2)]
            previous=[C.create_string_buffer(b''.join(prev)+bytes([0xa5])*(16-prevcount)*24,384) for _ in range(2)]
            bucket_buffer=(C.c_int32*16)(*buckets);contacts=[C.create_string_buffer(rng.randbytes(0x60),0x60) for _ in range(2)];C.memmove(contacts[1],contacts[0],0x60)
            for side in range(2):
                c=contacts[side];put(c,0xc,count,'I');put(c,0x10,16,'I');put(c,0x18,ptr(current[side]));put(c,0x28,0 if growth else ptr(previous[side]));put(c,0x30,prevcount,'I');put(c,0x34,0 if growth else 16,'I');put(c,0x38,ptr(bucket_buffer))
            kept.extend([*current,*previous,bucket_buffer,*contacts]);events[:]=[[],[]];allocated[:]=[[],[]]
            return current
        def compare_cache(current):
            if canonical_header(contacts[0].raw)!=canonical_header(contacts[1].raw):raise AssertionError('Contact header mismatch')
            if current[0].raw!=current[1].raw:raise AssertionError('Contact current rows mismatch')
            if events[0]!=events[1]:
                for k,(x,y) in enumerate(zip(events[0],events[1])):
                    if x!=y:
                        delta=next((j for j,(u,v) in enumerate(zip(x[-1],y[-1])) if u!=v),None)
                        raise AssertionError(f'Trace mismatch at {k}: {x[:-1]} vs {y[:-1]}, state offset {delta}')
                raise AssertionError('Trace lengths differ')
            size=read(contacts[0],0x34,'I')*24
            if size and C.string_at(read(contacts[0],0x28),size)!=C.string_at(read(contacts[1],0x28),size):raise AssertionError('Previous pair buffer mismatch')
            digest.update(canonical_header(contacts[0].raw));counts['contact_cache_callback_calls']+=sum(event[0]=='observe' for event in events[0])
        mode[0]='cache'
        for i in range(768):
            current=cache_fixture(i%9,i%5==0);phase[0]=0;n_finish(contacts[0],listener_vector);phase[0]=1
            if not core.fs15_physics_contacts_finish(contacts[1],0x60,listener_array,3,C.byref(services)):raise AssertionError('Contacts finish rejected')
            compare_cache(current);counts['contact_lifetimes']+=1;kept.clear()
        # Recover nonempty native contact collection through virtual backend
        # lookup/group slots, retaining raw selector and scratch-vector layout.
        n_collect=nf(0x144ce0,None,P,P);collector_events=[[],[]];active_body=[0];group_buffers=[];orientations=[]
        def collect_snapshot():return canonical_header(contacts[phase[0]].raw),C.string_at(read(contacts[phase[0]],0x18),512)
        def lookup(world,first,second):
            ids=[read(C.string_at(token,0x20),0x18,'I') for token in (first,second)]
            collector_events[phase[0]].append(('lookup',*ids,*collect_snapshot()))
            active_body[0]=read(C.string_at(second,0x20),0)
            return 0 if ids[1]%5==0 else ptr(empty_wrapper) if ids[1]%5==1 else ptr(wrapper)
        def groups(interface,vector):
            collector_events[phase[0]].append(('groups',*collect_snapshot()))
            for group,flipped in zip(group_buffers,orientations):put(group,0x310,0 if flipped else active_body[0])
            C.memmove(vector+4,struct.pack('<II',len(group_buffers),3),8);C.memmove(vector+0x10,struct.pack('<Q',ptr(group_items)),8);return 1
        native_lookup=C.WINFUNCTYPE(P,P,P,P)(lookup);native_groups=C.WINFUNCTYPE(None,P,P)(lambda interface,vector:groups(interface,vector))
        world_table=(N*13)();world_table[12]=C.cast(native_lookup,P).value;world_object=(N*1)(ptr(world_table))
        group_table=(N*4)();group_table[3]=C.cast(native_groups,P).value;group_object=(N*1)(ptr(group_table))
        wrapper=C.create_string_buffer(0x18);put(wrapper,0x10,ptr(group_object));empty_wrapper=C.create_string_buffer(0x18)
        services.lookup=LOOKUP(lambda _,world,a,b:lookup(world,a,b));services.groups=GROUPS(lambda _,interface,vector:groups(interface,vector));services.contact_row_limit=4
        for i in range(512):
            current=cache_fixture(i%9,False);group_buffers=[C.create_string_buffer(0x330) for _ in range(i%4)];orientations=[rng.choice([False,True]) for _ in group_buffers]
            for group in group_buffers:
                put(group,0x320,rng.choice([0,1,2,4]),'I')
                for k in range(4):put(group,0x78+k*0xc0,rng.randrange(256),'I');put(group,0x7c+k*0xc0,rng.randrange(256),'I')
            group_items=(N*max(1,len(group_buffers)))(*map(ptr,group_buffers))
            for c in contacts:put(c,0x44,0,'I');put(c,0x48,3,'I');put(c,0x50,ptr(group_items))
            collector_events[:]=[[],[]];phase[0]=0;n_collect(contacts[0],world_object);phase[0]=1
            if not core.fs15_physics_contacts_collect(contacts[1],0x60,ptr(world_object),C.byref(services)):raise AssertionError('Native contact collection rejected')
            compare_cache(current)
            if collector_events[0]!=collector_events[1]:raise AssertionError('Contact collection backend traces differ')
            counts['contact_collection_states']+=1;counts['nonempty_contact_collection_states']+=int(i%9!=0);counts['contact_collection_backend_calls']+=len(collector_events[0]);kept.clear()
        # Original wheel refresh resolves its actor frame through the original
        # nested owner pointers; portable transport resolves that same matrix.
        actor=C.create_string_buffer(0x240);frame_owner=C.create_string_buffer(0xa0);wheel_set=C.create_string_buffer(0x50)
        put(actor,0x238,ptr(frame_owner));put(wheel_set,8,ptr(actor))
        contact_body=C.create_string_buffer(0x118);metadata=C.create_string_buffer(0x10);owner=C.create_string_buffer(0x20);put(contact_body,0x110,ptr(metadata));put(metadata,8,ptr(owner));put(owner,0x18,91,'I')
        mode[0]='notify'
        for i in range(1536):
            m=[rng.uniform(-2,2) for _ in range(16)];m[15]=1;C.memmove(matrix,struct.pack('<16f',*m),64);C.memmove(ptr(frame_owner)+0x60,matrix,64)
            payload=bytearray(rng.randbytes(0x160));put(payload,0x28,0 if i%4==0 else ptr(contact_body));put(metadata,0,rng.choice([0,1,0x101]),'I');put(payload,0x138,0,'i')
            for off in [0xa0,0xa4,0xa8,0xb0,0xb4,0xb8,0x148,0x150]:put(payload,off,rng.uniform(-20,20),'f')
            records=[C.create_string_buffer(bytes(payload),len(payload)) for _ in range(2)]
            n_wheel(records[0],wheel_set)
            services.atan=callbacks[3] if i%2==0 else ATAN()
            if not core.fs15_physics_wheel_refresh(records[1],0x160,wheel_set,C.byref(services)):raise AssertionError('Wheel refresh rejected')
            aa,bb=[bytearray(p.raw) for p in records];u,v=read(aa,0x98,'f'),read(bb,0x98,'f');error=abs(u-v)/max(1.,abs(u));max_error=max(max_error,error)
            if error>2e-5:raise AssertionError('Wheel angle math differs')
            aa[0x98:0x9c]=bb[0x98:0x9c]=bytes(4)
            if aa!=bb:raise AssertionError('Wheel refresh differs at '+hex(next(j for j,(x,y) in enumerate(zip(aa,bb)) if x!=y)))
            digest.update(records[0].raw[:0x28]+records[0].raw[0x30:]);counts['wheel_states']+=1
        services.atan=callbacks[3]
        for i in range(128):
            registry_payload=bytearray(bytes(payload)*3)
            for j in range(3):put(registry_payload,j*0x160+0xa0,rng.uniform(-50,50),'f');put(registry_payload,j*0x160+0x108,rng.uniform(-20,20),'f')
            records=[C.create_string_buffer(bytes(registry_payload),0x420) for _ in range(2)];sets=[C.create_string_buffer(wheel_set.raw,0x50) for _ in range(2)];heads=[];nodes=[];states=[]
            for side in range(2):
                put(sets[side],0x3c,1+i%3,'i');put(sets[side],0x48,ptr(records[side]));head=C.create_string_buffer(0x30);node=C.create_string_buffer(0x30);head[0x29]=b'\x01';put(head,0,ptr(node));put(head,8,ptr(node));put(head,16,ptr(node));put(node,0,ptr(head));put(node,8,ptr(head));put(node,16,ptr(head));put(node,0x20,ptr(sets[side]));state=C.create_string_buffer(0x490);put(state,0x250,ptr(head));heads.append(head);nodes.append(node);states.append(state)
            n_wheels(states[0])
            if not core.fs15_physics_wheels(states[1],0x490,C.byref(services)) or records[0].raw!=records[1].raw:raise AssertionError('Wheel registry refresh differs')
            counts['nonempty_wheel_registry_states']+=1
        # Full post-step control flow with empty controller/wheel registries,
        # nonempty contact history and pending callback/removal lists.
        n_pending=C.WINFUNCTYPE(C.c_uint8,P,U,F)(lambda _,owner,item:(events[0].append(('pending',owner,item,snapshot())),int(owner%2==0))[1]);pending_table=(N*2)(0,C.cast(n_pending,P).value);pending_object=(N*1)(ptr(pending_table))
        mode[0]='post'
        for i in range(256):
            current=cache_fixture(i%9,False);managers=[C.create_string_buffer(0x490) for _ in range(2)]
            empty=C.create_string_buffer(0x40);put(empty,0,ptr(empty));put(empty,8,ptr(empty));put(empty,16,ptr(empty));empty[0x29]=empty[0x39]=b'\x01'
            jobs=[]
            for j in range(i%5):
                job=C.create_string_buffer(0x20);own=C.create_string_buffer(0x18);put(own,4,100+j,'I');put(own,0x10,ptr(pending_object));put(job,0x10,ptr(own));put(job,0x18,0.25+j,'f');job[0x1c]=bytes([j%2]);jobs.append(job);kept.extend([job,own])
            arrays=[(N*len(jobs))(*map(ptr,jobs)) for _ in range(2)]
            for side in range(2):
                C.memmove(ptr(managers[side])+0x48,contacts[side],0x60);put(managers[side],0x198,ptr(listener_array));put(managers[side],0x1a0,ptr(listener_array)+24);put(managers[side],0x250,ptr(empty));put(managers[side],0x278,ptr(empty));put(managers[side],0x3a8,ptr(empty));put(managers[side],0x320,ptr(arrays[side]));put(managers[side],0x328,len(jobs),'I');managers[side][0x448]=b'\x01';managers[side][0x48c]=bytes([i%2]);put(managers[side],0x480,32)
            contacts=[(C.c_char*0x60).from_address(ptr(m)+0x48) for m in managers]
            phase[0]=0;n_post(managers[0]);phase[0]=1
            if not core.fs15_physics_post_step(managers[1],0x490,C.byref(services)):raise AssertionError('Post-step rejected')
            # Pending array identities differ between sides; normalize snapshots.
            for side in range(2):
                normalized=[]
                for event in events[side]:
                    data=bytearray(event[-1]);put(data,0x320,0x320);normalized.append((*event[:-1],bytes(data)))
                events[side]=normalized
            compare_cache(current)
            x,y=[bytearray(m.raw) for m in managers]
            for value in (x,y):value[0x48:0xa8]=canonical_header(value[0x48:0xa8]);put(value,0x320,0x320)
            if x!=y or arrays[0][:]!=arrays[1][:]:raise AssertionError('Manager post-step/pending compaction differs')
            counts['post_step_states']+=1;kept.clear()
    report={'status':'passed','input_image_sha256':EXPECTED_IMAGE,'native_entries':['0x1400ca150','0x1400ca080','0x140144ce0','0x140145190','0x140144ef0','0x1400c7b20'],
        'rebuilt_source_sha256':{p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in ['src/core/physics_post.cpp','src/core/physics_post.h']},'comparison_counts':dict(counts),'comparison_digest':digest.hexdigest(),
        'maximum_scaled_platform_atan_error':max_error,
        'scope':'Original wheel readback, nonempty wheel registry traversal and contact enter/leave/stay logic; original full post-step control flow with empty controller/wheel registries, nonempty contact cache, pending removals, float-ABI callbacks and cancellation against an empty native registry. Successful native deletion, controller solver, full scene creation and GPU backend remain external/unported. Portable fixtures separately cover connected nonempty registries and cancellation-service routing.'}
    (ROOT/'analysis/physics-post-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
if __name__=='__main__':main()
