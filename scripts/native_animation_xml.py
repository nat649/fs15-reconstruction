"""Original XML callback reference with explicit private C-locale/FLS state.

Extends the private-heap reference's five kernel seams with four inspected
GetLastError/SetLastError/FlsGetValue/FlsSetValue imports. Code bytes are never
patched. The private FLS slot and thread/locale views are fixture initialization,
not recovered CRT startup, and never touch the live game or its TLS.
"""
import ctypes as C
from check_shared_scene import NativeShared
from check_scene_lifecycle import read,put
from inventory import PE
from native_gar import DEFAULT_IMAGE


class NativeAnimationXml(NativeShared):
    def __init__(self):
        super().__init__();self.fls_slot=None;self.thread_data=None
        try:
            k=self.kernel;P=C.c_void_p
            k.FlsAlloc.argtypes=[P];k.FlsAlloc.restype=C.c_uint32
            k.FlsSetValue.argtypes=[C.c_uint32,P];k.FlsGetValue.argtypes=[C.c_uint32];k.FlsGetValue.restype=P
            k.FlsFree.argtypes=[C.c_uint32];k.GetLastError.restype=C.c_uint32;k.SetLastError.argtypes=[C.c_uint32]
            self.fls_slot=k.FlsAlloc(None)
            if self.fls_slot==0xffffffff:raise RuntimeError('Unable to allocate isolated CRT FLS slot')
            self.thread_data=C.create_string_buffer(0x2c8);pointer=C.addressof(self.thread_data)
            # Observed _LocaleUpdate fields: MBC +b8, locale +c0, flags +c8.
            put(pointer,0xb8,read(self.base+0x56d640));put(pointer,0xc0,read(self.base+0x56db70));put(pointer,0xc8,2,'I')
            put(self.base+0x56db88,0,self.fls_slot,'I')
            if not k.FlsSetValue(self.fls_slot,pointer):raise RuntimeError('Unable to set private CRT FLS data')
            self.locale_imports=[]
            for library in PE(DEFAULT_IMAGE.read_bytes()).imports():
                if library['dll'].lower()!='kernel32.dll':continue
                for row in library['functions']:
                    if row['name'] not in ('GetLastError','SetLastError','FlsGetValue','FlsSetValue'):continue
                    at=int(row['iat_va'],16);old=C.c_uint32()
                    if not k.VirtualProtect(at,8,4,C.byref(old)):raise C.WinError(C.get_last_error())
                    put(at,0,C.cast(getattr(k,row['name']),P).value)
                    k.VirtualProtect(at,8,old.value,C.byref(C.c_uint32()));self.locale_imports.append(row['name'])
            if len(self.locale_imports)!=4:raise RuntimeError('Inspected XML CRT import boundary changed')
        except BaseException:self.close();raise

    def close(self):
        slot=getattr(self,'fls_slot',None)
        if slot is not None and slot!=0xffffffff:
            self.kernel.FlsSetValue(slot,None);self.kernel.FlsFree(slot);self.fls_slot=None
        self.thread_data=None;super().close()
