#pragma once
#include "scene_lifecycle.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct FS15SceneVector {uintptr_t begin,end,capacity;} FS15SceneVector;
/* Observed update-list fields, excluding the Windows critical section. Calls
 * using one context must be serialized by the host. Unknown bytes stay opaque. */
typedef struct FS15SharedSceneContext {
    FS15SceneVector active;uint64_t unknown18;
    FS15SceneVector pending;uint64_t unknown38;
} FS15SharedSceneContext;
typedef struct FS15CharacterData {
    uint64_t vtable;uint32_t references,unknown0c,flags,unknown14;
    FS15SceneVector translation;uint64_t unknown30;
    FS15SceneVector rotation;uint64_t unknown50;
    FS15SceneVector scale;uint64_t unknown70;
    FS15SceneVector visibility_words;uint64_t unknown90;
    size_t visibility_count;FS15SceneVector clips;uint64_t unknownb8;
} FS15CharacterData;
typedef struct FS15CharacterSet {
    uint8_t object_header[0x28];
    uint64_t reference_table;uint32_t references,unknown34;
    uint64_t observer_table;uint8_t pending,unknown41[7];
    FS15CharacterData* data;uint16_t flags;uint8_t unknown52[2];
    float state[20];uint32_t unknowna4;
    FS15SceneVector transforms;uint64_t unknownc0;
    uint8_t registered_observer,unknownc9[7];
} FS15CharacterSet;
/* Valid owned native-layout records only. Stable vtable identity tokens are
 * never called as original C++ function pointers. Platform allocator, object
 * registry/ID and synchronization are host services, not recovered CRT/OS code.
 * CharacterSet constructors retain supplied data or create it if null, and
 * register their embedded +0x38 update observer. They start with zero transform
 * references; binding increments the +0x30 count and sets node.shared_resource
 * to the embedded +0x28 reference interface. Failures after external callbacks
 * or member mutation starts cannot roll back all ownership changes. */
FS15_CORE_API int fs15_character_data_construct(void*,size_t);
FS15_CORE_API int fs15_character_data_destruct(void*,size_t,uint32_t,const FS15SceneLifecycleServices*);
FS15_CORE_API int fs15_character_set_construct(void*,size_t,const char*,size_t,
    FS15CharacterData*,FS15SharedSceneContext*,const FS15SceneLifecycleServices*);
FS15_CORE_API int fs15_character_set_destruct(void*,size_t,uint32_t,
    FS15SharedSceneContext*,const FS15SceneLifecycleServices*);
/* Release the known data/set reference interface: unsigned count <2 dispatches
 * deleting destruction, otherwise decrement. No generic derived-class support.
 * resource_bytes covers the supplied interface (0x10 for a set's +0x28 view).
 * A set's containing owned 0xd0 allocation must remain valid. */
FS15_CORE_API int fs15_shared_scene_release(void*,size_t,FS15SharedSceneContext*,const FS15SceneLifecycleServices*);
/* Bind original TRS nodes and optional default poses. Preserves prefix updates
 * on native warning conditions. diagnostic=1 for a non-TRS node, =2 for a clip
 * with fewer channels; the original logger is not invoked. Invalid/null node
 * pointers are outside native defined behavior and require caller validation.
 * Callback for old shared references must dispatch known resource ownership.
 * This does not evaluate animations, callbacks, skinning or renderer resources. */
FS15_CORE_API int fs15_character_set_bind(FS15CharacterSet*,FS15SceneStateRecord* const*,size_t,
    FS15SharedSceneContext*,const FS15SceneLifecycleServices*,int* diagnostic);
FS15_CORE_API int fs15_scene_property_clone(const FS15ScenePropertyView*,size_t,
    const FS15SceneLifecycleServices*,FS15ScenePropertyView**);
FS15_CORE_API int fs15_scene_property_replace(void*,size_t,FS15ScenePropertyView*,const FS15SceneLifecycleServices*);
#ifdef __cplusplus
}
static_assert(sizeof(FS15CharacterData)==0xc0);
static_assert(offsetof(FS15CharacterData,clips)==0xa0);
static_assert(sizeof(FS15CharacterSet)==0xd0);
static_assert(offsetof(FS15CharacterSet,data)==0x48);
static_assert(offsetof(FS15CharacterSet,transforms)==0xa8);
#endif
