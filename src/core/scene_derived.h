#pragma once
#include "scene_owned.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Authored transactional transport for known LOD=2 and Light=3 dispatch.
 * light_tokens is indexed like the arena, disjoint caller-owned uint32 storage;
 * only Light entries are read/committed. It stages native Light +0x1d4 fields.
 * Separate queues preserve each native manager's request order. Queue counts
 * report required capacity on rejection; node/token outputs stay unchanged.
 * Other arena, finite, ownership and callback limits remain those of scene_state.
 * MARK_INHERITED passes mask as the original inherited token to descendants. */
typedef struct FS15SceneDerivedState {
    FS15SceneLodQueue* lod;FS15SceneLodQueue* lights;
    uint32_t* light_tokens;size_t count;
} FS15SceneDerivedState;
FS15_CORE_API int fs15_scene_state_derived(const FS15SceneStateArena*,size_t,unsigned,uint32_t,int32_t,FS15SceneNotification*,size_t,size_t*,FS15SceneDerivedState*);
FS15_CORE_API int fs15_scene_trs_derived(const FS15SceneStateArena*,size_t,unsigned,void*,size_t,FS15SceneNotification*,size_t,size_t*,int*,FS15SceneDerivedState*);
FS15_CORE_API int fs15_scene_link_derived(const FS15SceneStateArena*,uintptr_t*,size_t,size_t,uint32_t,int,FS15SceneNotification*,size_t,size_t*,uint32_t*,FS15SceneDerivedState*);
FS15_CORE_API int fs15_scene_unlink_derived(const FS15SceneStateArena*,uintptr_t*,size_t,uint32_t,int,FS15SceneNotification*,size_t,size_t*,FS15SceneDerivedState*);
/* Owned variants stage Light tail tokens automatically. Full 0x200-byte Light
 * records must be valid when kind=3. Pending outputs contain source node indices;
 * caller dispatches each queue to its corresponding recovered manager append. */
FS15_CORE_API int fs15_scene_owned_state_derived(const FS15SceneOwnedView*,size_t,unsigned,uint32_t,int32_t,FS15SceneNotification*,size_t,size_t*,FS15SceneLodQueue*,FS15SceneLodQueue*);
FS15_CORE_API int fs15_scene_owned_trs_derived(const FS15SceneOwnedView*,size_t,unsigned,void*,size_t,FS15SceneNotification*,size_t,size_t*,int*,FS15SceneLodQueue*,FS15SceneLodQueue*);
FS15_CORE_API int fs15_scene_owned_link_derived(const FS15SceneOwnedView*,size_t,size_t,uint32_t,int,const FS15SceneLifecycleServices*,FS15SceneNotification*,size_t,size_t*,uint32_t*,FS15SceneLodQueue*,FS15SceneLodQueue*);
FS15_CORE_API int fs15_scene_owned_unlink_derived(const FS15SceneOwnedView*,size_t,uint32_t,int,FS15SceneNotification*,size_t,size_t*,FS15SceneLodQueue*,FS15SceneLodQueue*);
#ifdef __cplusplus
}
#endif
