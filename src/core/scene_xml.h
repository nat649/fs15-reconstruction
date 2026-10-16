#pragma once
#include "animation_xml.h"
#include "scene_properties.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Decoded attribute transport shared with animation callbacks. Names are exact
 * case, first matching occurrence wins; values may be null. */
typedef FS15AnimationXmlAttribute FS15SceneXmlAttribute;
typedef struct FS15SceneXmlServices {
    void* user;const FS15SceneLifecycleServices* memory;
    int (*rebuild)(void*,FS15SceneStateRecord*);
    int (*propagate)(void*,FS15SceneStateRecord*,uint32_t,int32_t);
    int (*integer)(void*,const char*,uint32_t*); /* Original static CRT strtol seam. */
    int (*register_node)(void*,uint32_t,FS15SceneStateRecord*);
} FS15SceneXmlServices;
FS15_CORE_API int fs15_scene_xml_find(const FS15SceneXmlAttribute*,size_t,const char*,size_t,const char**);
/* Recover 0x140194460 and its empty-scratch-name wrapper 0x14018c100.
 * For the wrapper pass fallback_name="". Flags retain original physics,
 * node-ID registration and joint switches. Fresh detached body/joint metadata
 * is supported; live PhysX actors, registered property observers and backend
 * ownership are not created or simulated. A joint replaces a preceding body
 * property in native order. Native trigger teardown remains a separate boundary.
 * TRS triples use three scalar cursor reads followed by +1, not the animation
 * vector splitter. Callback transport must call recovered known scene services;
 * callbacks cannot throw/reenter. Prefix mutations can remain after failures.
 * Valid supported node/attribute storage required; finite/bounds checks authored.
 * Registration transport must retain the original first-ID-wins behavior. */
FS15_CORE_API int fs15_scene_xml_apply(FS15SceneStateRecord*,const FS15SceneXmlAttribute*,size_t,
    int physics,int register_id,int joint,const char* fallback_name,size_t,
    const FS15SceneXmlServices*);
#ifdef __cplusplus
}
#endif
