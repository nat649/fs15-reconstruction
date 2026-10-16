#pragma once
#include "scene_lifecycle.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct FS15SceneAttributeValue {uint64_t bits;uint32_t kind,unknown0c;} FS15SceneAttributeValue;
typedef struct FS15SceneAttributeNode {
    struct FS15SceneAttributeNode *left,*parent,*right;
    uint8_t key_storage[16];
    size_t key_length,key_capacity;
    uint64_t unknown38;
    FS15SceneAttributeValue* value;
    uint8_t color,nil,unknown4a[6];
} FS15SceneAttributeNode;
typedef struct FS15SceneAttributeStrings {
    const char* key;size_t key_bytes;
    const char* type;size_t type_bytes;
    const char* value;size_t value_bytes;
} FS15SceneAttributeStrings;
/* Valid owned native-layout trees only; member pointers are trusted, not
 * arbitrary untrusted input. Strings are bounded NUL-terminated byte strings.
 * Platform allocation is supplied through lifecycle services. Lookup/ensure
 * preserve case-sensitive unsigned-byte ordering and original value storage.
 * Existing ensure entries retain their type even when a new kind is requested.
 * New keys preserve the observed inline-15/heap capacity and RB tree layout. */
FS15_CORE_API int fs15_scene_attribute_find(const void*,size_t,const char*,size_t,FS15SceneAttributeValue**);
FS15_CORE_API int fs15_scene_attribute_ensure(void*,size_t,const char*,size_t,uint32_t,
    const FS15SceneLifecycleServices*,FS15SceneAttributeValue**);
/* Getter/setter original frame adapters. frame pointers +0/+0x10/+0x20 must
 * match the bounded strings where applicable. Getter writes only +0xf0 and tag
 * +0xf8. Setter type names are exactly Boolean/Integer/Float/String. Integer
 * conversion reproduces CVTTSS2SI (INT_MIN for NaN/out-of-range). Null-node
 * setter is a no-op; null/missing getter clears only its result tag.
 * Native ownership quirks are retained: String->scalar overwrites without
 * freeing its former text, and map cleanup frees text only for kind 3, not 4.
 * The owning allocator/context must track such orphaned allocations separately.
 * Self-aliasing string replacement is rejected before freeing its source. */
FS15_CORE_API int fs15_scene_attribute_frame(void*,size_t,unsigned,const FS15SceneAttributeStrings*,
    void*,size_t,const FS15SceneLifecycleServices*);
/* Validate/clear native attribute storage. release_header=0 retains an empty
 * sentinel; =1 reproduces 0x1401365f0, leaving the node's header pointer stale
 * as in native destruction. Do not reuse/destruct a released map twice.
 * Per-object validation precedes external frees; unknown value kinds retain
 * native cleanup behavior. No arbitrary derived resource destructors run. */
FS15_CORE_API int fs15_scene_attributes_validate(const void*,size_t);
FS15_CORE_API int fs15_scene_attributes_clear(void*,size_t,int,const FS15SceneLifecycleServices*);
/* Recovered value/map cloning. Clone zero-initializes scalar payload storage,
 * copies only the native active lane and duplicates kinds 3 and 4 strings.
 * Kind 4 text still has the original map-cleanup retention quirk. Values/maps
 * must be valid owned storage, with terminated string pointees. Map assignment
 * clears the destination then inserts source keys in sorted order; self-copy
 * therefore clears the map, as in GE6. Failures retain completed prefixes.
 * Distinct owners must not share map/member allocations. */
FS15_CORE_API int fs15_scene_attribute_value_clone(const FS15SceneAttributeValue*,size_t,const FS15SceneLifecycleServices*,FS15SceneAttributeValue**);
FS15_CORE_API int fs15_scene_attributes_copy(void*,size_t,const void*,size_t,const FS15SceneLifecycleServices*);
#ifdef __cplusplus
}
static_assert(sizeof(FS15SceneAttributeValue)==0x10);
static_assert(offsetof(FS15SceneAttributeNode,key_storage)==0x18);
static_assert(offsetof(FS15SceneAttributeNode,value)==0x40);
static_assert(offsetof(FS15SceneAttributeNode,color)==0x48);
static_assert(sizeof(FS15SceneAttributeNode)==0x50);
#endif
