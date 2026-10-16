#pragma once
#include "scene_light.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct FS15SceneSpatialEntry {
    uint32_t* token;float center[3],radius,clip[2];uint32_t lod_token,unknown24;
} FS15SceneSpatialEntry;
typedef struct FS15SceneSpatialCell {
    FS15SceneSpatialEntry* entries;uint16_t count,capacity;
    float center_x,center_z,half_x,half_z;uint32_t skip;
} FS15SceneSpatialCell;
typedef struct FS15SceneSpatialGrid {
    FS15SceneSpatialCell* cells;uint32_t dimension,levels,count;
    float min_x,max_x,min_z,max_z,min_y,max_y,scale_x,scale_z;uint32_t unknown34;
} FS15SceneSpatialGrid;
/* Recovered native grid and cell paths. Caller owns valid disjoint records,
 * tokens and allocations. Opaque/spare bytes are retained. Bounds/finite guards
 * reject undefined float conversion and uint16 capacity wrap. Init rounds the
 * requested dimension to a power of two in 1..128; warning callbacks are absent.
 * Reinitialization migrates entries and retains OLD per-cell allocations as the
 * original does. The caller/allocator must retain and eventually reclaim them.
 * Allocation/callback failure may retain a native-order prefix. */
FS15_CORE_API int fs15_scene_spatial_index(const FS15SceneSpatialGrid*,uint32_t,uint32_t,uint32_t,uint32_t*);
FS15_CORE_API int fs15_scene_spatial_init(FS15SceneSpatialGrid*,const float*,const float*,int32_t,const FS15SceneLifecycleServices*);
FS15_CORE_API int fs15_scene_spatial_select(const FS15SceneSpatialGrid*,const FS15SceneSpatialSphere*,uint32_t*,uint32_t*,uint32_t*,uint32_t*);
FS15_CORE_API int fs15_scene_spatial_cell_append(FS15SceneSpatialCell*,uint32_t*,const FS15SceneSpatialSphere*,float,float,uint32_t,uint32_t*,const FS15SceneLifecycleServices*);
FS15_CORE_API int fs15_scene_spatial_cell_move(FS15SceneSpatialCell*,uint32_t,const FS15SceneSpatialSphere*,float,float,uint32_t);
FS15_CORE_API int fs15_scene_spatial_cell_remove(FS15SceneSpatialCell*,uint32_t);
FS15_CORE_API int fs15_scene_spatial_add(FS15SceneSpatialGrid*,uint32_t*,const FS15SceneSpatialSphere*,float,float,uint32_t,const FS15SceneLifecycleServices*);
FS15_CORE_API int fs15_scene_spatial_move(FS15SceneSpatialGrid*,uint32_t*,const FS15SceneSpatialSphere*,float,float,uint32_t,const FS15SceneLifecycleServices*);
FS15_CORE_API int fs15_scene_spatial_remove(FS15SceneSpatialGrid*,uint32_t*);
/* Authored release of CURRENT cell allocations/header. Does not invalidate live
 * owner tokens or reclaim buffers retained by earlier native reinitialization. */
FS15_CORE_API int fs15_scene_spatial_release(FS15SceneSpatialGrid*,const FS15SceneLifecycleServices*);
typedef struct FS15SceneLightUpdateServices {
    void* user;const FS15SceneLifecycleServices* memory;
    int (*world_matrix)(void*,FS15SceneStateRecord*,float*); /* 16 floats, recovered world cache */
} FS15SceneLightUpdateServices;
FS15_CORE_API int fs15_scene_light_bounds(FS15SceneLightRecord*,const FS15SceneLightUpdateServices*);
FS15_CORE_API int fs15_scene_light_update(FS15SceneLightRecord*,FS15SceneSpatialGrid*,const FS15SceneLightUpdateServices*);
/* Extends the existing destructor path to active spatial registration; render
 * resources still reject. The existing pending-only API retains its boundary. */
FS15_CORE_API int fs15_scene_light_spatial_destruct(FS15SceneLightRecord*,size_t,uint32_t,FS15SceneSpatialGrid*,FS15SceneLightContext*,const FS15SceneLifecycleServices*);
/* Original manager traversal with Shape/audio pending lists empty: snapshot LOD
 * requests first, then Light requests, then clear Light and LOD ends. Separate
 * context layout and callback transport are authored, not a full manager ABI. */
FS15_CORE_API int fs15_scene_lights_flush(FS15SceneLodContext*,const FS15SceneLodServices*,FS15SceneLightContext*,FS15SceneSpatialGrid*,const FS15SceneLightUpdateServices*);
#ifdef __cplusplus
}
static_assert(sizeof(FS15SceneSpatialSphere)==0x20);
static_assert(sizeof(FS15SceneSpatialEntry)==0x28);
static_assert(sizeof(FS15SceneSpatialCell)==0x20);
static_assert(sizeof(FS15SceneSpatialGrid)==0x38);
#endif
