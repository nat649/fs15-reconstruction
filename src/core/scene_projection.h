#pragma once
#include "export.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Recovered off-axis perspective (0x140042800) and orthographic (0x140042930)
 * matrices, column-major, native clip depth -1..1. Bounds are left/right,
 * bottom/top, near/far. Reversed bounds and finite negative distances retain
 * native behavior; zero spans and nonfinite results reject. Authored bounds
 * checks and staged writes leave output unchanged on failure. */
FS15_CORE_API int fs15_scene_perspective(float,float,float,float,float,float,float*,size_t);
FS15_CORE_API int fs15_scene_orthographic(float,float,float,float,float,float,float*,size_t);
/* Full projective 4x4 product (0x14001b060), including fourth-row terms that
 * affine_multiply does not handle. Native arithmetic grouping is retained.
 * Inputs/output may alias: staging is authored, original aliasing is not used. */
FS15_CORE_API int fs15_scene_projective_multiply(const float*,size_t,const float*,size_t,float*,size_t);
/* In-place basis conditioning (0x1400b2130) used during Camera view preparation.
 * Preserve all seven non-basis lanes. Native comparisons use the SAME interval
 * [0.99999f,1.00001f] for inverse lengths AND dot products; no epsilon-around-
 * zero replacement. Degenerate finite vectors retain native seed behavior.
 * Does not recover the complete render view, stereo handling or GPU upload. */
FS15_CORE_API int fs15_scene_camera_basis(float*,size_t);
#ifdef __cplusplus
}
#endif
