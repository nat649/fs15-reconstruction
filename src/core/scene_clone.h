#pragma once
#include "scene_camera.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct FS15SceneCloneServices {
    FS15SceneXmlServices scene;
    int (*refresh)(void*,FS15SceneStateRecord*);
    int (*constructed)(void*,FS15SceneStateRecord*,unsigned,size_t);
} FS15SceneCloneServices;
/* Recovered 0x14010fd80 copy into an existing supported TRS/LOD/Camera/Light node.
 * Refreshes source caches, rebuilds target, clones/replaces known properties,
 * propagates individual clip/mask/visibility changes in native order and
 * replaces the attribute map. Identity, children and shared animation bindings
 * are not copied by this native routine. Valid owned maps/properties required.
 * Target properties must be default or detached before replacement. Finite
 * guards authored; callbacks cannot throw/reenter. Failures retain prefixes. */
FS15_CORE_API int fs15_scene_trs_copy(FS15SceneStateRecord*,FS15SceneStateRecord*,const FS15SceneCloneServices*);
/* Recover Camera/LOD/Light clone wrappers; plain TRS construction plus the same copy
 * routine is authored dispatch for the corresponding original loader branch.
 * This clones ONE record, not a complete subtree. constructed must register it
 * with owned transport before copy callbacks run. A completed constructor is
 * returned in result even after later failure and must be released by caller.
 * LOD distances retain the native clone's count inference, including truncation.
 * Full original remap tables/recursive derived resource clone remain open. */
FS15_CORE_API int fs15_scene_clone_node(FS15SceneStateRecord*,const FS15SceneCloneServices*,FS15SceneGroupResult*);
#ifdef __cplusplus
}
#endif
