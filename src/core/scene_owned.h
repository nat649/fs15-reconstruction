#pragma once
#include "shared_scene.h"
#include "scene_lod.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Recovered pointer-vector reserve/grow/append (0x1400d5f30, 0x1400d34e0,
 * 0x14005a1b0). Owned valid storage and allocator required. Reserve copies only
 * used elements and retains native 1.5x growth. Append handles an input element
 * aliased into used storage, including when allocation relocates that storage.
 * Added bounds/allocation checks reject before changing the vector on failure. */
FS15_CORE_API int fs15_scene_pointer_reserve(FS15SceneVector*,size_t,const FS15SceneLifecycleServices*);
FS15_CORE_API int fs15_scene_pointer_grow(FS15SceneVector*,const FS15SceneLifecycleServices*);
FS15_CORE_API int fs15_scene_pointer_append(FS15SceneVector*,const uintptr_t*,const FS15SceneLifecycleServices*);
/* Authored scatter/gather transport for the recovered scene operations.
 * Records, property objects, child and observer vectors may be allocated
 * separately. The view must contain the complete supported forest and each
 * node exactly once; kinds are known base/matrix=0 or TRS=1 dispatch only.
 * Pointees and complete vector capacities must be valid caller-owned storage.
 * Current bounds: 65535 nodes, 1048576 total child/observer slots, depth 256.
 * This is not a replacement engine, allocator or original scene-loader class.
 * Identity tokens/opaque record bytes remain untouched by transport. */
typedef struct FS15SceneOwnedView {
    FS15SceneStateRecord* const* nodes;size_t count;
    const uint8_t* kinds;size_t special_root;
} FS15SceneOwnedView;
FS15_CORE_API int fs15_scene_owned_state(const FS15SceneOwnedView*,size_t,unsigned,uint32_t,int32_t,
    FS15SceneNotification*,size_t,size_t*);
FS15_CORE_API int fs15_scene_owned_trs(const FS15SceneOwnedView*,size_t,unsigned,void*,size_t,
    FS15SceneNotification*,size_t,size_t*,int*);
/* Ordered linking now grows owned child vectors through recovered allocation
 * logic. A successful commit retains native capacities and unused-slot bytes.
 * Preflight/temporary staging are authored: malformed views, insufficient event
 * capacity or allocation failure leave the forest unchanged; callbacks cannot
 * throw/reenter. External input/output storage must be disjoint from the view. */
FS15_CORE_API int fs15_scene_owned_link(const FS15SceneOwnedView*,size_t,size_t,uint32_t,int,
    const FS15SceneLifecycleServices*,FS15SceneNotification*,size_t,size_t*,uint32_t*);
FS15_CORE_API int fs15_scene_owned_unlink(const FS15SceneOwnedView*,size_t,uint32_t,int,
    FS15SceneNotification*,size_t,size_t*);
/* LOD-aware variants collect pending-update requests in native traversal order.
 * They preserve the same ownership/guard contracts, accepting kind=2 for LOD.
 * LOD tails are separate valid 0x188-byte owned records and remain addressable. */
FS15_CORE_API int fs15_scene_owned_state_lod(const FS15SceneOwnedView*,size_t,unsigned,uint32_t,int32_t,FS15SceneNotification*,size_t,size_t*,FS15SceneLodQueue*);
FS15_CORE_API int fs15_scene_owned_trs_lod(const FS15SceneOwnedView*,size_t,unsigned,void*,size_t,FS15SceneNotification*,size_t,size_t*,int*,FS15SceneLodQueue*);
FS15_CORE_API int fs15_scene_owned_link_lod(const FS15SceneOwnedView*,size_t,size_t,uint32_t,int,const FS15SceneLifecycleServices*,FS15SceneNotification*,size_t,size_t*,uint32_t*,FS15SceneLodQueue*);
FS15_CORE_API int fs15_scene_owned_unlink_lod(const FS15SceneOwnedView*,size_t,uint32_t,int,FS15SceneNotification*,size_t,size_t*,FS15SceneLodQueue*);
FS15_CORE_API int fs15_scene_owned_world(const FS15SceneOwnedView*,size_t,float*,size_t);
/* Known matrix setter dispatch, including the inherited TRS setter on LOD.
 * This copies the local matrix/dirty cache only; propagation is a separate call. */
FS15_CORE_API int fs15_scene_owned_matrix(const FS15SceneOwnedView*,size_t,const float*,size_t);
/* Known inherited-LOD marker (0x1400b61f0) on a valid owned subtree, without
 * inspecting parents/properties or requiring the rest of a tearing-down forest.
 * Supports matrix/TRS/LOD/Camera tables; authored depth/cycle bounds preflight. */
FS15_CORE_API int fs15_scene_owned_mark_branch(FS15SceneStateRecord*);
#ifdef __cplusplus
}
#endif
