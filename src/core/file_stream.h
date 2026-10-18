#pragma once
#include "export.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Authored OS seams for recovered 0x14004f310/410/460/4b0. Records retain the
 * native x64 handle at +8 and EOF byte at +0x10. At least 0x11 bytes required.
 * UINT64_MAX is the invalid handle. Other bytes are never changed here.
 * Callbacks return the same DWORD/count conventions as the inspected APIs.
 * No native constructor, string ownership, destructor or full VFS is implied. */
typedef struct FS15_FileServices {
    void* user;
    uint32_t (*read)(void*,uint64_t,void*,uint32_t,uint32_t*);
    uint32_t (*seek)(void*,uint64_t,int32_t,uint32_t);
    uint32_t (*size)(void*,uint64_t);
    void (*corrupt)(void*,const void*,uint32_t);
} FS15_FileServices;
/* Read callback returns zero on success or a Windows-style error code.
 * Errors 0x570/0x571 emit an explicit corruption callback; original logger/
 * global-list behavior is outside this bounded component. */
FS15_CORE_API uint32_t fs15_file_read(void*,size_t,void*,uint32_t,const FS15_FileServices*);
FS15_CORE_API int fs15_file_seek(void*,size_t,uint32_t,const FS15_FileServices*);
FS15_CORE_API uint32_t fs15_file_size(const void*,size_t,const FS15_FileServices*);
FS15_CORE_API uint32_t fs15_file_tell(const void*,size_t,const FS15_FileServices*);
#ifdef __cplusplus
}
#endif
