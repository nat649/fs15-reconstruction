#pragma once
#include "scene_camera.h"
#include "scene_visibility.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Inspected prefixes; opaque bytes are retained. Parameter names/units in the
 * render-settings record remain unresolved. Not a complete render context. */
typedef struct FS15SceneViewSettings {
    uint8_t unknown00[0x20];float values20[3],unknown2c,scalar30,scalar34;
    float values38[3],unknown44,scalar48,scalar4c,scalar50,values54[3],scalar60,scalar64;
} FS15SceneViewSettings;
typedef struct FS15SceneRenderView {
    FS15SceneFrustum frustum;
    float world[16],inverse[16],projection[16],combined[16];
    float position[4],basis[3][3],unknown19c;
    uintptr_t camera,volume;
    uint8_t unknown1b0[0x20];
    const FS15SceneViewSettings* settings;
    uintptr_t buffer,secondary_buffer;
} FS15SceneRenderView;
/* Native map/unmap slots +8/+16 are represented by authored callbacks. The
 * returned mapped storage must be disjoint from inputs and hold >=49 floats.
 * A successful nonnull map is always balanced, including short-capacity errors.
 * world refreshes the caller's Camera/cache; optional tangent supplies a math
 * backend (default platform tanf, not a recovered CRT). No callback may reenter
 * or throw. Identity pointers in records are tokens and never executed. */
typedef struct FS15SceneViewServices {
    void* user;
    int (*world)(void*,FS15SceneStateRecord*,float*);
    float* (*map)(void*,uintptr_t,size_t*);
    int (*unmap)(void*,uintptr_t);
    float (*tangent)(void*,float);
} FS15SceneViewServices;
/* 0x14012cca0: position/normalized basis/self-volume and the native 49-float
 * shader block, including double milliseconds * 0.001 -> float. Opaque lanes
 * and mapped spares remain unchanged. Normalization and transpose are native;
 * finite/capacity guards and callback transport are authored. Failure after map
 * may retain view/cache/output mutations. This is not a GPU driver/backend. */
FS15_CORE_API int fs15_scene_view_upload(FS15SceneRenderView*,size_t,double,const FS15SceneViewServices*);
/* 0x1400b3e20: cached/refreshed Camera world, basis conditioning, projection,
 * stereo displacement/off-axis shift, inverse, product, frustum and upload.
 * Camera identity is stored AFTER unmap, as in the native routine. Singular
 * inverses retain prior inverse bytes. aspect/stereo/eye and elapsed time are
 * bounded caller inputs, replacing the original context/global transport.
 * World callback changes can remain after a later rejection. */
FS15_CORE_API int fs15_scene_view_prepare(FS15SceneRenderView*,size_t,FS15SceneCameraRecord*,float,uint8_t,uint8_t,double,const FS15SceneViewServices*);
#ifdef __cplusplus
}
static_assert(sizeof(FS15SceneViewSettings)==0x68);
static_assert(sizeof(FS15SceneRenderView)==0x1e8);
static_assert(offsetof(FS15SceneRenderView,world)==0x68);
static_assert(offsetof(FS15SceneRenderView,position)==0x168);
static_assert(offsetof(FS15SceneRenderView,camera)==0x1a0);
static_assert(offsetof(FS15SceneRenderView,settings)==0x1d0);
#endif
