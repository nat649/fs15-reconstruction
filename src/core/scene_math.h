#pragma once
#include "export.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* GE6 affine layout: column-major 3x3 basis at 0..10 (stride 4),
 * translation at 12..14. Padding lanes are not a general projective matrix.
 * Authored bounded APIs stage inputs/outputs, permit aliasing, and leave output
 * unchanged on rejection. Finite inputs and sufficient element counts required. */
FS15_CORE_API int fs15_affine_multiply(const float*,size_t,const float*,size_t,float*,size_t);
FS15_CORE_API int fs15_affine_inverse(const float*,size_t,float*,size_t);
/* Native 0x14009e0a0 transposes the basis after subtracting translation. This
 * is an inverse only for orthonormal bases; no scale/shear correction is added. */
FS15_CORE_API int fs15_rigid_inverse_point(const float*,size_t,const float*,float*,size_t);
/* Native quaternion order is W, X, Y, Z. Matrix conversion writes ONLY the
 * nine basis lanes, preserving existing translation/padding. No normalization. */
FS15_CORE_API int fs15_quaternion_to_matrix(const float*,size_t,float*,size_t);
FS15_CORE_API int fs15_matrix_to_quaternion(const float*,size_t,float*,size_t);
FS15_CORE_API int fs15_matrix_scale(const float*,size_t,float*,size_t);
#ifdef __cplusplus
}
#endif
