// Recovered native property records; backend dispatch is an explicit host seam.
#include "scene_properties.h"
#include <cmath>
#include <cstring>
namespace {
constexpr uint64_t body_table=0x1404c7a90,body_observer=0x1404c7ac0,joint_table=0x1404c8158,
    joint_observer=0x1404c8188,base_table=0x1404c0ca8,base_observer=0x1404c1cf0;
bool queued(FS15RigidBodyProperty& p,uint32_t old,const FS15ScenePropertyUpdates* updates) {
    if(old||!p.manager||!p.actor)return true;
    return updates&&updates->queue&&updates->queue(updates->user,p.manager,&p);
}
}
extern "C" int fs15_scene_property_construct(unsigned kind,void* memory,size_t bytes,FS15SceneStateRecord* owner) {
    if(!memory||!owner||kind>FS15_PROPERTY_JOINT||bytes<(kind==0?sizeof(FS15RigidBodyProperty):sizeof(FS15JointProperty)))return 0;
    if(kind==0) {
        auto& p=*static_cast<FS15RigidBodyProperty*>(memory);p.flags=0x80c;p.vtable=body_table;p.observer_table=body_observer;
        p.manager=p.actor=p.attachment=p.attachment_count=p.extra_reference=0;p.owner=owner;p.changes=0;
        p.collision_mask=0xff;p.restitution=0;p.static_friction=p.dynamic_friction=0.5f;p.linear_damping=0;p.angular_damping=0.01f;
        p.density=p.unknown6c=1;p.unknown70=p.unknown74=p.unknown78=0;p.unknown7c=0;p.solver_iterations=4;
    }else {
        auto& p=*static_cast<FS15JointProperty*>(memory);p.flags=0x14;p.vtable=joint_table;p.observer_table=joint_observer;p.owner=owner;
        p.manager=p.constraint=0;p.unknown30=0;p.projection_distance=p.projection_angle=p.drive_damping=0.01f;
        p.drive_force=p.break_force=p.break_torque=100000;p.drive_spring=1;
        std::memset(p.rotation_limited,0,3);std::memset(p.translation_limited,0,3);
        std::memset(p.rotation_min,0,24);std::memset(p.translation_min,0,24);p.unknown80=0;
    }
    return 1;
}
extern "C" int fs15_scene_property_detached(const void* memory) {
    if(!memory)return 0;
    const auto table=*static_cast<const uint64_t*>(memory);const FS15SceneStateRecord* owner=nullptr;
    if(table==body_table) {
        const auto& p=*static_cast<const FS15RigidBodyProperty*>(memory);
        if(p.manager||p.actor||p.attachment||p.attachment_count||p.extra_reference||(p.flags&3u))return 0;
        owner=p.owner;
    }else if(table==joint_table) {
        const auto& p=*static_cast<const FS15JointProperty*>(memory);if(p.manager||p.constraint||(p.flags&3u))return 0;owner=p.owner;
    }else return 0;
    if(!owner||owner->observers_end<owner->observers_begin||(owner->observers_end-owner->observers_begin)%8||
        (owner->observers_end-owner->observers_begin)/8>65535||(owner->observers_end!=owner->observers_begin&&!owner->observers_begin))return 0;
    const uintptr_t observer=reinterpret_cast<uintptr_t>(memory)+0x18;
    for(uintptr_t p=owner->observers_begin;p<owner->observers_end;p+=8)if(*reinterpret_cast<const uintptr_t*>(p)==observer)return 0;
    return 1;
}
extern "C" int fs15_scene_property_destruct_detached(void* memory,uint32_t flags,const FS15SceneLifecycleServices* services) {
    if(!fs15_scene_property_detached(memory)||((flags&1u)&&(!services||!services->deallocate)))return 0;
    // All backend/observer-release branches have been excluded by the detached
    // precondition. The original destructors end with these two token writes.
    auto* bytes=static_cast<uint8_t*>(memory);std::memcpy(bytes,&base_table,8);std::memcpy(bytes+0x18,&base_observer,8);
    if(flags&1u)services->deallocate(services->user,memory);
    return 1;
}
extern "C" int fs15_scene_property_flags(FS15RigidBodyProperty* p,uint32_t flags,int enable,const FS15ScenePropertyUpdates* updates) {
    if(!p||p->vtable!=body_table)return 0;
    const uint32_t old=p->changes;p->flags=enable?p->flags|flags:p->flags&~flags;p->changes=old|flags;return queued(*p,old,updates);
}
extern "C" int fs15_scene_property_density(FS15RigidBodyProperty* p,float density,const FS15ScenePropertyUpdates* updates) {
    if(!p||p->vtable!=body_table||!std::isfinite(density))return 0;
    if(density<0.0001f)density=0.0001f;
    p->flags&=~uint32_t(0x10000);p->density=density;
    if(!p->actor)return 1;
    const uint32_t old=p->changes;p->changes=old|0x400000u;return queued(*p,old,updates);
}
