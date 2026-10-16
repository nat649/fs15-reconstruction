#pragma once
#include "scene_clone.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct FS15SceneCloneMapNode {
    struct FS15SceneCloneMapNode *left,*parent,*right;
    uintptr_t source,target;uint8_t color,nil,unknown2a[6];
} FS15SceneCloneMapNode;
typedef struct FS15SceneCloneMap {uintptr_t unknown00;FS15SceneCloneMapNode* head;size_t count;} FS15SceneCloneMap;
/* Observed pointer-keyed native map layout. Initialization/whole-map cleanup
 * are authored entry points around recovered construction/recursive free.
 * Init requires null head/count and preserves unknown00. Clear never destroys
 * mapped scene objects; release_header=0 retains a reusable empty sentinel.
 * Valid owned storage/pointees only, at most 1048576 entries and depth 256. */
FS15_CORE_API int fs15_scene_clone_map_init(FS15SceneCloneMap*,const FS15SceneLifecycleServices*);
FS15_CORE_API int fs15_scene_clone_map_clear(FS15SceneCloneMap*,int,const FS15SceneLifecycleServices*);
FS15_CORE_API int fs15_scene_clone_map_find(const FS15SceneCloneMap*,uintptr_t,FS15SceneCloneMapNode**);
/* Factory returns a caller-owned unlinked candidate. Insert consumes it on
 * success, retaining the existing entry and freeing the candidate on duplicate.
 * Failure does not consume it. Source keys compare as unsigned pointer bits. */
FS15_CORE_API int fs15_scene_clone_map_node(FS15SceneCloneMap*,uintptr_t,uintptr_t,const FS15SceneLifecycleServices*,FS15SceneCloneMapNode**);
FS15_CORE_API int fs15_scene_clone_map_insert(FS15SceneCloneMap*,FS15SceneCloneMapNode*,const FS15SceneLifecycleServices*,FS15SceneCloneMapNode**,int*);
/* Original clone-list vector instantiations, same recovered pointer-vector
 * allocation/growth rules, with authored bounds. Capacity/spare bytes retained. */
FS15_CORE_API int fs15_scene_clone_vector_reserve(FS15SceneVector*,size_t,const FS15SceneLifecycleServices*);
FS15_CORE_API int fs15_scene_clone_vector_grow(FS15SceneVector*,const FS15SceneLifecycleServices*);
typedef struct FS15SceneCloneTreeServices {
    FS15SceneCloneServices clone;
    int (*link)(void*,FS15SceneStateRecord*,FS15SceneStateRecord*,uint32_t);
} FS15SceneCloneTreeServices;
/* Recovered 0x1401b9d70 on known TRS/Camera/LOD dispatch. Records are cloned
 * pre-order, inserted into remap, appended to created, then children with type
 * flag 0x40 are recursively cloned and attached in order. Already-mapped input
 * returns success with null result, not the existing target. A root without
 * a supported derived bit or 0x40 also produces a successful null result.
 * Unrecovered Shape/Light/reference resources reject where encountered; no
 * reinterpretation or silent stripping. Bounds: 65535 calls, depth 256.
 * All fresh completed allocations must remain owned by constructed callbacks.
 * Failures keep map/vector/scene prefixes. result retains the root allocation;
 * unfinished identifies a record at failure and can alias result or map/vector
 * entries. Neither result owns a second copy. No rollback or implicit deletion.
 * Full clone finalization, skin remapping and animation rebinding remain open. */
FS15_CORE_API int fs15_scene_clone_tree(FS15SceneStateRecord*,FS15SceneCloneMap*,FS15SceneVector*,const FS15SceneCloneTreeServices*,FS15SceneGroupResult*,FS15SceneGroupResult*);
#ifdef __cplusplus
}
static_assert(sizeof(FS15SceneCloneMapNode)==0x30);
static_assert(offsetof(FS15SceneCloneMapNode,color)==0x28);
static_assert(sizeof(FS15SceneCloneMap)==0x18);
#endif
