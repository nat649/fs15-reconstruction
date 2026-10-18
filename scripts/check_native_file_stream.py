"""Compare four recovered file operations with unchanged native GE6 routines.

Runs only in a local Windows process against the pinned analysis image. Kernel
IAT slots are rebound to real Windows APIs; no game entry point is executed.
No code bytes, original resources or per-case vectors are published.
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
from inventory import PE, ROOT
from native_gar import NativeGarCipher, DEFAULT_IMAGE, EXPECTED_IMAGE

P=C.c_void_p
U=C.c_uint32
READ=C.CFUNCTYPE(U,P,C.c_uint64,P,U,C.POINTER(U))
SEEK=C.CFUNCTYPE(U,P,C.c_uint64,C.c_int32,U)
SIZE=C.CFUNCTYPE(U,P,C.c_uint64)
CORRUPT=C.CFUNCTYPE(None,P,P,U)
class Services(C.Structure):
    _fields_=[('user',P),('read',READ),('seek',SEEK),('size',SIZE),('corrupt',CORRUPT)]


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dll',type=Path,required=True)
    args=parser.parse_args()
    core=C.CDLL(str(args.dll.resolve()))
    for name,types,result in [
        ('read',[P,C.c_size_t,P,U,P],U),('seek',[P,C.c_size_t,U,P],C.c_int),
        ('size',[P,C.c_size_t,P],U),('tell',[P,C.c_size_t,P],U)]:
        function=getattr(core,'fs15_file_'+name)
        function.argtypes=types;function.restype=result
    rng=random.Random(0xf51504f310)
    counts=Counter();digest=hashlib.sha256();sources=[]
    fixture_root=ROOT/'build/native-file-fixtures'
    fixture_root.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(dir=fixture_root) as folder, NativeGarCipher() as native:
        kernel=native.kernel
        kernel.CreateFileW.argtypes=[C.c_wchar_p,U,U,P,U,U,P];kernel.CreateFileW.restype=P
        kernel.CloseHandle.argtypes=[P];kernel.CloseHandle.restype=C.c_int
        kernel.ReadFile.argtypes=[P,P,U,C.POINTER(U),P];kernel.ReadFile.restype=C.c_int
        kernel.SetFilePointer.argtypes=[P,C.c_int32,P,U];kernel.SetFilePointer.restype=U
        kernel.GetFileSize.argtypes=[P,P];kernel.GetFileSize.restype=U
        kernel.GetLastError.argtypes=[];kernel.GetLastError.restype=U
        imports={'ReadFile','SetFilePointer','GetFileSize','GetLastError'}
        patched=set();slots={}
        for library in PE(DEFAULT_IMAGE.read_bytes()).imports():
            if library['dll'].lower()!='kernel32.dll':continue
            for row in library['functions']:
                if row['name'] not in imports:continue
                at=int(row['iat_va'],16);old=U()
                if not kernel.VirtualProtect(at,8,4,C.byref(old)):raise C.WinError(C.get_last_error())
                C.c_void_p.from_address(at).value=C.cast(getattr(kernel,row['name']),P).value
                slots[row['name']]=at
                restored=U();kernel.VirtualProtect(at,8,old.value,C.byref(restored));patched.add(row['name'])
        assert patched==imports
        callbacks=[];injected=[False]
        def read(_,handle,out,count,actual):
            if injected[0]:return 6
            ok=kernel.ReadFile(handle,out,count,actual,None)
            return 0 if ok else C.get_last_error()
        callbacks.extend([READ(read),SEEK(lambda _,h,d,o:UINT32_MAX if injected[0] else kernel.SetFilePointer(h,d,None,o)),
                          SIZE(lambda _,h:UINT32_MAX if injected[0] else kernel.GetFileSize(h,None)),CORRUPT(lambda *_:None)])
        services=Services(None,*callbacks)
        nf=lambda offset,result,*types:C.WINFUNCTYPE(result,*types)(native.base+offset)
        # The seek result is a C++ bool in AL, not the undefined upper EAX
        # bits shown as a 32-bit return by the catalogue prototype.
        original={'read':nf(0x4f310,U,P,P,U),'seek':nf(0x4f410,C.c_uint8,P,U),
                  'size':nf(0x4f460,U,P),'tell':nf(0x4f4b0,U,P)}
        UINT32_MAX=0xffffffff
        faults={
            'ReadFile':C.WINFUNCTYPE(C.c_int,P,P,U,C.POINTER(U),P)(lambda *_:0),
            'SetFilePointer':C.WINFUNCTYPE(U,P,C.c_int32,P,U)(lambda *_:UINT32_MAX),
            'GetFileSize':C.WINFUNCTYPE(U,P,P)(lambda *_:UINT32_MAX),
            'GetLastError':C.WINFUNCTYPE(U)(lambda:6)}
        def fault_mode(enable):
            injected[0]=enable
            for name,at in slots.items():
                old=U();assert kernel.VirtualProtect(at,8,4,C.byref(old))
                C.c_void_p.from_address(at).value=C.cast(faults[name] if enable else getattr(kernel,name),P).value
                restored=U();kernel.VirtualProtect(at,8,old.value,C.byref(restored))
        def compare(path,steps,faulty=False,invalid=False):
            handles=[]
            try:
                if invalid:handles=[2**64-1]*2
                else:
                    for _ in range(2):
                        value=kernel.CreateFileW(str(path),0x80000000,1,None,3,0x80,None)
                        assert value not in (None,2**64-1)
                        handles.append(value)
                payload=rng.randbytes(0x38)
                records=[C.create_string_buffer(payload,len(payload)) for _ in range(2)]
                for record,handle in zip(records,handles):struct.pack_into('<Q',record,8,handle)
                if faulty:fault_mode(True)
                for operation,arg in steps:
                    buffers=None
                    if operation=='read':
                        seed=rng.randbytes(arg+32)
                        buffers=[C.create_string_buffer(seed,len(seed)) for _ in range(2)]
                        a=original['read'](records[0],C.addressof(buffers[0])+16,arg)
                        b=core.fs15_file_read(records[1],len(payload),C.addressof(buffers[1])+16,arg,C.byref(services))
                        assert buffers[0].raw==buffers[1].raw
                        digest.update(buffers[1].raw);counts['compared_output_buffer_bytes']+=len(seed)
                    elif operation=='seek':
                        a=original['seek'](records[0],arg)
                        b=core.fs15_file_seek(records[1],len(payload),arg,C.byref(services))
                    else:
                        a=original[operation](records[0])
                        b=getattr(core,'fs15_file_'+operation)(records[1],len(payload),C.byref(services))
                    assert a==b,(operation,arg,a,b)
                    canonical=[]
                    for record in records:
                        value=bytearray(record.raw);value[8:16]=bytes(8);canonical.append(bytes(value))
                    assert canonical[0]==canonical[1],(operation,arg,'complete record mismatch')
                    digest.update(canonical[1]);digest.update(struct.pack('<I',b))
                    counts[operation]+=1;counts['complete_record_bytes']+=len(payload)
                counts['sequences']+=1
                if faulty:counts['injected_failure_sequences']+=1
            finally:
                if faulty:fault_mode(False)
                if not invalid:
                    for h in handles:kernel.CloseHandle(h)
        for length in [0,1,7,8,15,16,255,1023,1024,4097,65536]:
            path=Path(folder)/'resource space-\u00e9.dat';path.write_bytes(rng.randbytes(length))
            steps=[('size',0),('tell',0),('read',0),('read',8),('read',1024),('tell',0)]
            for position in [0,1,length,length+1,0x7fffffff,0x80000000,0xffffffff]:
                steps.extend([('seek',position),('tell',0),('read',17),('size',0)])
            for _ in range(180):
                operation=rng.choice(['read','seek','size','tell'])
                arg=rng.randrange(2049) if operation=='read' else rng.choice([0,length,length+10,rng.randrange(length+32),0x80000000,0xffffffff])
                steps.append((operation,arg))
            compare(path,steps)
            compare(path,[('read',0),('read',1),('seek',0),('size',0),('tell',0)],invalid=True)
            compare(path,[('read',0),('read',1),('seek',0),('size',0),('tell',0)],faulty=True)
        # Original bytes are read in place and remain local; compare full file
        # reads for a script, XML, font metrics and compressed menu texture.
        for name in ['recovered/lua/dataS/scripts/I18N.lua','recovered/gar/dataS/l10n_en.xml',
                     'recovered/gar/shared/font/defaultFont.font','recovered/gar/dataS2/menu/main_logo_en.dds']:
            path=ROOT/name
            if not path.is_file():raise ValueError('Missing local reference resource: '+name)
            size=path.stat().st_size
            sources.append({'path':name,'bytes':size,'sha256':hashlib.sha256(path.read_bytes()).hexdigest()})
            compare(path,[('size',0),('tell',0),('read',size),('read',1),('seek',max(0,size-7)),('read',16),('tell',0)])
    report={'status':'passed','input_image_sha256':EXPECTED_IMAGE,
            'native_entries':['0x14004f310','0x14004f410','0x14004f460','0x14004f4b0'],
            'rebuilt_source_sha256':{name:hashlib.sha256((ROOT/name).read_bytes()).hexdigest()
                                     for name in ['src/core/file_stream.cpp','src/core/file_stream.h']},
            'comparison_counts':dict(counts),'comparison_digest':digest.hexdigest(),
            'original_resource_inputs':sources,
            'scope':'Four bounded file read/seek/size/tell paths, complete records and output guards against unchanged native routines using actual Windows kernel I/O for successful operations and explicit injected ordinary-I/O failure seams. Constructors/destructors, corruption logger/global queue, full VFS and cross-process sharing semantics are outside this comparison.'}
    (ROOT/'analysis/file-stream-validation.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))


if __name__=='__main__':main()
