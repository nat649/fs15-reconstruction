#pragma once
#include "export.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Partial native layout, not a recovered C++ class/allocator. Unknown bytes
 * remain opaque. Pointers describe caller-owned, bounded views below. */
typedef struct FS15SceneStateRecord {
    uint64_t vtable;
    uint8_t registered,unknown09[7];
    uintptr_t name;
    uint32_t identifier;
    uint32_t type_flags;
    uint32_t class_tag,unknown24;
    uintptr_t parent,children_begin,children_end,children_capacity;
    uint8_t unknown48[8];
    uint32_t flags,unknown54;
    uintptr_t observers_begin,observers_end,observers_capacity;
    uint8_t unknown70[8];
    uintptr_t attribute_unknown,attribute_head;
    size_t attribute_count;
    uint8_t unknown90[8];
    float local[16],world[16];
    uintptr_t properties;
    uintptr_t shared_resource;
    float local_clip[2];
    uint32_t local_masks[2];
    float world_clip[2];
    uint32_t world_masks[2];
    float translation[3],rotation[3],scale[3];
    uint32_t unknown16c;
} FS15SceneStateRecord;
typedef struct FS15ScenePropertyView {uint64_t unknown00;uint32_t flags,unknown0c;} FS15ScenePropertyView;
enum FS15SceneNodeKind {FS15_SCENE_MATRIX_NODE=0,FS15_SCENE_TRS_NODE=1};
typedef struct FS15SceneStateArena {
    void* nodes;
    size_t node_bytes,node_stride;
    const uintptr_t* children;
    size_t child_count;
    const uintptr_t* observers;
    size_t observer_count;
    const FS15ScenePropertyView* properties;
    size_t property_count;
    const uint8_t* kinds;
    size_t kind_count;
    size_t special_root; /* SIZE_MAX for no native global-root equivalent. */
} FS15SceneStateArena;
typedef struct FS15SceneNotification {size_t node_index;uintptr_t observer;int32_t origin;uint32_t reserved;} FS15SceneNotification;
enum FS15SceneStateOperation {
    FS15_SCENE_PROPAGATE=0,FS15_SCENE_RESCAN_OBSERVERS=1,FS15_SCENE_MARK_INHERITED=2
};
/* Recovered paths: 0x1400b1b00, 0x1400b6160, 0x1400b61f0. No raw virtual
 * functions are executed. Known base/TRS dispatch is selected by kinds[].
 * Notifications are returned in original traversal order, for later caller
 * delivery; callback side effects/reentrancy are not reconstructed.
 * All outputs must be disjoint from input views. On failure, records/events
 * are unchanged; notification_count may report the required output capacity.
 * Parent/child references must form a consistent bounded tree. Descendant
 * traversal is limited to 256 levels; bounds/finite checks are authored. */
FS15_CORE_API int fs15_scene_state_update(const FS15SceneStateArena*,size_t,unsigned,
    uint32_t,int32_t,FS15SceneNotification*,size_t,size_t*);
/* Recovered matrix vtable setters: plain copy vs TRS copy + dirty bit 0x80.
 * This does not propagate world changes or rebuild the cached TRS fields. */
FS15_CORE_API int fs15_scene_set_local_matrix(const FS15SceneStateArena*,size_t,const float*,size_t);
enum FS15SceneTRSOperation {
    FS15_SCENE_TRS_REFRESH=0,FS15_SCENE_TRS_REBUILD=1,
    FS15_SCENE_SET_TRANSLATION=2,FS15_SCENE_SET_ROTATION=3,FS15_SCENE_SET_SCALE=4,
    FS15_SCENE_GET_TRANSLATION=5,FS15_SCENE_GET_ROTATION=6,FS15_SCENE_GET_SCALE=7
};
/* Recovered cache/rebuild and six complete Lua-frame adapters. Getter frames
 * use values +0xf0/+0x100/+0x110, uint16 numeric tags at +8. Setter inputs are
 * at +0/+0x10/+0x20. Refresh/rebuild accept a null frame. Notifications are
 * staged as above. Rebuild propagates mask 1 with original origin=0.
 * setScale preserves the native store/rebuild behavior even on dynamic nodes;
 * scale_warning reports its diagnostic condition instead of invoking the
 * unrecovered native logger. Math uses the platform libm; see scene_trs.h. */
FS15_CORE_API int fs15_scene_trs_update(const FS15SceneStateArena*,size_t,unsigned,
    void*,size_t,FS15SceneNotification*,size_t,size_t*,int*);
/* Recovered hierarchy paths: raw ordered reparent (0x1401363d0), matrix-node
 * link (0x1400b17b0), and unlink (0x1400b19c0). writable_children must equal
 * arena.children and cover its entire child_count, including reserved slots.
 * Each node's begin/end/capacity must describe a disjoint reserved subrange.
 * No native allocator is used: insufficient child/event capacity rejects the
 * entire update. Link position is clamped after removal from the old parent;
 * UINT32_MAX appends. matrix_link=0 selects raw reparent, =1 also updates
 * observer/inherited/transform state. Unlink removes the given child slot;
 * suppress_update!=0 preserves its cached state, as during native destruction.
 * Ordered notifications are returned for later delivery, with origin 0.
 * Nodes, child storage, events and scalar outputs must not overlap. */
FS15_CORE_API int fs15_scene_link(const FS15SceneStateArena*,uintptr_t* writable_children,
    size_t parent,size_t child,uint32_t position,int matrix_link,
    FS15SceneNotification*,size_t,size_t*,uint32_t* attached_index);
FS15_CORE_API int fs15_scene_unlink(const FS15SceneStateArena*,uintptr_t* writable_children,
    size_t parent,uint32_t child_slot,int suppress_update,
    FS15SceneNotification*,size_t,size_t*);
#ifdef __cplusplus
}
static_assert(sizeof(void*)==8,"Observed scene layout requires 64-bit pointers");
static_assert(offsetof(FS15SceneStateRecord,name)==0x10);
static_assert(offsetof(FS15SceneStateRecord,identifier)==0x18);
static_assert(offsetof(FS15SceneStateRecord,attribute_head)==0x80);
static_assert(offsetof(FS15SceneStateRecord,type_flags)==0x1c);
static_assert(offsetof(FS15SceneStateRecord,parent)==0x28);
static_assert(offsetof(FS15SceneStateRecord,flags)==0x50);
static_assert(offsetof(FS15SceneStateRecord,local)==0x98);
static_assert(offsetof(FS15SceneStateRecord,world)==0xd8);
static_assert(offsetof(FS15SceneStateRecord,properties)==0x118);
static_assert(offsetof(FS15SceneStateRecord,local_clip)==0x128);
static_assert(offsetof(FS15SceneStateRecord,world_masks)==0x140);
static_assert(offsetof(FS15SceneStateRecord,translation)==0x148);
static_assert(sizeof(FS15SceneStateRecord)==0x170);
#endif
