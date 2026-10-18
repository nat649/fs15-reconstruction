"""Compare native GE6 memory-stream operations using local borrowed buffers.

No original factory/destructor or PE entry point is executed. Native read and
query bodies execute unchanged; originals and detailed vectors remain local.
"""
import argparse
from collections import Counter
import ctypes as C
import csv
import hashlib
import json
from pathlib import Path
import random
import struct
from inventory import ROOT, PE
from native_gar import NativeGarCipher, DEFAULT_IMAGE, EXPECTED_IMAGE

P=C.c_void_p;U=C.c_uint32;N=C.c_size_t


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--dll',type=Path,required=True);args=parser.parse_args()
    core=C.CDLL(str(args.dll.resolve()));rng=random.Random(0xf51506bcf0);counts=Counter();digest=hashlib.sha256();inputs=[]
    functions={'valid':(0x6bc00,C.c_uint8,[P],C.c_int,[P,N]),
               'read':(0x6bcf0,U,[P,P,U],U,[P,N,N,P,N,U]),
               'seek':(0x6bd50,C.c_uint8,[P,U],C.c_int,[P,N,U]),
               'size':(0x5f670,U,[P],U,[P,N]),'tell':(0x6bc10,U,[P],U,[P,N]),
               'eof':(0x6bd60,C.c_uint8,[P],C.c_int,[P,N])}
    for name,(_,_,_,result,types) in functions.items():
        fn=getattr(core,'fs15_memory_file_'+name);fn.argtypes=types;fn.restype=result
    # Validate both explicit tables, retaining the previous scene audit.
    image=DEFAULT_IMAGE.read_bytes();pe=PE(image)
    with (ROOT/'analysis/vtable-discovery/slots.csv').open() as stream:
        rows=[r for r in csv.DictReader(stream) if r['table'] in ('1404bbb38','1404d9ff8')]
    assert len(rows)==24 and len({(r['table'],r['slot']) for r in rows})==24
    for row in rows:
        assert struct.unpack_from('<Q',image,pe.offset(int(row['table'],16)-pe.base+int(row['slot'])))[0]==int(row['target'],16)
        assert row['target']==row['containing_function']
    newly=sorted({r['target'] for r in rows if r['status']=='created_from_explicit_vtable'})
    assert len(newly)==6
    with NativeGarCipher() as native:
        original={name:C.WINFUNCTYPE(result,*types)(native.base+at) for name,(at,result,types,_,_) in functions.items()}
        def compare(data,operations,invalid=False):
            seed=rng.randbytes(16)+data+rng.randbytes(16)
            buffers=[C.create_string_buffer(seed,len(seed)) for _ in range(2)]
            value=bytearray(rng.randbytes(0x20));struct.pack_into('<II',value,0x14,len(data),0)
            records=[C.create_string_buffer(bytes(value),len(value)) for _ in range(2)]
            for record,buf in zip(records,buffers):struct.pack_into('<Q',record,8,0 if invalid else C.addressof(buf)+16)
            for operation,arg in operations:
                if operation=='read':
                    raw=rng.randbytes(arg+32);outputs=[C.create_string_buffer(raw,len(raw)) for _ in range(2)]
                    a=original['read'](records[0],C.addressof(outputs[0])+16,arg)
                    b=core.fs15_memory_file_read(records[1],0x20,len(data),C.addressof(outputs[1])+16,arg,arg)
                    assert outputs[0].raw==outputs[1].raw,('output',len(data),arg)
                    digest.update(outputs[1].raw);counts['output_buffer_bytes']+=len(raw)
                elif operation=='seek':
                    a=original['seek'](records[0],arg);b=core.fs15_memory_file_seek(records[1],0x20,arg)
                else:
                    a=original[operation](records[0]);b=getattr(core,'fs15_memory_file_'+operation)(records[1],0x20)
                assert a==b,(operation,len(data),arg,a,b)
                states=[]
                for record in records:
                    state=bytearray(record.raw);state[8:16]=bytes(8);states.append(bytes(state))
                assert states[0]==states[1],(operation,arg,'complete record')
                digest.update(states[1]);digest.update(struct.pack('<I',b));counts[operation]+=1;counts['complete_record_bytes']+=len(value)
            assert all(buf.raw==seed for buf in buffers),'Borrowed source was modified'
            counts['sequences']+=1
            if invalid:counts['null_data_sequences']+=1
        for length in [0,1,7,8,15,16,255,1024,32768]:
            for _ in range(25):
                data=rng.randbytes(length)
                operations=[('valid',0),('size',0),('tell',0),('eof',0),('read',0),('read',length+1),
                            ('eof',0),('seek',UINT32_MAX),('tell',0),('read',1),('seek',0),('read',1)]
                for _ in range(45):
                    op=rng.choice(list(functions))
                    arg=rng.randrange(4097) if op=='read' else rng.choice([0,1,length,length+1,0x80000000,UINT32_MAX])
                    operations.append((op,arg))
                compare(data,operations)
            compare(data,operations,invalid=True)
        for name in ['recovered/lua/dataS/scripts/I18N.lua','recovered/gar/dataS/l10n_en.xml',
                     'recovered/gar/shared/font/defaultFont.font','recovered/gar/dataS2/menu/main_logo_en.dds']:
            data=(ROOT/name).read_bytes();inputs.append({'path':name,'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()})
            compare(data,[('valid',0),('size',0),('read',len(data)),('eof',0),('seek',0),('read',31),('tell',0)])
    report={'status':'passed','input_image_sha256':EXPECTED_IMAGE,
            'native_entries':['0x'+format(at+0x140000000,'x') for at,_,_,_,_ in functions.values()],
            'rebuilt_source_sha256':{name:hashlib.sha256((ROOT/name).read_bytes()).hexdigest() for name in ['src/core/memory_file.cpp','src/core/memory_file.h']},
            'comparison_counts':dict(counts),'comparison_digest':digest.hexdigest(),'original_resource_inputs':inputs,
            'explicit_memory_vtables':{'tables':2,'slots':24,'newly_catalogued_functions':len(newly),'new_entries':['0x'+x for x in newly],
                'existing_function_bodies_split':False,'discovery_csv_sha256':hashlib.sha256((ROOT/'analysis/vtable-discovery/slots.csv').read_bytes()).hexdigest()},
            'scope':'Six bounded memory-stream valid/read/seek/size/tell/EOF paths against unchanged native routines, complete records, guarded outputs and immutable borrowed sources. Native factories, ownership transfer/free, archive decryption and full loader startup remain outside this comparison. Authored capacity/state checks exclude original read overflow and invalid-span behavior.'}
    (ROOT/'analysis/memory-file-validation.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))


UINT32_MAX=0xffffffff
if __name__=='__main__':main()
