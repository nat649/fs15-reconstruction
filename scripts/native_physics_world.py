"""Local unchanged GE6 physics factory/body/solver reference.

Uses the pinned image, explicit isolated CRT heap, aligned allocator and timer
imports. No entry point, Steam, installed game process or ReFarm is invoked.
Body setup composes inspected original base/setup bodies around an explicit
caller-owned construction descriptor. This is not recovered process startup.
"""
import ctypes as C
import struct
from inventory import PE
from native_animation_xml import NativeAnimationXml
from native_gar import DEFAULT_IMAGE

P=C.c_void_p;N=C.c_size_t;U=C.c_uint32;F=C.c_float
class NativePhysicsWorld(NativeAnimationXml):
    def __init__(self):
        super().__init__()
        self.aligned={};self.aligned_errors=[];self.worlds=[]
        self.crt=C.CDLL('ucrtbase');self.crt._aligned_malloc.argtypes=[N,N];self.crt._aligned_malloc.restype=P;self.crt._aligned_free.argtypes=[P]
        @C.WINFUNCTYPE(P,N,N)
        def allocate(size,_alignment):
            # Some inlined native sites omit the unused second argument.
            pointer=self.crt._aligned_malloc(size,16)
            if pointer:C.memset(pointer,0xa5,size);self.aligned[pointer]=size
            return pointer
        @C.WINFUNCTYPE(None,P)
        def release(pointer):
            if not pointer:return
            if pointer not in self.aligned:self.aligned_errors.append('foreign aligned free');return
            self.aligned.pop(pointer);self.crt._aligned_free(pointer)
        self.physics_callbacks=[allocate,release]
        self._slot(self.base+0x575110,C.cast(allocate,P).value);self._slot(self.base+0x575118,C.cast(release,P).value)
        for lib in PE(DEFAULT_IMAGE.read_bytes()).imports():
            for row in lib['functions']:
                if row['name'] in ('QueryPerformanceCounter','GetTickCount','QueryPerformanceFrequency','DeleteCriticalSection'):
                    self._slot(int(row['iat_va'],16),C.cast(getattr(self.kernel,row['name']),P).value)
        self.timer=C.create_string_buffer(40)
        self.kernel.QueryPerformanceFrequency(C.byref(self.timer));self.kernel.QueryPerformanceCounter(P(C.addressof(self.timer)+24))
        U.from_address(C.addressof(self.timer)+8).value=self.kernel.GetTickCount()
        P.from_address(self.base+0x162e538).value=C.addressof(self.timer)
        # This detects an omitted auxiliary constant section before validating
        # any trajectories. Previously powf(1, dt) silently returned zero.
        if self.fn(0x166b3e0,F,F,F)(1.,1./60)!=1.:raise AssertionError('Original powf constants/ABI unavailable')

    def _slot(self,address,pointer):
        old=U();self.kernel.VirtualProtect(address,8,4,C.byref(old));P.from_address(address).value=pointer;self.kernel.VirtualProtect(address,8,old.value,C.byref(U()))

    def fn(self,offset,result,*args):return C.WINFUNCTYPE(result,*args)(self.base+offset)

    def create(self,gravity):
        cfg=C.create_string_buffer(0x48);struct.pack_into('<3f',cfg,0x34,0,gravity,0);world=P();configuration=P()
        if not self.fn(0x27fb30,C.c_uint8,P,P,P)(cfg,C.byref(world),C.byref(configuration)):raise AssertionError('Original world factory failed')
        self.worlds.append((world.value,configuration.value));return world.value

    def destroy(self,world):
        pair=next(p for p in self.worlds if p[0]==world)
        broadphase=P.from_address(world+0x60).value
        external_cache=P.from_address(broadphase+0xe0).value
        self.fn(0x27fd70,None,P,P)(*pair);self.worlds.remove(pair)
        # The native broadphase receives an external pair cache and leaves its
        # ownership outside its destructor. Reclaim this isolated fixture's
        # borrowed cache explicitly; do not attribute that step to GE6 teardown.
        if external_cache in self.aligned:
            table=P.from_address(external_cache).value
            C.WINFUNCTYPE(P,P,U)(P.from_address(table).value)(external_cache,1)
            if external_cache in self.aligned:self.physics_callbacks[1](external_cache)
        if self.aligned_errors:raise AssertionError(self.aligned_errors)

    def sphere(self,world,body_mass,radius,y):
        body=C.create_string_buffer(0x300);shape=C.create_string_buffer(0x50);ci=C.create_string_buffer(0xa0)
        self.fn(0xc2930,P,P,F)(shape,radius)
        struct.pack_into('<f',ci,0,body_mass);struct.pack_into('<Q',ci,0x50,C.addressof(shape))
        for i in range(3):struct.pack_into('<f',ci,0x10+i*16+i*4,1.)
        struct.pack_into('<f',ci,0x44,y);struct.pack_into('<3f',ci,0x60,*([.4*body_mass*radius*radius]*3))
        struct.pack_into('<5f',ci,0x78,.5,0,0,.8,1.);struct.pack_into('<4f',ci,0x90,.005,.01,.01,.01)
        self.fn(0x2303e0,P,P)(body);struct.pack_into('<Q',body,0,self.base+0x4d51d0)
        body[0x258]=b'\x01'
        self.fn(0x229c70,None,P,P)(body,ci)
        # The MSVC overloaded default addRigidBody slot differs from the
        # overload taking filter group/mask. Call its inspected body.
        self.fn(0x2eda30,None,P,P)(world,body);return body,shape,ci

    def sphere_drop(self,gravity,height,radius,mass,steps,dt):
        world=self.create(gravity)
        U.from_address(world+0xa4).value=20;U.from_address(world+0xd0).value|=0x70
        objects=[self.sphere(world,0,radius,-radius),self.sphere(world,mass,radius,height)];falling=C.addressof(objects[1][0]);rows=[]
        try:
            dispatch=P.from_address(world+0x28).value
            for _ in range(steps):
                self.fn(0x2ed5c0,C.c_int,P,F,C.c_int,F)(world,dt,20,dt)
                position=struct.unpack('<3f',C.string_at(falling+0x40,12));velocity=struct.unpack('<3f',C.string_at(falling+0x180,12))
                count=U.from_address(dispatch+0x14).value
                data=P.from_address(dispatch+0x20).value;contacts=0
                for j in range(count):contacts+=U.from_address(P.from_address(data+j*8).value+0x320).value
                rows.append((*position,*velocity,float(contacts)))
        finally:
            for body,shape,ci in reversed(objects):
                self.fn(0x2ed9c0,None,P,P)(world,body)
                for obj in (body,shape):
                    table=P.from_address(C.addressof(obj)).value
                    C.WINFUNCTYPE(P,P,U)(P.from_address(table).value)(obj,0)
            self.destroy(world)
        return rows

    def hook_trace(self,keys,steps):
        world=self.create(-9.81);U.from_address(world+0xa4).value=20;U.from_address(world+0xd0).value|=0x70
        body,shape,ci=self.sphere(world,1,1,8);events=[];identities={};actors=[]
        def event(kind,p,extra=0):events.append((kind,identities[p],extra))
        def timed(kind,p,dt):event(kind,p,struct.unpack('<I',struct.pack('<f',dt))[0])
        callbacks=[C.WINFUNCTYPE(None,P,P)(lambda p,_:event(2,p)),
            C.WINFUNCTYPE(C.c_int,P)(lambda p:C.c_int.from_address(p+8).value),
            C.WINFUNCTYPE(None,P,P)(lambda p,_:event(1,p)),
            C.WINFUNCTYPE(None,P,P,P)(lambda p,b,_:event(3,p,U.from_address(b+4).value)),
            C.WINFUNCTYPE(None,P,P,F)(lambda p,_,dt:timed(4,p,dt)),
            C.WINFUNCTYPE(None,P,P,F)(lambda p,_,dt:timed(5,p,dt)),
            C.WINFUNCTYPE(None,P,P,F)(lambda p,_,dt:timed(6,p,dt))]
        table=(N*7)(*[C.cast(cb,P).value for cb in callbacks])
        for i,key in enumerate(keys):
            actor=C.create_string_buffer(16);struct.pack_into('<Qi',actor,0,C.addressof(table),key);identities[C.addressof(actor)]=i;actors.append(actor)
        array=(N*len(keys))(*[C.addressof(a) for a in actors])
        struct.pack_into('<ii', (C.c_char*0x1f0).from_address(world),0x1d4,len(keys),len(keys));P.from_address(world+0x1e0).value=C.addressof(array);C.c_uint8.from_address(world+0x1e8).value=0
        try:
            for _ in range(steps):self.fn(0x2ed5c0,C.c_int,P,F,C.c_int,F)(world,1/60,20,1/60)
        finally:
            self.fn(0x2ed9c0,None,P,P)(world,body)
            for obj in (body,shape):
                table_ptr=P.from_address(C.addressof(obj)).value;C.WINFUNCTYPE(P,P,U)(P.from_address(table_ptr).value)(obj,0)
            self.destroy(world)
        return events

    def close(self):
        for world,_ in list(getattr(self,'worlds',[])):self.destroy(world)
        for pointer in list(getattr(self,'aligned',{})):self.crt._aligned_free(pointer);self.aligned.pop(pointer)
        super().close()
