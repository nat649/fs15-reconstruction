// Recovered object initialization/lifetime flow with authored platform services.
#include "scene_lifecycle.h"
#include "scene_attributes.h"
#include "scene_properties.h"
#include <cstring>
#include <limits>
#include <initializer_list>

namespace {
constexpr uint64_t object_table=0x1404b88a0,node_table=0x1404c10c8,
    hierarchy_table=0x1404c7dd0,matrix_table=0x1404c0cd8,trs_table=0x1404c5d98,
    property_table=0x1404c0ca8;
template<class T>T read(const void* p,size_t at){T result;std::memcpy(&result,static_cast<const uint8_t*>(p)+at,sizeof(T));return result;}
template<class T>void write(void* p,size_t at,T value){std::memcpy(static_cast<uint8_t*>(p)+at,&value,sizeof(T));}
size_t record_size(unsigned kind){return kind==0?0x28:kind==1?0x98:kind==2?0x148:0x170;}
bool services_ok(const FS15SceneLifecycleServices* s){return s&&s->allocate&&s->deallocate&&s->next_id&&s->registered&&s->unregistered;}
size_t name_length(const char* name,size_t bytes) {
    if(!name||!bytes)return 0;
    const auto* end=static_cast<const char*>(std::memchr(name,0,bytes));
    if(!end||static_cast<size_t>(end-name)>=std::numeric_limits<uint32_t>::max())return 0;
    return static_cast<size_t>(end-name)+1;
}
}
extern "C" FS15_CORE_API int fs15_scene_construct(void* record,size_t bytes,unsigned kind,
    const char* name,size_t name_bytes,uint32_t tag,uint8_t registration,const FS15SceneLifecycleServices* s) {
    const size_t length=name_length(name,name_bytes);
    if(!record||kind>FS15_OBJECT_TRS_TYPED||bytes<record_size(kind)||!length||!services_ok(s))return 0;
    void* copied_name=nullptr;void* attributes=nullptr;void* props=nullptr;
    const uint32_t id=s->next_id(s->user);
    if(length>1) {
        copied_name=s->allocate(s->user,length);if(!copied_name)return 0;
        std::memcpy(copied_name,name,length);
    }
    write(record,0,object_table);write(record,0x18,id);
    write(record,0x10,reinterpret_cast<uintptr_t>(copied_name));write<uint32_t>(record,0x1c,0);
    write<uint8_t>(record,8,kind?1:registration);
    const bool registered=kind||registration;
    if(registered)s->registered(s->user,id,record);
    write<uint32_t>(record,0x20,kind==FS15_OBJECT_TRS?0x1d:tag);
    if(kind) {
        write(record,0,node_table);write<uint32_t>(record,0x1c,2);
        for(size_t offset:{size_t(0x28),size_t(0x30),size_t(0x38),size_t(0x40),size_t(0x58),size_t(0x60),size_t(0x68),size_t(0x88)})write<uintptr_t>(record,offset,0);
        attributes=s->allocate(s->user,0x50);
        if(!attributes){if(registered)s->unregistered(s->user,id,record);s->deallocate(s->user,copied_name);return 0;}
        const auto pointer=reinterpret_cast<uintptr_t>(attributes);
        write(attributes,0,pointer);write(attributes,8,pointer);write(attributes,16,pointer);
        write<uint8_t>(attributes,0x48,1);write<uint8_t>(attributes,0x49,1);
        write(record,0x80,reinterpret_cast<uintptr_t>(attributes));write<uint32_t>(record,0x1c,0x12);write<uint32_t>(record,0x50,1);
    }
    if(kind>=FS15_OBJECT_MATRIX) {
        write(record,0,matrix_table);write<uint32_t>(record,0x1c,0x32);
        const float identity[]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
        std::memcpy(static_cast<uint8_t*>(record)+0x98,identity,64);
        write<uint32_t>(record,0x50,0x41);
        props=s->allocate(s->user,0x10);
        if(!props){s->deallocate(s->user,attributes);if(registered)s->unregistered(s->user,id,record);s->deallocate(s->user,copied_name);return 0;}
        write(props,0,property_table);write<uint32_t>(props,8,0);
        write(record,0x118,reinterpret_cast<uintptr_t>(props));
        write<uintptr_t>(record,0x120,0);
        for(size_t offset:{size_t(0x128),size_t(0x138)}) {
            write<uint64_t>(record,offset,0x49742400);write<uint32_t>(record,offset+8,0xffff);write<uint32_t>(record,offset+12,0xffffffff);
        }
    }
    if(kind>=FS15_OBJECT_TRS) {
        write(record,0,trs_table);write<uint32_t>(record,0x1c,0x72);
        const float cache[]={0,0,0,0,0,0,1,1,1};std::memcpy(static_cast<uint8_t*>(record)+0x148,cache,36);
    }
    return 1;
}
extern "C" FS15_CORE_API int fs15_scene_rename(void* record,size_t bytes,const char* name,
    size_t name_bytes,const FS15SceneLifecycleServices* s) {
    const size_t length=name_length(name,name_bytes);
    if(!record||bytes<0x28||!length||!services_ok(s))return 0;
    const uintptr_t previous=read<uintptr_t>(record,0x10),input=reinterpret_cast<uintptr_t>(name);
    // The native free-before-copy cannot safely rename from its own storage.
    if(previous&&input>=previous&&input-previous<=std::strlen(reinterpret_cast<const char*>(previous)))return 0;
    s->deallocate(s->user,reinterpret_cast<void*>(previous));
    void* copied=s->allocate(s->user,length);if(!copied){write<uintptr_t>(record,0x10,0);return 0;}
    std::memcpy(copied,name,length);
    write(record,0x10,reinterpret_cast<uintptr_t>(copied));return 1;
}
extern "C" FS15_CORE_API int fs15_scene_destruct(void* record,size_t bytes,unsigned kind,
    uint32_t deleting_flags,const FS15SceneLifecycleServices* s) {
    if(!record||kind>FS15_OBJECT_TRS_TYPED||bytes<record_size(kind)||!services_ok(s))return 0;
    if(kind) {
        if(!fs15_scene_attributes_validate(record,bytes))return 0;
        if(kind>=FS15_OBJECT_MATRIX&&read<uintptr_t>(record,0x120)&&!s->release_shared)return 0;
        if(kind>=FS15_OBJECT_MATRIX) {
            const auto props=read<uintptr_t>(record,0x118);
            if(props&&read<uint64_t>(reinterpret_cast<void*>(props),0)!=property_table&&!fs15_scene_property_detached(reinterpret_cast<void*>(props)))return 0;
        }
        if(kind>=FS15_OBJECT_TRS) {
            const auto props=read<uintptr_t>(record,0x118);
            if(!props||(read<uint32_t>(reinterpret_cast<void*>(props),8)&0x1008)==0x1008)return 0;
        }
        const uintptr_t begin=read<uintptr_t>(record,0x30),end=read<uintptr_t>(record,0x38);
        if(end<begin||(end-begin)%sizeof(uintptr_t)||(end-begin)/sizeof(uintptr_t)>0x7fffffff||
            (end!=begin&&(!begin||!s->destroy_child))||(read<uintptr_t>(record,0x28)&&!s->unlink))return 0;
    }
    if(kind>=FS15_OBJECT_MATRIX) {
        write(record,0,matrix_table);
        if(const auto props=read<uintptr_t>(record,0x118)) {
            auto* property=reinterpret_cast<void*>(props);
            if(read<uint64_t>(property,0)==property_table)s->deallocate(s->user,property);
            else if(!fs15_scene_property_destruct_detached(property,1,s))return 0;
        }
        if(const auto shared=read<uintptr_t>(record,0x120))if(!s->release_shared(s->user,reinterpret_cast<void*>(shared)))return 0;
    }
    if(kind) {
        write(record,0,node_table);if(!fs15_scene_attributes_clear(record,bytes,1,s))return 0;
        if(const auto observers=read<uintptr_t>(record,0x58))s->deallocate(s->user,reinterpret_cast<void*>(observers));
        write<uintptr_t>(record,0x58,0);write<uintptr_t>(record,0x60,0);write<uintptr_t>(record,0x68,0);
        write(record,0,hierarchy_table);
        if(const auto parent=read<uintptr_t>(record,0x28)) {
            const auto* source=reinterpret_cast<void*>(parent);
            const auto begin=read<uintptr_t>(source,0x30),end=read<uintptr_t>(source,0x38);
            if(end<begin||(end-begin)%sizeof(uintptr_t))return 0;
            for(uint32_t slot=0;slot<(end-begin)/sizeof(uintptr_t);++slot) {
                if(read<uintptr_t>(reinterpret_cast<void*>(begin),slot*sizeof(uintptr_t))==reinterpret_cast<uintptr_t>(record)) {
                    if(!s->unlink(s->user,reinterpret_cast<void*>(parent),slot,1))return 0;
                    break;
                }
            }
        }
        while(read<uintptr_t>(record,0x38)!=read<uintptr_t>(record,0x30)) {
            const auto previous=read<uintptr_t>(record,0x38);
            const auto first=read<uintptr_t>(reinterpret_cast<void*>(read<uintptr_t>(record,0x30)),0);
            if(!first||!s->destroy_child(s->user,reinterpret_cast<void*>(first))||read<uintptr_t>(record,0x38)>=previous)return 0;
        }
        if(const auto children=read<uintptr_t>(record,0x30))s->deallocate(s->user,reinterpret_cast<void*>(children));
        write<uintptr_t>(record,0x30,0);write<uintptr_t>(record,0x38,0);write<uintptr_t>(record,0x40,0);
    }
    write(record,0,object_table);
    if(read<uint8_t>(record,8))s->unregistered(s->user,read<uint32_t>(record,0x18),record);
    s->deallocate(s->user,reinterpret_cast<void*>(read<uintptr_t>(record,0x10)));
    if(deleting_flags&1)s->deallocate(s->user,record);
    return 1;
}
