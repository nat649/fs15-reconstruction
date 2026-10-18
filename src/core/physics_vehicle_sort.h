#pragma once
#include "export.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* 0x14027fe40. Native middle-pivot partition/swap order, including equal keys.
 * Authored bounded span and key callback replace native array/vtable transport.
 * Keys must be stable and callbacks must not mutate/reenter the list.
 * At most one million borrowed vehicle tokens. Empty/single spans are no-ops. */
FS15_CORE_API int fs15_physics_vehicle_sort(uintptr_t*,size_t,int (*key)(void*,uintptr_t),void*);
#ifdef __cplusplus
}
#endif
