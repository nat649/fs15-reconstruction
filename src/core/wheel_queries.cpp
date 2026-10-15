#include "api.h"
#include "wheel_record.h"
#include <cstring>

extern "C" FS15_CORE_API const void* fs15_resolve_wheel_record(
    const void* manager_ptr,const void* object_ptr,std::int32_t id,const void** found_set) noexcept {
    const auto* manager=static_cast<const fs15::PhysicsManagerView*>(manager_ptr);
    const auto* object=static_cast<const fs15::SceneObjectView*>(object_ptr);
    if(!manager||!object||!object->physics||(object->physics->flags&8)==0||!manager->wheels)return nullptr;
    const auto key=object->physics->actor_key;
    const auto* sentinel=manager->wheels;
    const auto* node=sentinel->parent;
    const auto* candidate=sentinel;
    // Authored cycle bound for malformed fixtures; valid registry pointers are caller-owned.
    std::size_t visited=0;
    while(node && node->is_nil==0) {
        if(++visited>4096)return nullptr;
        if(node->actor_key<key)node=node->right;
        else {candidate=node;node=node->left;}
    }
    if(candidate==sentinel||key<candidate->actor_key||!candidate->value)return nullptr;
    const auto* set=candidate->value;
    if(found_set)*found_set=set;
    if(set->count<=0||set->count>1048576||!set->records)return nullptr;
    for(std::int32_t i=0;i<set->count;++i) {
        if(set->records[i].shape_id==id) { return &set->records[i]; }
    }
    return nullptr;
}

extern "C" FS15_CORE_API int fs15_query_wheel_record(unsigned query,const void* record_ptr,
    std::size_t bytes,int node_present,void* frame_ptr,std::size_t frame_bytes) noexcept {
    if(query>FS15_WHEEL_SLIP||!frame_ptr||frame_bytes<0x130||(record_ptr&&bytes<sizeof(fs15::WheelRecord)))return 0;
    const auto* record=static_cast<const fs15::WheelRecord*>(record_ptr);
    auto* frame=static_cast<std::uint8_t*>(frame_ptr);
    auto type=[&](unsigned slot,std::uint16_t value){std::memcpy(frame+0xf8+slot*16,&value,2);};
    auto value=[&](unsigned slot,const void* source,std::uint16_t tag){std::memcpy(frame+0xf0+slot*16,source,4);type(slot,tag);};
    if(query==FS15_WHEEL_CONTACT_OBJECT) {
        const std::uint32_t id=node_present&&record?record->contact_object:0;
        value(0,&id,1);return 1;
    }
    if(!node_present)return 1; // Native adapters leave the frame untouched.
    const float zero=0;
    if(query==FS15_WHEEL_AXLE_SPEED){value(0,record?&record->axle_speed:&zero,3);return 1;}
    if(query==FS15_WHEEL_SLIP){for(unsigned i=0;i<2;++i) {value(i,record?&record->slip[i]:&zero,3);}return 1;}
    if(query==FS15_WHEEL_POSITION) {
        // The original exposes uninitialized stack values when lookup fails.
        // The authored API rejects that undefined case, without writing output.
        if(!record)return 0;
        for(unsigned i=0;i<3;++i) {value(i,&record->position[i],3);}
        value(3,&record->drive_angle,3);return 1;
    }
    const unsigned count=query==FS15_WHEEL_CONTACT_POINT?4:query==FS15_WHEEL_CONTACT_NORMAL?3:1;
    if(!record||!record->contact_active){for(unsigned i=0;i<count;++i) {type(i,0);}return 1;}
    if(query==FS15_WHEEL_CONTACT_FORCE)value(0,&record->contact_force,3);
    else if(query==FS15_WHEEL_CONTACT_NORMAL){for(unsigned i=0;i<3;++i) {value(i,&record->contact_normal[i],3);}}
    else {for(unsigned i=0;i<3;++i) {value(i,&record->contact_point[i],3);}value(3,&record->contact_point_aux,3);}
    return 1;
}
