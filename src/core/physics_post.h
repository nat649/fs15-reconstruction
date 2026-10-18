#pragma once
#include "physics_dispatch.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct FS15PhysicsTreeNode {
    struct FS15PhysicsTreeNode *left,*parent,*right;
    uint64_t key;uintptr_t value;uint8_t color,nil,unknown2a[6];
} FS15PhysicsTreeNode;
typedef struct FS15PhysicsPair {uintptr_t first,second;uint32_t mask,opaque14;int32_t next;uint32_t opaque1c;} FS15PhysicsPair;
typedef struct FS15PhysicsPreviousPair {uintptr_t first,second;uint32_t mask,opaque14;} FS15PhysicsPreviousPair;
typedef struct FS15PhysicsContacts {
    uint8_t unknown00[0xc];uint32_t count,capacity,unknown14;
    FS15PhysicsPair* current;uint8_t unknown20[8];
    FS15PhysicsPreviousPair* previous;uint32_t previous_count,previous_capacity;
    int32_t* buckets;uint8_t unknown40[0x20];
} FS15PhysicsContacts;
/* Caller-owned native pointer graphs: pair endpoints have body at +0 and ID
 * at +0x18; bodies have metadata at +0x110. Metadata: flags +0, owner +8,
 * indexed 16-byte part records +0x10, fallback ID +0x18. All traversed storage
 * must be valid/disjoint. part_limit bounds indexed records, at most 32.
 * No native code pointers are executed; tokens identify external services. */
typedef struct FS15PhysicsPostServices {
    void* user;
    uint32_t (*wait)(void*,uintptr_t,uint32_t,uint8_t);
    int (*object)(void*,uintptr_t);
    int (*frame)(void*,const void* wheel_set,int32_t index,float* matrix16);
    float (*arctangent)(void*,float);
    void* (*allocate)(void*,size_t,size_t alignment);
    void (*release)(void*,void*);
    int (*observer)(void*,uintptr_t listener,uintptr_t first,uintptr_t second,uintptr_t target,uint32_t event,uint32_t part);
    int (*pending)(void*,uintptr_t callback,uint32_t owner,float value,uint8_t* cancel);
    int (*cancel)(void*,void* manager,uint32_t owner);
    size_t part_limit;
    uintptr_t (*lookup)(void*,uintptr_t world,uintptr_t first,uintptr_t second);
    int (*groups)(void*,uintptr_t interface_token,void* native_vector);
    size_t contact_row_limit;
} FS15PhysicsPostServices;
/* 0x140144ce0: consume backend contact groups into current pair masks, including
 * indexed endpoint selectors, low-five-bit wrapping and scratch-count reset.
 * Lookup returns a native-layout wrapper with interface at +0x10. The groups
 * service fills the native vector at cache+0x40 (count+4, capacity+8, data+0x10).
 * Group count at +0x320; selectors at +0x78/+0x7c with 0xc0 stride, first body
 * at +0x310. Caller bounds group rows through contact_row_limit. */
FS15_CORE_API int fs15_physics_contacts_collect(FS15PhysicsContacts*,size_t,uintptr_t world,const FS15PhysicsPostServices*);
/* 0x1400ca150 including 0x1400c02d0's translated basis product. Wheel source
 * fields are retained; readback fields are refreshed, fourth position lane is
 * zero. Frame lookup and optional atan are explicit unresolved backend seams. */
FS15_CORE_API int fs15_physics_wheel_refresh(void*,size_t,const void*,const FS15PhysicsPostServices*);
/* 0x1400ca080: native wheel registry order and per-set record order. */
FS15_CORE_API int fs15_physics_wheels(FS15PhysicsDispatchState*,size_t,const FS15PhysicsPostServices*);
/* 0x140145190: suppression/stay/indexed-part policy and handled-listener order.
 * Listener span replaces the original vector transport. Observer returns a
 * native handled Boolean; callbacks may not throw, reenter or mutate topology. */
FS15_CORE_API int fs15_physics_contact_notify(const void* pair,uint32_t mask,uint32_t event,const uintptr_t*,size_t,const FS15PhysicsPostServices*);
/* 0x140144ef0: hash lookup, enter/leave/stay masks, swap-last removal, previous
 * buffer growth and retention. Callbacks cannot mutate cache or endpoint IDs.
 * Authored bounds/cycle/allocation checks can fail after prior notifications. */
FS15_CORE_API int fs15_physics_contacts_finish(FS15PhysicsContacts*,size_t,const uintptr_t*,size_t,const FS15PhysicsPostServices*);
/* 0x1400c7b20: completion wait -> registered objects -> wheels -> contacts ->
 * pending swap-last removal/callback/cancel -> clear busy. Object finalizers,
 * controller updates and cancel implementation remain services. No physics
 * world/solver is created. Valid manager trees/list/vector ownership required.
 * Synchronous busy clears BEFORE callbacks; asynchronous busy clears LAST.
 * Cancellation may change pending queue storage/count; other callbacks must
 * keep traversed trees/cache endpoints stable. Pending values use float ABI. */
FS15_CORE_API int fs15_physics_post_step(FS15PhysicsDispatchState*,size_t,const FS15PhysicsPostServices*);
#ifdef __cplusplus
}
static_assert(sizeof(FS15PhysicsTreeNode)==0x30);
static_assert(sizeof(FS15PhysicsPair)==0x20&&sizeof(FS15PhysicsPreviousPair)==0x18);
static_assert(sizeof(FS15PhysicsContacts)==0x60);
#endif
