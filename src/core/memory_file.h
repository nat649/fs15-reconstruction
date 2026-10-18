#pragma once
#include "export.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Native x64 memory-stream view: data pointer +8, ownership byte +0x10,
 * DWORD size +0x14, DWORD position +0x18. At least 0x1c bytes required.
 * Recovered operations never acquire/release ownership or alter other bytes.
 * Authored pointer/capacity/state guards reject undefined native read cases.
 * Callers must provide live, nonoverlapping input/output storage. */
FS15_CORE_API int fs15_memory_file_valid(const void*,size_t);
FS15_CORE_API uint32_t fs15_memory_file_read(void*,size_t,size_t,void*,size_t,uint32_t);
FS15_CORE_API int fs15_memory_file_seek(void*,size_t,uint32_t);
FS15_CORE_API uint32_t fs15_memory_file_size(const void*,size_t);
FS15_CORE_API uint32_t fs15_memory_file_tell(const void*,size_t);
FS15_CORE_API int fs15_memory_file_eof(const void*,size_t);
#ifdef __cplusplus
}
#endif
