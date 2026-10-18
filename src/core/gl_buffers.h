#pragma once
#include "export.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Native x64 buffer layouts. Vtable values are identities, never executed. */
typedef struct FS15GLVertexBuffer {
    uintptr_t vtable; uint32_t name,usage,flags;
    uint8_t mapped,staged,unknown16[2]; uint32_t offset,length;
    uint8_t discard,unknown21[3]; uint32_t bytes;
} FS15GLVertexBuffer;
typedef struct FS15GLIndexBuffer {
    uintptr_t vtable; uint32_t type,name,usage,flags;
    uint8_t mapped,unknown19[3]; uint32_t bytes;
} FS15GLIndexBuffer;
typedef struct FS15GLBufferDesc {
    uintptr_t data; uint32_t bytes,flags,format;
} FS15GLBufferDesc;
/* Authored explicit transport for GE6's process-global GL dispatch/counters.
 * Own the current context on this thread. Callbacks must not throw/reenter.
 * Scratch is borrowed storage (native reserves 16 MiB), shared by all buffers.
 * Map modes: 1 discard/write, 2 read, all others write. A vertex staged map
 * returns the start of scratch, NOT scratch+offset. Unmap exactly once.
 * Original null-map+offset behavior is retained; caller checks GL errors.
 * Allocations and all driver/cache/object storage must remain valid throughout.
 */
typedef struct FS15GLBufferServices {
    void* user;
    void (*gen)(void*,int,uint32_t*);
    void (*bind)(void*,uint32_t,uint32_t);
    void (*data)(void*,uint32_t,size_t,uintptr_t,uint32_t);
    void (*subdata)(void*,uint32_t,size_t,size_t,uintptr_t);
    uintptr_t (*map)(void*,uint32_t,uint32_t);
    void (*unmap)(void*,uint32_t);
    void (*erase)(void*,int,const uint32_t*);
    void* (*allocate)(void*,size_t);
    void (*release)(void*,void*);
    uintptr_t scratch; size_t scratch_bytes;
    uint32_t vertex_bytes,index_bytes; uint8_t scratch_busy;
} FS15GLBufferServices;
/* 0x140065c60: driver prefix >=0x190, count+138, borrowed cache+140.
 * Allocation failure is an authored return boundary, not recovered CRT throws. */
FS15_CORE_API FS15GLVertexBuffer* fs15_gl_vertex_create(void*,size_t,const FS15GLBufferDesc*,FS15GLBufferServices*);
FS15_CORE_API int fs15_gl_index_construct(FS15GLIndexBuffer*,const FS15GLBufferDesc*,FS15GLBufferServices*);
FS15_CORE_API int fs15_gl_vertex_destroy(FS15GLVertexBuffer*,uint32_t,FS15GLBufferServices*);
FS15_CORE_API int fs15_gl_index_destroy(FS15GLIndexBuffer*,uint32_t,FS15GLBufferServices*);
FS15_CORE_API uintptr_t fs15_gl_vertex_map(FS15GLVertexBuffer*,uint32_t,uint32_t,int,FS15GLBufferServices*);
FS15_CORE_API uintptr_t fs15_gl_index_map(FS15GLIndexBuffer*,uint32_t,int,FS15GLBufferServices*);
FS15_CORE_API int fs15_gl_vertex_unmap(FS15GLVertexBuffer*,FS15GLBufferServices*);
FS15_CORE_API int fs15_gl_index_unmap(FS15GLIndexBuffer*,FS15GLBufferServices*);
FS15_CORE_API int fs15_gl_vertex_upload(FS15GLVertexBuffer*,uint32_t,uint32_t,uint8_t,FS15GLBufferServices*);
#ifdef __cplusplus
}
static_assert(sizeof(FS15GLVertexBuffer)==0x28);
static_assert(sizeof(FS15GLIndexBuffer)==0x20);
static_assert(offsetof(FS15GLVertexBuffer,bytes)==0x24);
#endif
