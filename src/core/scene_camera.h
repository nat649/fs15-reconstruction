#pragma once
#include "scene_groups.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Camera identity and parameter tail observed in the 0x190-byte GE6 object.
 * fov is stored in degrees. Unknown fields retain observed defaults/names;
 * this record does not implement projection, culling or rendering. */
typedef struct FS15SceneCameraRecord {
    FS15SceneStateRecord node;
    float fov,near_clip,far_clip,orthographic_height,unknown180,unknown184;
    uint8_t orthographic,unknown189[7];
} FS15SceneCameraRecord;
FS15_CORE_API int fs15_scene_camera_construct(FS15SceneCameraRecord*,size_t,const char*,size_t,float,float,float,uint8_t,float,const FS15SceneLifecycleServices*);
FS15_CORE_API int fs15_scene_camera_destruct(FS15SceneCameraRecord*,size_t,uint32_t,const FS15SceneLifecycleServices*);
/* Same ownership/prefix contract as group_begin, kind=1 and bytes=0x190.
 * Camera common attributes use physics=0, registration=1, joint=0. */
FS15_CORE_API int fs15_scene_camera_begin(FS15SceneStateRecord**,const FS15SceneXmlAttribute*,size_t,const FS15SceneGroupServices*,FS15SceneGroupResult*);
/* Script frame: name pointer +0, fov +0x10, near +0x20, far +0x30.
 * name_bytes bounds the pointed-to string. Frame/strings must be valid,
 * disjoint storage. Caller owns the completed allocation returned in output. */
FS15_CORE_API int fs15_scene_camera_script_create(const void*,size_t,size_t,const FS15SceneLifecycleServices*,FS15SceneCameraRecord**);
typedef struct FS15SceneCameraServices {
    void* user;
    int (*world)(void*,FS15SceneStateRecord*,float*); /* 16 floats, refresh cache */
    int (*refresh)(void*,FS15SceneStateRecord*);
    int (*rebuild)(void*,FS15SceneStateRecord*);
} FS15SceneCameraServices;
/* Recovered in-place three-vector normalization, using the recovered reciprocal
 * square-root seed table and two native double refinement steps. Finite result
 * required; rejected output is unchanged (zero normalizes to zero natively). */
FS15_CORE_API int fs15_scene_normalize3(float*,size_t);
/* Recovered world pose -> parent-relative Euler/translation. Camera scale is
 * retained. Singular parent inverse retains native preinitialized identity.
 * Rotation and translation each trigger a separate rebuild/propagation round.
 * Finite/bounds guards and callback transport are authored. Callback failures
 * can retain prefix/cache mutations; callbacks cannot throw or reenter. */
FS15_CORE_API int fs15_scene_camera_pose(FS15SceneCameraRecord*,const float*,size_t,const FS15SceneCameraServices*);
FS15_CORE_API int fs15_scene_camera_follow(FS15SceneCameraRecord*,const float*,size_t,float,const FS15SceneCameraServices*);
FS15_CORE_API int fs15_scene_camera_smooth(FS15SceneCameraRecord*,const float*,size_t,float,float,float,const FS15SceneCameraServices*);
/* Script follow frame: target XYZ +0/+0x10/+0x20, distance +0x30,
 * alpha +0x40, factor +0x50. Null camera is the original successful no-op. */
FS15_CORE_API int fs15_scene_camera_script_follow(FS15SceneCameraRecord*,const void*,size_t,const FS15SceneCameraServices*);
#ifdef __cplusplus
}
#endif
