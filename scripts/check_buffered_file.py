"""Local differential oracle for GE6's 32 KiB default-disk buffered callbacks.

Original callback bodies and default disk methods execute unchanged. Four OS
IAT slots are explicit tracing/failure seams. No game entry point is run and
original bytes/vectors remain local.
"""
import argparse
from collections import Counter
import ctypes as C
import hashlib
import json
from pathlib import Path
import random
import struct
import tempfile
from inventory import ROOT, PE
from native_gar import NativeGarCipher, DEFAULT_IMAGE, EXPECTED_IMAGE
from check_native_file_stream import Services, READ, SEEK, SIZE, CORRUPT

P=C.c_void_p;U=C.c_uint32;N=C.c_size_t
RECORD=0x8058


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--dll',type=Path,required=True);args=parser.parse_args()
    core=C.CDLL(str(args.dll.resolve()))
    core.fs15_buffered_file_read.argtypes=[P,N,P,N,U,P];core.fs15_buffered_file_read.restype=U
    core.fs15_buffered_file_seek.argtypes=[P,N,U,C.c_int,P];core.fs15_buffered_file_seek.restype=U
    core.fs15_buffered_file_status.argtypes=[P,N];core.fs15_buffered_file_status.restype=U
    rng=random.Random(0xf5152e86b0);counts=Counter();digest=hashlib.sha256();inputs=[]
    temporary=ROOT/'build/buffered-file-fixtures';temporary.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(dir=temporary) as folder,NativeGarCipher() as native:
        k=native.kernel
        k.CreateFileW.argtypes=[C.c_wchar_p,U,U,P,U,U,P];k.CreateFileW.restype=P
        k.CloseHandle.argtypes=[P];k.CloseHandle.restype=C.c_int
        k.ReadFile.argtypes=[P,P,U,C.POINTER(U),P];k.ReadFile.restype=C.c_int
        k.SetFilePointer.argtypes=[P,C.c_int32,P,U];k.SetFilePointer.restype=U
        k.GetFileSize.argtypes=[P,P];k.GetFileSize.restype=U
        traces=[[],[]];reads=[0,0];policy={'fail_after':None,'short':None}
        def read(side,h,out,count,actual):
            fail=policy['fail_after'] is not None and reads[side]>=policy['fail_after'];reads[side]+=1
            if fail:actual[0]=0;traces[side].append(('read',count,0,6));return False
            request=min(count,policy['short']) if policy['short'] is not None else count
            ok=k.ReadFile(h,out,request,actual,None)
            if not ok:raise RuntimeError('Unexpected real I/O failure')
            traces[side].append(('read',count,actual[0],0));return True
        def seek(side,h,d,o):
            result=k.SetFilePointer(h,d,None,o);traces[side].append(('seek',d,o,result));return result
        def size(side,h):
            result=k.GetFileSize(h,None);traces[side].append(('size',result));return result
        native_callbacks={
            'ReadFile':C.WINFUNCTYPE(C.c_int,P,P,U,C.POINTER(U),P)(lambda h,o,n,a,_:read(0,h,o,n,a)),
            'SetFilePointer':C.WINFUNCTYPE(U,P,C.c_int32,P,U)(lambda h,d,_,o:seek(0,h,d,o)),
            'GetFileSize':C.WINFUNCTYPE(U,P,P)(lambda h,_:size(0,h)),
            'GetLastError':C.WINFUNCTYPE(U)(lambda:6)}
        patched=set()
        for library in PE(DEFAULT_IMAGE.read_bytes()).imports():
            if library['dll'].lower()!='kernel32.dll':continue
            for row in library['functions']:
                name=row['name']
                if name not in native_callbacks:continue
                at=int(row['iat_va'],16);old=U();assert k.VirtualProtect(at,8,4,C.byref(old))
                C.c_void_p.from_address(at).value=C.cast(native_callbacks[name],P).value
                restored=U();assert k.VirtualProtect(at,8,old.value,C.byref(restored));patched.add(name)
        assert patched==set(native_callbacks)
        callbacks=[READ(lambda _,h,o,n,a:0 if read(1,h,o,n,a) else 6),
                   SEEK(lambda _,h,d,o:seek(1,h,d,o)),SIZE(lambda _,h:size(1,h)),CORRUPT(lambda *_:None)]
        services=Services(None,*callbacks)
        nf=lambda at,result,*types:C.WINFUNCTYPE(result,*types)(native.base+at)
        original={'read':nf(0x2e86b0,U,P,P,P,U),'seek':nf(0x2e8830,U,P,P,U,C.c_int),'status':nf(0x2e8930,U,P,P)}
        def compare(path,operations,invalid=False,unknown=False,fail_after=None,short=None):
            handles=[];policy.update(fail_after=fail_after,short=short);reads[:]=[0,0]
            try:
                for _ in range(2):
                    h=2**64-1 if invalid else k.CreateFileW(str(path),0x80000000,1,None,3,0x80,None)
                    assert invalid or h not in (None,2**64-1);handles.append(h)
                seed=bytearray(rng.randbytes(RECORD));struct.pack_into('<Q',seed,0,native.base+0x4b8d18)
                seed[0x10]=0;struct.pack_into('<III',seed,0x8048,0,0,0xffffffff if unknown else 0);seed[0x8054]=0
                records=[C.create_string_buffer(bytes(seed),len(seed)) for _ in range(2)]
                for record,h in zip(records,handles):struct.pack_into('<Q',record,8,h)
                for operation,arg in operations:
                    traces[0].clear();traces[1].clear()
                    if operation=='read':
                        out=rng.randbytes(arg+32);outputs=[C.create_string_buffer(out,len(out)) for _ in range(2)]
                        a=original['read'](None,records[0],C.addressof(outputs[0])+16,arg)
                        b=core.fs15_buffered_file_read(records[1],RECORD,C.addressof(outputs[1])+16,arg,arg,C.byref(services))
                        assert outputs[0].raw==outputs[1].raw,('output',arg)
                        digest.update(outputs[1].raw);counts['output_buffer_bytes']+=len(out)
                    elif operation=='seek':
                        offset,origin=arg
                        a=original['seek'](None,records[0],offset,origin)
                        b=core.fs15_buffered_file_seek(records[1],RECORD,offset,origin,C.byref(services))
                    else:
                        a=original['status'](None,records[0]);b=core.fs15_buffered_file_status(records[1],RECORD)
                    assert a==b,(operation,arg,a,b)
                    assert traces[0]==traces[1],(operation,arg,traces)
                    states=[]
                    for record in records:
                        state=bytearray(record.raw);state[8:16]=bytes(8);states.append(bytes(state))
                    assert states[0]==states[1],(operation,arg,'complete record')
                    digest.update(states[1]);digest.update(struct.pack('<I',b));counts[operation]+=1
                    counts['complete_record_bytes']+=RECORD;counts['os_events']+=len(traces[0])
                counts['sequences']+=1
                if fail_after is not None:counts['injected_failure_sequences']+=1
                if short is not None:counts['injected_short_read_sequences']+=1
            finally:
                if not invalid:
                    for h in handles:assert k.CloseHandle(h)
        for length in [0,1,7,32767,32768,32769,65536,65549,100001]:
            path=Path(folder)/'buffer space-\u00e9.dat';path.write_bytes(rng.randbytes(length))
            operations=[('status',0),('read',1),('seek',(0,0)),('read',32768),('seek',(32767,0)),
                        ('read',5),('seek',(0,1)),('seek',(0,2)),('read',10),('seek',(7,2)),('read',20),
                        ('seek',(0x80000000,0)),('read',5),('seek',(0,7)),('status',0)]
            for _ in range(200):
                op=rng.choice(['read','seek','status'])
                arg=rng.randrange(70001) if op=='read' else (rng.choice([0,1,32767,32768,length,length+1,0xffffffff]),rng.choice([0,1,2,7]))
                operations.append((op,arg))
            compare(path,operations)
            compare(path,operations[:15],unknown=True)
            compare(path,[('status',0),('read',10),('status',0),('seek',(0,0)),('status',0)],invalid=True)
        path=Path(folder)/'large.dat';path.write_bytes(rng.randbytes(100001))
        for failed in [0,1,2]:
            compare(path,[('read',99999),('status',0),('seek',(7,0)),('read',13),('status',0)],fail_after=failed)
        compare(path,[('read',1000),('read',10),('seek',(0,0)),('read',20),('status',0)],short=5)
        for name in ['recovered/lua/dataS/scripts/I18N.lua','recovered/gar/dataS/l10n_en.xml',
                     'recovered/gar/shared/font/defaultFont.font','recovered/gar/dataS2/menu/main_logo_en.dds']:
            path=ROOT/name;data=path.read_bytes()
            inputs.append({'path':name,'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()})
            compare(path,[('read',len(data)),('status',0),('seek',(0,0)),('read',31),('seek',(7,2)),('read',32),('status',0)])
        assert original['read'](None,None,None,1)==0 and original['seek'](None,None,0,0)==0 and original['status'](None,None)==0xffffffff
    report={'status':'passed','input_image_sha256':EXPECTED_IMAGE,
            'native_entries':['0x1402e86b0','0x1402e8830','0x1402e8930'],
            'rebuilt_source_sha256':{name:hashlib.sha256((ROOT/name).read_bytes()).hexdigest() for name in ['src/core/buffered_file.cpp','src/core/buffered_file.h']},
            'comparison_counts':dict(counts),'comparison_digest':digest.hexdigest(),'original_resource_inputs':inputs,
            'scope':'Three bounded native buffered default-disk read/seek/status paths; complete records, guarded outputs and ordered OS event traces against original native callbacks and disk methods. Original open/close allocation, write callbacks, arbitrary derived vtables, corruption diagnostics and full VFS are outside this comparison.'}
    (ROOT/'analysis/buffered-file-validation.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))


if __name__=='__main__':main()
