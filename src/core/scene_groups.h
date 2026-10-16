#pragma once
#include "scene_xml.h"
#include "scene_lod.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Recovered 4-byte float-vector growth/parser paths used by lodDistance.
 * scan_length is the original explicit byte count; it must not exceed the
 * bounded string's first NUL. Parsing appends, including a final scalar for
 * scan_length=0. Repeated spaces and trailing empty scalars retain native
 * behavior. Counts/work/finite guards are authored. Prefix appends remain on
 * failure. Vector capacities/pointees must be valid owned storage. */
FS15_CORE_API int fs15_scene_float_reserve(FS15SceneVector*,size_t,const FS15SceneLifecycleServices*);
FS15_CORE_API int fs15_scene_float_grow(FS15SceneVector*,size_t,const FS15SceneLifecycleServices*);
FS15_CORE_API int fs15_scene_float_vector(const char*,size_t,uint32_t,FS15SceneVector*,const FS15SceneLifecycleServices*);
/* Decoded XML callback transport, not the original complete loader class.
 * constructed registers the completed allocation with the caller's supported
 * owned view before common attribute services run. link invokes known recovered
 * hierarchy dispatch. Both use common.user, cannot throw/reenter, and retain
 * ownership through failure. kind is TRS=1 or LOD=2; bytes is 0x170 or 0x188. */
typedef struct FS15SceneGroupServices {
    FS15SceneXmlServices common;
    int (*constructed)(void*,FS15SceneStateRecord*,unsigned,size_t);
    int (*link)(void*,FS15SceneStateRecord*,FS15SceneStateRecord*,uint32_t);
} FS15SceneGroupServices;
typedef struct FS15SceneGroupResult {FS15SceneStateRecord* node;size_t bytes;unsigned kind;} FS15SceneGroupResult;
/* Recover 0x14018c1a0. Missing/null first name is a successful no-op. Both name
 * and lodDistance searches start at attribute zero, retaining the first match.
 * Empty nonnull lodDistance still creates a LOD node with defaults.
 * Common attributes run before ordered UINT32_MAX attachment. current changes
 * only after successful attachment. After a constructor succeeds, result owns
 * the node even if a later callback fails; caller must release that prefix via
 * recovered known lifetime services. A failed constructor is already cleaned.
 * No rollback of common source-ID registration or partial links is attempted. */
FS15_CORE_API int fs15_scene_group_begin(FS15SceneStateRecord** current,const FS15SceneXmlAttribute*,size_t,const FS15SceneGroupServices*,FS15SceneGroupResult*);
/* Recover 0x14018c370 on non-shape nodes. Pop first, then report unsupported
 * shape flag 0x800 without reading derived +0x190/render-scheduling data.
 * That original shape-finalization branch remains open. Null current rejects. */
FS15_CORE_API int fs15_scene_group_end(FS15SceneStateRecord** current);
#ifdef __cplusplus
}
#endif
