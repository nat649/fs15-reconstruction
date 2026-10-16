#pragma once
#include "shared_scene.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Observed GE6 layouts. Keys have an eight-byte header followed by variable
 * payload: translation at +8, signed int16 XYZW at +8+rotation_offset,
 * scale at +8+scale_offset. The caller owns readable, correctly sized decoded
 * keys and all vectors/pointees; these are not untrusted-file parsers. */
typedef struct FS15AnimationKey {
    float time;uint16_t flags;uint8_t rotation_offset,scale_offset;
} FS15AnimationKey;
typedef struct FS15AnimationChannel {
    uint16_t node_index,flags;uint32_t unknown04;
    FS15SceneVector keys;uint64_t unknown20;
    uint16_t lookup[64];float index_scale,last_time,weights[3];uint32_t unknownbc;
    const FS15AnimationKey* first;const FS15AnimationKey* second;
} FS15AnimationChannel;
typedef struct FS15AnimationClip {
    float duration;uint32_t unknown04;uint8_t name[32];uint64_t unknown28;
    FS15SceneVector channels;
} FS15AnimationClip;
/* Recovered seed generation, fast normalization, shortest-path normalized
 * linear interpolation and compressed channel rotation. No original table
 * bytes are embedded. sqrt/fmod use the platform math library; original CRT
 * bodies are not recovered. Finite input/output is required. Results are
 * staged, may alias inputs, and remain unchanged on failure. Zero quaternion
 * normalization retains native zero output (it does not invent an identity). */
FS15_CORE_API int fs15_animation_rsqrt_table(uint32_t*,size_t);
FS15_CORE_API int fs15_animation_normalize(float*,size_t);
FS15_CORE_API int fs15_animation_nlerp(const float*,const float*,float,float*,size_t);
FS15_CORE_API int fs15_animation_rotation(const FS15AnimationChannel*,float*,size_t);
/* Select keys and interpolation weights in every channel. Exact duration
 * endpoints remain at duration even when looping. Only valid sorted keys and
 * acceleration indices are supported; malformed indices/empty channels reject
 * before committing any channel output. Positive finite duration is required. */
FS15_CORE_API int fs15_animation_sample(FS15AnimationClip*,float,int);
/* Four-track native state controls. flags/clip indices retain their original
 * bit representations in CharacterSet.state[0..7]. Time stores the requested
 * value, while dirty detection compares clamped values, including on loops. */
FS15_CORE_API int fs15_animation_set_time(FS15CharacterSet*,uint32_t,float);
FS15_CORE_API int fs15_animation_set_weight(FS15CharacterSet*,uint32_t,float);
FS15_CORE_API int fs15_animation_set_clip(FS15CharacterSet*,uint32_t,uint32_t);
FS15_CORE_API int fs15_animation_find_clip(const FS15CharacterSet*,const char*,size_t,uint32_t*);
/* Authored transport for native virtual slots. Hosts serialize updates and
 * supply known TRS refresh, matrix setter and state propagation services.
 * No arbitrary native function pointers are executed. Callback side effects,
 * reentrancy and scheduling are outside this bounded reconstruction. A callback
 * failure preserves preceding native-order mutations; it is not a transaction.
 * Updates shared by multiple tracks preserve native last-sample-wins behavior. */
typedef struct FS15AnimationServices {
    void* user;
    int (*refresh)(void*,FS15SceneStateRecord*);
    int (*set_matrix)(void*,FS15SceneStateRecord*,const float*);
    int (*propagate)(void*,FS15SceneStateRecord*,uint32_t,int32_t);
} FS15AnimationServices;
FS15_CORE_API int fs15_animation_update(FS15CharacterSet*,float,const FS15AnimationServices*);
#ifdef __cplusplus
}
static_assert(sizeof(FS15AnimationKey)==8);
static_assert(sizeof(FS15AnimationChannel)==0xd0);
static_assert(offsetof(FS15AnimationChannel,lookup)==0x28);
static_assert(offsetof(FS15AnimationChannel,weights)==0xb0);
static_assert(offsetof(FS15AnimationChannel,first)==0xc0);
static_assert(sizeof(FS15AnimationClip)==0x48);
#endif
