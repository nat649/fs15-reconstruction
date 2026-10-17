#pragma once
#include "scene_spatial.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Inspected native prefixes. Identity tables are tokens, never executed. The
 * frustum is the first 0x68 bytes of a larger render-view object; the clip view
 * is the first 0x18 bytes of the dynamic volume, not its complete owned class. */
typedef struct FS15SceneFrustum {uintptr_t vtable;float planes[6][4];} FS15SceneFrustum;
typedef struct FS15SceneClipPlanes {uintptr_t vtable;const float (*planes)[4];uint32_t count,unknown14;} FS15SceneClipPlanes;
/* Native plane normalization and row combinations from a column-major 4x4
 * matrix. The record's vtable is preserved. Disjoint input/output storage is
 * required. Finite/length validation is authored; degenerate finite planes
 * retain native reciprocal-seed behavior rather than using ordinary 1/sqrt. */
FS15_CORE_API int fs15_scene_plane_normalize(float*,size_t);
FS15_CORE_API int fs15_scene_frustum_matrix(FS15SceneFrustum*,const float*,size_t);
/* Six-plane and dynamic-plane predicates. Sphere input is {x,y,z,radius};
 * boxes use center/half-extents. Native tangency rejects (<=), empty dynamic
 * plane sets accept. Caller owns valid records and pointees. */
FS15_CORE_API int fs15_scene_frustum_point(const FS15SceneFrustum*,const float*,int*);
FS15_CORE_API int fs15_scene_frustum_sphere(const FS15SceneFrustum*,const float*,int*);
FS15_CORE_API int fs15_scene_frustum_box(const FS15SceneFrustum*,const float*,const float*,int*);
FS15_CORE_API int fs15_scene_clip_point(const FS15SceneClipPlanes*,const float*,int*);
FS15_CORE_API int fs15_scene_clip_sphere(const FS15SceneClipPlanes*,const float*,int*);
FS15_CORE_API int fs15_scene_clip_box(const FS15SceneClipPlanes*,const float*,const float*,int*);
/* Known volume tables: 0x1404c7cc0 (six planes) and 0x1404c7da8 (dynamic).
 * LOD cache mutation precedes distance/volume rejection. Near/far operands and
 * strictness retain native order. sqrtf is the platform backend, not recovered
 * CRT startup. Mutable LOD records and outputs must not alias inputs. */
FS15_CORE_API int fs15_scene_spatial_entry_visible(const void*,const float*,float,const FS15SceneSpatialEntry*,FS15SceneLodManager*,int*);
/* Queries preserve native depth-first cell/row order, skip rejected subtrees,
 * count EVERY match and write only the first capacity owner-token pointers.
 * They do not allocate, sort, deduplicate or select by other render masks.
 * Output capacity may be zero. Malformed storage, nonfinite inputs, dynamic
 * plane counts above 65535 and invalid forward skips reject. On failure,
 * already written output/cache prefixes may remain; no transaction promised. */
FS15_CORE_API int fs15_scene_spatial_volume_query(const FS15SceneSpatialGrid*,const void*,const float*,float,FS15SceneLodManager*,uint32_t**,uint32_t,uint32_t*);
FS15_CORE_API int fs15_scene_spatial_point_query(const FS15SceneSpatialGrid*,const float*,FS15SceneLodManager*,uint32_t**,uint32_t,uint32_t*);
#ifdef __cplusplus
}
static_assert(sizeof(FS15SceneFrustum)==0x68);
static_assert(sizeof(FS15SceneClipPlanes)==0x18);
#endif
