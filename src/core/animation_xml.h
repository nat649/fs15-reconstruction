#pragma once
#include "animation_resources.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Recovered GE6 decimal grammar, including float rounding after every digit,
 * ASCII-space-only skipping/splitting and lowercase scientific exponents.
 * Bounds, NUL checks and exponent-work limits are authored. Scalar cursor is
 * the original consumed byte offset, not a validation of the whole string.
 * Native nonfinite results are retained; downstream key packing guards them.
 * On rejection outputs stay unchanged. */
FS15_CORE_API int fs15_parse_scene_float(const char*,size_t,float*,size_t* consumed);
FS15_CORE_API int fs15_parse_scene_float_vector(const char*,size_t,float*,size_t,size_t* written);
typedef struct FS15AnimationXmlAttribute {
    const char* name;size_t name_bytes;
    const char* value;size_t value_bytes; /* null represents an absent value */
} FS15AnimationXmlAttribute;
typedef struct FS15AnimationXmlServices {
    FS15AnimationResourceServices resources;
    /* The original callbacks call static CRT atof/strtol wrappers here. These
     * are explicit host services, not reconstructed CRT/locale startup.
     * Conversion callbacks must report failure through zero, not throw. */
    int (*duration)(void*,const char*,float*);
    int (*node_id)(void*,const char*,uint32_t*);
} FS15AnimationXmlServices;
enum FS15AnimationXmlEvent {
    FS15_ANIMATION_XML_BEGIN_SET=0,FS15_ANIMATION_XML_END_SET=1,
    FS15_ANIMATION_XML_BEGIN_CLIP=2,FS15_ANIMATION_XML_END_CLIP=3,
    FS15_ANIMATION_XML_BEGIN_CHANNEL=4,FS15_ANIMATION_XML_END_CHANNEL=5,
    FS15_ANIMATION_XML_KEY=6
};
typedef struct FS15AnimationXmlView {
    FS15CharacterSet* set;FS15AnimationClip* clip;FS15AnimationChannel* channel;
    uint32_t node_id,reserved;size_t node_count,rejected_keys;
} FS15AnimationXmlView;
/* Authored opaque context/attribute/event transport around seven recovered
 * callbacks. The host owns XML tokenization and supplies decoded attributes.
 * Duplicate names resolve to their first exact-case match. Event order must
 * be properly nested; unsupported sequencing rejects before destructive work.
 * Missing key time is a native no-op. Short numeric vectors are rejected to
 * avoid the original undefined uninitialized-stack payloads.
 * diagnostic=1/2 for rejected key time/control bits; end-set reports the known
 * binding/missing-node diagnostics through resources.loaded(). Rejected keys
 * retain their native orphan ownership until this authored context is destroyed.
 * destroy() releases pending unbound resources/orphans, not completed node-owned
 * sets. Zero-node completed sets are transferred to resources.loaded().
 * All referenced services must outlive the context. Serialize calls, do not
 * throw from callbacks or reenter, and retain completed sets explicitly before
 * later node rebinding if they must stay alive. No XML parser, full scene loader,
 * callback reentrancy, scheduler, skinning or complete engine is supplied. */
FS15_CORE_API void* fs15_animation_xml_create(const FS15AnimationXmlServices*);
FS15_CORE_API int fs15_animation_xml_event(void*,unsigned,const FS15AnimationXmlAttribute*,size_t,int* diagnostic);
FS15_CORE_API int fs15_animation_xml_view(const void*,FS15AnimationXmlView*);
FS15_CORE_API int fs15_animation_xml_destroy(void*);
#ifdef __cplusplus
}
#endif
