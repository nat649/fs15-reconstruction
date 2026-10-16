#pragma once
#include "shared_scene.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct FS15SceneLodRecord {
    FS15SceneStateRecord node;float distances[4];uint32_t index,unknown184;
} FS15SceneLodRecord;
typedef struct FS15SceneLodEntry {
    FS15SceneLodRecord* node;float position[3],distance_squared[4];
    uint8_t levels,selected;uint16_t stamp;
} FS15SceneLodEntry;
typedef struct FS15SceneLodManager {
    FS15SceneLodEntry* entries;float viewer[3],distance_factor_squared;
    uint16_t count,capacity;uint32_t stamp; /* Row cache compares low 16 bits. */
} FS15SceneLodManager;
/* The context combines two observed global views. Its aggregate layout and
 * initialization are authored; it is not the complete native spatial manager. */
typedef struct FS15SceneLodContext {FS15SceneLodManager manager;FS15SceneVector pending;} FS15SceneLodContext;
typedef struct FS15SceneLodServices {
    void* user;const FS15SceneLifecycleServices* memory;
    int (*base_state)(void*,FS15SceneStateRecord*,uint32_t,int32_t);
    int (*world_position)(void*,FS15SceneStateRecord*,float*);
    int (*mark_child)(void*,FS15SceneStateRecord*,uint32_t);
} FS15SceneLodServices;
/* Known LOD constructor, index/child token assignment, manager storage and
 * stamp-cached squared-distance selection. Opaque bytes are preserved. Records
 * and all pointees must be valid owned supported storage; callbacks invoke only
 * recovered base/TRS/known-derived dispatch, never native identity tokens.
 * Added finite/bounds guards exclude undefined native inputs and uint16 capacity
 * overflow. Normal manager growth is in blocks of 1024, at most 64512 entries.
 * Callback/allocation failures may retain native-order prefix mutations. */
FS15_CORE_API int fs15_scene_lod_construct(FS15SceneLodRecord*,size_t,const char*,size_t,const float*,uint32_t,const FS15SceneLifecycleServices*);
FS15_CORE_API int fs15_scene_lod_set_index(FS15SceneLodRecord*,uint32_t,const FS15SceneLodServices*);
FS15_CORE_API int fs15_scene_lod_child_token(const FS15SceneLodRecord*,const FS15SceneStateRecord*,uint32_t*);
FS15_CORE_API int fs15_scene_inherited_lod_token(const FS15SceneStateRecord*,uint32_t*);
FS15_CORE_API int fs15_scene_lod_register(FS15SceneLodManager*,FS15SceneLodRecord*,const float*,const float*,uint32_t,uint32_t*,const FS15SceneLifecycleServices*);
FS15_CORE_API int fs15_scene_lod_remove(FS15SceneLodManager*,uint32_t,const FS15SceneLodServices*);
FS15_CORE_API int fs15_scene_lod_move(FS15SceneLodManager*,uint32_t,const float*,const float*,uint32_t);
FS15_CORE_API int fs15_scene_lod_test(FS15SceneLodManager*,uint32_t,int*);
FS15_CORE_API int fs15_scene_lod_enqueue(FS15SceneLodContext*,FS15SceneLodRecord*,const FS15SceneLifecycleServices*);
FS15_CORE_API int fs15_scene_lod_state(FS15SceneLodRecord*,uint32_t,int32_t,FS15SceneLodContext*,const FS15SceneLodServices*);
FS15_CORE_API int fs15_scene_lod_update(FS15SceneLodRecord*,FS15SceneLodContext*,const FS15SceneLodServices*);
FS15_CORE_API int fs15_scene_lod_destruct(FS15SceneLodRecord*,size_t,uint32_t,FS15SceneLodContext*,const FS15SceneLodServices*);
/* Authored queue-output transport for recovered LOD virtual state dispatch.
 * These variants accept arena kind=2 for LOD, alongside matrix=0 and TRS=1.
 * They collect ordered pending-queue requests; callers append corresponding
 * owned records to the context through lod_enqueue after a successful commit.
 * Notifications remain side-effect-free outputs. count reports required queue
 * capacity on failure; all node/event/queue output storage must be disjoint. */
typedef struct FS15SceneLodQueue {size_t* indices;size_t capacity,count;} FS15SceneLodQueue;
FS15_CORE_API int fs15_scene_state_lod(const FS15SceneStateArena*,size_t,unsigned,uint32_t,int32_t,FS15SceneNotification*,size_t,size_t*,FS15SceneLodQueue*);
FS15_CORE_API int fs15_scene_trs_lod(const FS15SceneStateArena*,size_t,unsigned,void*,size_t,FS15SceneNotification*,size_t,size_t*,int*,FS15SceneLodQueue*);
FS15_CORE_API int fs15_scene_link_lod(const FS15SceneStateArena*,uintptr_t*,size_t,size_t,uint32_t,int,FS15SceneNotification*,size_t,size_t*,uint32_t*,FS15SceneLodQueue*);
FS15_CORE_API int fs15_scene_unlink_lod(const FS15SceneStateArena*,uintptr_t*,size_t,uint32_t,int,FS15SceneNotification*,size_t,size_t*,FS15SceneLodQueue*);
#ifdef __cplusplus
}
static_assert(sizeof(FS15SceneLodRecord)==0x188);
static_assert(sizeof(FS15SceneLodEntry)==0x28);
static_assert(sizeof(FS15SceneLodManager)==0x20);
static_assert(offsetof(FS15SceneLodRecord,index)==0x180);
static_assert(offsetof(FS15SceneLodEntry,levels)==0x24);
#endif
