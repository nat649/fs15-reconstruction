#pragma once
#include "api.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Native 0x1400e76c0. Caller-owned physics registry + scene object + script
 * frame (>=0x130 bytes). Resolves the original wheel ID, tangent of the angle,
 * tire forces, then writes two float/tag-3 return slots. Missing wheel or load
 * <=1e-5 yields zero forces. Native singleton/log transport is not recovered.
 * Optional tangent callback is a math boundary; default is platform tanf.
 * Finite/size/state guards are authored; failure leaves frame unchanged. */
FS15_CORE_API int fs15_script_wheel_forces(const void* manager,const void* object,
    void* frame,size_t bytes,void* user,float (*tangent)(void*,float));
#ifdef __cplusplus
}
#endif
