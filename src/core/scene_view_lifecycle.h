#pragma once
#include "scene_view.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Inspected context prefix: settings + opaque state + viewport dimensions. */
typedef struct FS15SceneViewContext {
    FS15SceneViewSettings settings;
    uint8_t unknown68[0x10];
    uint32_t width,height;
} FS15SceneViewContext;
/* Authored transport for driver slot +0x220 and buffer deleting slot +0.
 * Native descriptor +4 is uninitialized and intentionally not exposed.
 * Callbacks must not throw/reenter. Context outlives the constructed view. */
typedef struct FS15SceneViewLifetimeServices {
    void* user;
    uintptr_t (*create)(void*,uint32_t bytes,uintptr_t initial_data,uint32_t flags);
    void (*destroy)(void*,uintptr_t token,uint32_t deleting);
} FS15SceneViewLifetimeServices;
/* 0x14012ca00. Preserves opaque bytes and the combined matrix. Null buffer
 * returns are retained, matching native behavior; success means initialization
 * completed, not that a usable GPU exists. Never call twice without teardown. */
FS15_CORE_API int fs15_scene_view_construct(FS15SceneRenderView*,size_t,
    const FS15SceneViewContext*,size_t,const FS15SceneViewLifetimeServices*);
/* 0x14012cc40. Releases nonnull buffers in order then changes base vtable.
 * Native tokens remain unchanged: destruction must be called exactly once. */
FS15_CORE_API int fs15_scene_view_destruct(FS15SceneRenderView*,size_t,
    const FS15SceneViewLifetimeServices*);
#ifdef __cplusplus
}
static_assert(sizeof(FS15SceneViewContext)==0x80);
static_assert(offsetof(FS15SceneViewContext,width)==0x78);
#endif
