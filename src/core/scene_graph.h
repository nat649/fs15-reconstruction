#pragma once
#include "export.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Bounded original-layout caller-owned arena: parent pointer at +0x28,
 * uint32 flags at +0x50, local matrix +0x98, cached world matrix +0xd8.
 * Every traversed parent must be exactly a record start in this same arena.
 * Only dirty bit 0x40 and world matrices change. Cached ancestors stop traversal.
 * These APIs do not allocate engine objects or implement lifecycle/dirty marking. */
FS15_CORE_API int fs15_scene_update_world(void*,size_t,size_t,size_t);
enum FS15SceneTransform {
    FS15_LOCAL_TO_WORLD=0,FS15_LOCAL_DIRECTION_TO_WORLD=1,
    FS15_WORLD_TO_LOCAL=2,FS15_WORLD_DIRECTION_TO_LOCAL=3
};
/* Complete native script frame: input floats +0,+0x10,+0x20, output values
 * +0xf0,+0x100,+0x110 and uint16 type tags +8 (numeric tag 3).
 * Singular inverse requests reject instead of exposing native uninitialized data.
 * Frame must be separate from the arena. Finite arithmetic required. */
FS15_CORE_API int fs15_scene_transform_frame(unsigned,void*,size_t,size_t,size_t,void*,size_t);
#ifdef __cplusplus
}
#endif
