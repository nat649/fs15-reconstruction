#pragma once
#include "scene_lifecycle.h"
#ifdef __cplusplus
extern "C" {
#endif
enum FS15ScenePropertyKind {FS15_PROPERTY_RIGID_BODY=0,FS15_PROPERTY_JOINT=1};
typedef struct FS15RigidBodyProperty {
    uint64_t vtable;uint32_t flags,unknown0c;uintptr_t manager;uint64_t observer_table;
    uint32_t changes,unknown24;FS15SceneStateRecord* owner;uintptr_t actor,attachment;
    uint64_t attachment_count;uintptr_t extra_reference;
    uint32_t collision_mask;float restitution,static_friction,dynamic_friction,linear_damping,angular_damping,density;
    float unknown6c,unknown70,unknown74,unknown78;uint32_t unknown7c,solver_iterations,unknown84;
} FS15RigidBodyProperty;
typedef struct FS15JointProperty {
    uint64_t vtable;uint32_t flags,unknown0c;uintptr_t manager;uint64_t observer_table;
    FS15SceneStateRecord* owner;uintptr_t constraint;uint8_t unknown30,unknown31[3];
    float projection_distance,projection_angle,drive_force,drive_spring,drive_damping;
    uint8_t rotation_limited[3],unknown4b;float rotation_min[3],rotation_max[3];
    uint8_t translation_limited[3],unknown67;float translation_min[3],translation_max[3];
    uint8_t unknown80,unknown81[3];float break_force,break_torque;uint32_t unknown8c;
} FS15JointProperty;
/* Recovered metadata construction and detached destructor/deleting paths.
 * Native backend manager/actor/constraint/resource ownership is not supplied.
 * Detached means no live backend fields/low state bits and no embedded observer
 * registered on the valid owner node. Unknown fields stay opaque. Caller-owned
 * pointees/vectors must be valid. Active backend/observer teardown rejects before
 * mutation; flags&1 controls deleting release, as in the original wrappers. */
FS15_CORE_API int fs15_scene_property_construct(unsigned,void*,size_t,FS15SceneStateRecord*);
FS15_CORE_API int fs15_scene_property_detached(const void*);
FS15_CORE_API int fs15_scene_property_destruct_detached(void*,uint32_t,const FS15SceneLifecycleServices*);
/* Original flag/density changes. queue(user,manager,property) represents native
 * update dispatch; required only when a previously clean live property is queued.
 * Density retains the native minimum 0.0001 and actor-dependent dirty behavior.
 * Side effects after a callback begins are not rolled back on callback failure. */
typedef struct FS15ScenePropertyUpdates {void* user;int (*queue)(void*,uintptr_t,FS15RigidBodyProperty*);} FS15ScenePropertyUpdates;
FS15_CORE_API int fs15_scene_property_flags(FS15RigidBodyProperty*,uint32_t,int,const FS15ScenePropertyUpdates*);
FS15_CORE_API int fs15_scene_property_density(FS15RigidBodyProperty*,float,const FS15ScenePropertyUpdates*);
/* Native metadata clone dispatch: base, rigid body and joint tables only.
 * A new detached property belongs to owner. Backend handles/observer bindings
 * are initialized afresh; source flags and documented metadata follow native
 * copy/default/clamp rules. This does not clone a live physics actor. */
FS15_CORE_API int fs15_scene_property_clone_owned(const void*,FS15SceneStateRecord*,const FS15SceneLifecycleServices*,void**);
#ifdef __cplusplus
}
static_assert(sizeof(FS15RigidBodyProperty)==0x88);
static_assert(sizeof(FS15JointProperty)==0x90);
static_assert(offsetof(FS15RigidBodyProperty,owner)==0x28);
static_assert(offsetof(FS15JointProperty,rotation_min)==0x4c);
static_assert(offsetof(FS15JointProperty,break_torque)==0x88);
#endif
