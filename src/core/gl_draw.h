#pragma once
#include "gl_buffers.h"
typedef struct FS15GLDrawState {
    uint8_t unknown00[0x160];FS15GLIndexBuffer* index;
    uint8_t unknown168[0x20];uintptr_t layout;
    uint8_t unknown190[0x3c];float point_size;
} FS15GLDrawState;
typedef struct FS15GLDrawServices {
    void* user;
    void (*point_size)(void*,float);
    void (*arrays)(void*,uint32_t,int32_t,int32_t);
    void (*elements)(void*,uint32_t,int32_t,uint32_t,uintptr_t);
    void (*range)(void*,uint32_t,uint32_t,uint32_t,int32_t,uint32_t,uintptr_t);
} FS15GLDrawServices;
#ifdef __cplusplus
extern "C" {
#endif
// 0x140065540. Context, attributes/program and index binding are supplied by
// the caller. Primitive IDs 0..4: triangles, strip, lines, line-strip, points.
// Counts/offsets retain original 32-bit wrapping; bounds are a caller contract.
// Null/invalid service guards are authored transport boundaries.
FS15_CORE_API int fs15_gl_draw(FS15GLDrawState*,int32_t count,int32_t first,uint32_t primitive,uint32_t minimum,uint32_t range_count,FS15GLDrawServices*);
#ifdef __cplusplus
}
static_assert(offsetof(FS15GLDrawState,index)==0x160&&offsetof(FS15GLDrawState,layout)==0x188&&offsetof(FS15GLDrawState,point_size)==0x1cc);
#endif
