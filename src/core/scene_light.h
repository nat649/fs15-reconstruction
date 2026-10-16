#pragma once
#include "scene_groups.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct FS15SceneSpatialSphere {
    uintptr_t vtable;uint32_t unknown08;float center[3],radius;uint32_t unknown1c;
} FS15SceneSpatialSphere;
typedef struct FS15SceneLightRecord {
    FS15SceneStateRecord node;uint32_t spatial_token,type;
    float diffuse[3],specular[3],range,drop_off,cone_angle,outer_cos,inner_cos;
    float depth_bias,depth_slope_bias,depth_slope_clamp;uint32_t depth_resolution;
    float shadow_far,shadow_extrusion,shadow_perspective;uint32_t shadow_splits;
    float shadow_split_parameter;uint32_t decay,affected_masks,light_flags,lod_token;
    FS15SceneSpatialSphere spatial;uintptr_t render_resources;
} FS15SceneLightRecord;
typedef struct FS15SceneLightParameters {
    uint32_t type;float diffuse[3],specular[3];
    uint8_t emit_diffuse,emit_specular,cast_shadow,perspective;
    float depth_bias,depth_slope_bias,depth_slope_clamp;uint32_t depth_resolution;
    float range;uint32_t decay;float cone_angle,drop_off,shadow_far,shadow_extrusion,shadow_perspective;
    uint32_t shadow_splits;float shadow_split_parameter;
} FS15SceneLightParameters;
typedef struct FS15SceneLightContext {FS15SceneVector pending;} FS15SceneLightContext;
/* Native constructor/dirty and pending-update append. Float math uses platform
 * cosf; native trig backend itself is not recovered. Construction initializes
 * ordinary hidden TRS flags and does not enqueue; context may be null only when
 * no enqueue is requested. All record/pointee storage must be valid owned data.
 * Queuing failures keep native-order flag mutations. Finite/bounds guards added. */
FS15_CORE_API int fs15_scene_light_construct(FS15SceneLightRecord*,size_t,const char*,size_t,const FS15SceneLightParameters*,FS15SceneLightContext*,const FS15SceneLifecycleServices*);
FS15_CORE_API int fs15_scene_light_dirty(FS15SceneLightRecord*,FS15SceneLightContext*,const FS15SceneLifecycleServices*);
FS15_CORE_API int fs15_scene_light_enqueue(FS15SceneLightContext*,FS15SceneLightRecord*,const FS15SceneLifecycleServices*);
/* Recovered virtual post-state body, called AFTER the known base traversal.
 * This entry is transport for the post phase; scene_state_derived supplies the
 * complete virtual walk. Input mask is the original mask, before suppression. */
FS15_CORE_API int fs15_scene_light_post(FS15SceneLightRecord*,uint32_t,FS15SceneLightContext*,const FS15SceneLifecycleServices*);
/* Known inherited marker dispatch on a valid subtree without reading its
 * parents/properties. Light override updates its cached token before children;
 * other inspected tables retain base marker behavior. Prefixes remain on error. */
FS15_CORE_API int fs15_scene_light_mark_tree(FS15SceneStateRecord*,uint32_t,FS15SceneLightContext*,const FS15SceneLifecycleServices*);
/* Pending-only lifetime: no active spatial registration or render_resources.
 * Remove queued node by swap-last, retain native flag bytes, then inherited TRS
 * destruction. Active spatial/GPU ownership remains an unsupported boundary. */
FS15_CORE_API int fs15_scene_light_destruct(FS15SceneLightRecord*,size_t,uint32_t,FS15SceneLightContext*,const FS15SceneLifecycleServices*);
typedef struct FS15SceneLightXmlServices {
    FS15SceneGroupServices scene;
    int (*unsigned_integer)(void*,const char*,uint32_t*); /* original CRT strtoul, base 0 */
} FS15SceneLightXmlServices;
/* Decoded Light begin callback. Same constructed/retained-prefix contract as
 * group_begin, kind=3/bytes=0x200. Common loading uses physics=0, IDs=1,joint=0.
 * Native case/boolean/numeric grammar, shadow/spot conditionals retained. */
FS15_CORE_API int fs15_scene_light_begin(FS15SceneStateRecord**,const FS15SceneXmlAttribute*,size_t,const FS15SceneLightXmlServices*,FS15SceneLightContext*,FS15SceneGroupResult*);
#ifdef __cplusplus
}
static_assert(sizeof(FS15SceneLightRecord)==0x200);
static_assert(offsetof(FS15SceneLightRecord,lod_token)==0x1d4);
static_assert(offsetof(FS15SceneLightRecord,spatial)==0x1d8);
#endif
