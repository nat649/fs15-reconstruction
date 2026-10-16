"""Local constructor/destructor reference with explicit private-heap/OS seams.

Uses the pinned native image in this isolated process only. No executable code
is patched. Five inspected kernel imports are rebound; original CRT allocation
and original registry/tree code execute on an authored private heap. Singleton
initialization is supplied explicitly, not claimed as recovered startup.
"""
import ctypes as C
from inventory import PE
from native_gar import NativeGarCipher, DEFAULT_IMAGE

P=C.c_void_p
N=C.c_size_t


class NativeSceneLifecycle(NativeGarCipher):
    def __init__(self,*,math_helpers=False):
        super().__init__(math_helpers=math_helpers)
        self.heap=0;self.allocations={};self.freed=[];self.retired={};self.callbacks=[];self.imports=[];self.critical=None
        try:
            k=self.kernel
            k.HeapCreate.argtypes=[C.c_uint32,N,N];k.HeapCreate.restype=P
            k.HeapDestroy.argtypes=[P];k.HeapDestroy.restype=C.c_int
            k.HeapAlloc.argtypes=[P,C.c_uint32,N];k.HeapAlloc.restype=P
            k.HeapFree.argtypes=[P,C.c_uint32,P];k.HeapFree.restype=C.c_int
            for name in ('InitializeCriticalSection','DeleteCriticalSection','EnterCriticalSection','LeaveCriticalSection'):
                getattr(k,name).argtypes=[P]
            self.heap=k.HeapCreate(0,0,0)
            if not self.heap:raise C.WinError(C.get_last_error())

            @C.WINFUNCTYPE(P,P,C.c_uint32,N)
            def allocate(heap,flags,size):
                pointer=k.HeapAlloc(heap,flags,size)
                if pointer:
                    C.memset(pointer,0xa5,size)
                    self.allocations[pointer]=size
                return pointer

            @C.WINFUNCTYPE(C.c_int,P,C.c_uint32,P)
            def release(heap,flags,pointer):
                self.freed.append(pointer or 0)
                if pointer not in self.allocations:return 0
                self.retired[pointer]=C.string_at(pointer,self.allocations.pop(pointer))
                return k.HeapFree(heap,flags,pointer)

            self.callbacks=[allocate,release]
            replacements={'HeapAlloc':allocate,'HeapFree':release,
                'InitializeCriticalSection':k.InitializeCriticalSection,
                'EnterCriticalSection':k.EnterCriticalSection,'LeaveCriticalSection':k.LeaveCriticalSection}
            pe=PE(DEFAULT_IMAGE.read_bytes())
            for library in pe.imports():
                if library['dll'].lower()!='kernel32.dll':continue
                for row in library['functions']:
                    if row['name'] not in replacements:continue
                    address=int(row['iat_va'],16);old=C.c_uint32()
                    if not k.VirtualProtect(address,8,4,C.byref(old)):raise C.WinError(C.get_last_error())
                    slot=C.c_void_p.from_address(address);self.imports.append((address,slot.value))
                    slot.value=C.cast(replacements[row['name']],P).value
                    protection=C.c_uint32();k.VirtualProtect(address,8,old.value,C.byref(protection))
            if len(self.imports)!=5:raise RuntimeError('Inspected kernel import set changed')
            C.c_void_p.from_address(self.base+0x582510).value=self.heap
            C.c_uint32.from_address(self.base+0x1631ae0).value|=1
            C.c_uint32.from_address(self.base+0x1630d60).value=0
            self.critical=self.base+0x1630d68;k.InitializeCriticalSection(self.critical)
            C.WINFUNCTYPE(P)(self.base+0x24020)()
        except BaseException:
            self.close();raise

    def registry(self):
        sentinel=C.c_uint64.from_address(self.base+0x1630d48).value
        root=C.c_uint64.from_address(sentinel+8).value
        result={};pending=[root];visited=set()
        while pending:
            node=pending.pop()
            if node==sentinel:continue
            if node in visited or node not in self.allocations:raise AssertionError('Invalid native registry tree')
            visited.add(node)
            result[C.c_uint32.from_address(node+0x18).value]=C.c_uint64.from_address(node+0x20).value
            pending.extend((C.c_uint64.from_address(node).value,C.c_uint64.from_address(node+0x10).value))
        if len(result)!=C.c_size_t.from_address(self.base+0x1630d50).value:raise AssertionError('Native registry count differs')
        return result

    def close(self):
        if getattr(self,'critical',None):
            self.kernel.DeleteCriticalSection(self.critical);self.critical=None
        if getattr(self,'heap',0):
            self.kernel.HeapDestroy(self.heap);self.heap=0
        super().close()


if __name__=='__main__':
    with NativeSceneLifecycle() as native:
        record=C.create_string_buffer(bytes([0xcc])*0x170)
        ctor=C.WINFUNCTYPE(P,P,C.c_char_p,C.c_uint32)(native.base+0x10fa90)
        ctor(record,b'lifecycle-fixture',0)
        print('Native TRS constructor:',hex(C.c_uint32.from_buffer(record,0x1c).value),
              'registry objects:',len(native.registry()),'private allocations:',len(native.allocations))
        C.WINFUNCTYPE(None,P)(native.base+0x10fbb0)(record)
        print('After destruction: registry objects:',len(native.registry()),'private allocations:',len(native.allocations))
