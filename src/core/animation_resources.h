#pragma once
#include "animation.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Recovered Euler-to-WXYZ conversion, variable-size key packing, ordered key
 * insertion, lookup generation, channel append and clip registration. These
 * use caller-owned valid native-layout graphs and host allocator services. */
FS15_CORE_API int fs15_animation_euler_quaternion(const float*,float*,size_t);
FS15_CORE_API int fs15_animation_key_create(float,uint32_t,const float*,const float*,
    const float*,int,const FS15SceneLifecycleServices*,FS15AnimationKey**);
/* diagnostic=1: non-increasing time; =2: changed low-four control bits.
 * Rejected keys remain caller-owned, matching the original insertion boundary.
 * The original logger is replaced with an explicit diagnostic. */
FS15_CORE_API int fs15_animation_channel_add_key(FS15AnimationChannel*,FS15AnimationKey*,
    const FS15SceneLifecycleServices*,int* diagnostic);
FS15_CORE_API int fs15_animation_channel_finalize(FS15AnimationChannel*);
FS15_CORE_API int fs15_animation_clip_add_channel(FS15AnimationClip*,FS15AnimationChannel*,const FS15SceneLifecycleServices*);
FS15_CORE_API int fs15_animation_set_add_clip(FS15CharacterSet*,FS15AnimationClip*,const FS15SceneLifecycleServices*);
/* Platform file access, XML dispatch and node-ID map integration are authored
 * transport. The binary payload algorithm is recovered from 0x1401ca840.
 * resolve() must return a valid live node or null. loaded() sees each group
 * immediately after binding: nodes own nonzero reference counts. A zero-node
 * group has count zero and is transferred to the host for explicit cleanup.
 * A pointer from an earlier group can be invalidated by later node rebinding;
 * retaining it requires an explicit external reference. Do not reenter loading.
 * diagnostic=0 success, 1/2 native binding warning, 3 missing node (set=null),
 * 4 incomplete bind after host allocation failure (preceding mutations remain).
 * Native exceptions, logger effects and a complete scene loader are not supplied. */
typedef struct FS15AnimationResourceServices {
    void* user;
    const FS15SceneLifecycleServices* memory;
    FS15SharedSceneContext* context;
    FS15SceneStateRecord* (*resolve)(void*,uint32_t);
    void (*loaded)(void*,FS15CharacterSet*,uint32_t);
} FS15AnimationResourceServices;
typedef struct FS15AnimationLoadStats {
    size_t consumed;
    uint32_t groups,loaded,skipped,binding_warnings;
} FS15AnimationLoadStats;
/* The file wrapper accepts the observed big-endian version 2/3 headers.
 * has_version=0 accepts the native entry's payload, starting at group count.
 * Truncation/count/finite/monotonic guards preflight the whole input before
 * engine allocation. Stats stay unchanged if preflight fails. Bytes following
 * the parsed payload are allowed and reported through consumed. */
FS15_CORE_API int fs15_animation_load_binary(const void*,size_t,int has_version,
    const FS15AnimationResourceServices*,FS15AnimationLoadStats*);
#ifdef __cplusplus
}
#endif
