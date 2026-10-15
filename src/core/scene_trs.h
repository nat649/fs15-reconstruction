#pragma once
#include "export.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* GE6 rotation/composition/decomposition flow. sin/cos/atan2/sqrt use the
 * platform C++ math library, not recovered CRT instruction implementations.
 * Matrix layout matches scene_math.h. Angles are radians; rotation order is
 * original Z, Y, X column updates, followed by per-column scale.
 * Finite inputs required. Outputs may alias inputs and remain intact on failure. */
FS15_CORE_API int fs15_rotate_affine_axis(const float*,size_t,float,unsigned,float*,size_t);
FS15_CORE_API int fs15_compose_trs(const float*,const float*,const float*,float*,size_t);
/* Optional scale vector: zero scale components retain native reciprocal=1.
 * Gimbal branch uses the original 1e-6 threshold and forces Z rotation to zero. */
FS15_CORE_API int fs15_matrix_euler(const float*,size_t,const float*,float*,size_t);
#ifdef __cplusplus
}
#endif
