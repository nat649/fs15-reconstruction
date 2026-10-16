#pragma once
#include "scene_state.h"
#ifdef __cplusplus
extern "C" {
#endif
enum FS15SceneObjectKind {
    FS15_OBJECT_BASE=0,FS15_OBJECT_NODE=1,FS15_OBJECT_MATRIX=2,
    FS15_OBJECT_TRS=3,FS15_OBJECT_TRS_TYPED=4
};
/* Platform/container seams, not recovered CRT, mutex or registry internals.
 * Callbacks must not throw. allocate returns distinct storage, deallocate
 * accepts null. next_id supplies the native wrapping 32-bit sequence under the
 * caller's synchronization. registered receives the object before its final
 * class/type field is written, matching the native constructor ordering.
 * unlink/destroy_child implement the known scene dispatch for owned forests;
 * unlink must remove the indexed child and clear its parent. destroy_child
 * must perform deleting destruction and detach the child from its parent.
 * Records/auxiliary allocations must remain addressable through callbacks.
 * Checks reject malformed attribute maps and active-trigger
 * teardown on the supplied record before its external frees begin. The caller
 * must preflight all descendants and only dispatch recovered object kinds. */
typedef struct FS15SceneLifecycleServices {
    void* user;
    void* (*allocate)(void*,size_t);
    void (*deallocate)(void*,void*);
    uint32_t (*next_id)(void*);
    void (*registered)(void*,uint32_t,void*);
    void (*unregistered)(void*,uint32_t,void*);
    int (*unlink)(void*,void*,uint32_t,int);
    int (*destroy_child)(void*,void*);
    int (*release_shared)(void*,void*); /* Optional known-resource dispatch. */
} FS15SceneLifecycleServices;
/* Successful native initialization preserves opaque bytes. Stable observed
 * vtable addresses are stored as identity TOKENS, never callable C++ vtables.
 * kind TRS fixes the native class field to 0x1d; TRS_TYPED preserves type_tag.
 * Names are bounded NUL-terminated byte strings, not assumed UTF-8. Scene
 * objects register unconditionally; the base object's registration byte is
 * caller-supplied. On allocation failure, acquired storage is released, but
 * the ID sequence may already have advanced and the record may be partially
 * initialized; do not destruct a failed record. Native exception paths differ.
 * Inputs must not alias the record. */
FS15_CORE_API int fs15_scene_construct(void*,size_t,unsigned,const char*,size_t,
    uint32_t,uint8_t,const FS15SceneLifecycleServices*);
/* Rename allocates even for an empty string. Self-aliasing names are rejected
 * before the original name is freed; the native implementation has no guard. */
FS15_CORE_API int fs15_scene_rename(void*,size_t,const char*,size_t,const FS15SceneLifecycleServices*);
/* kind identifies the complete destructor chain. deleting_flags&1 frees the
 * object record after its members. Recursive child deletion is dispatched via
 * services, without executing original vtable pointers. Shared references use
 * release_shared; the owner must preflight its supported resource classes.
 * Active trigger services remain open. Callbacks must only invoke
 * supported descendants; callback failures after teardown starts cannot roll
 * back external frees or registry effects. */
FS15_CORE_API int fs15_scene_destruct(void*,size_t,unsigned,uint32_t,const FS15SceneLifecycleServices*);
#ifdef __cplusplus
}
#endif
