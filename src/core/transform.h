#pragma once
#include "export.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Recovered 0x1400bfec0: native object bytes 0x60..0x9f contain three
 * padded basis rows and a translation vector. Output is four floats (w=0).
 * Input/output may alias. Caller owns the bounded object and buffers. */
FS15_CORE_API int fs15_transform_point(const void* object, size_t object_bytes,
    const float* point, float* result, size_t result_floats);
#ifdef __cplusplus
}
#endif
