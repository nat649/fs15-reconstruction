// Recovered TransformGroup/LOD callbacks; decoded-attribute/owned-view glue authored.
#include "scene_groups.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace {
constexpr size_t limit=1048576;
bool allocator(const FS15SceneLifecycleServices* s){return s&&s->allocate&&s->deallocate;}
size_t checked(const FS15SceneVector& v) {
    if(v.end<v.begin||v.capacity<v.end||(v.end-v.begin)%4||(v.capacity-v.begin)%4||(!v.begin&&(v.end||v.capacity))||(v.capacity-v.begin)/4>limit)
        throw std::runtime_error("Invalid scene float vector");
    return (v.capacity-v.begin)/4;
}
size_t length(const char* text,size_t bytes) {
    if(!text||!bytes||bytes>limit+1)throw std::runtime_error("Invalid scene group string");
    const auto* end=static_cast<const char*>(std::memchr(text,0,bytes));if(!end)throw std::runtime_error("Unterminated scene group string");return static_cast<size_t>(end-text);
}
bool append(FS15SceneVector& v,const char* text,size_t bytes,const FS15SceneLifecycleServices* s) {
    float value=0;size_t consumed=0;
    if(!fs15_parse_scene_float(text,bytes,&value,&consumed)||!std::isfinite(value)||!fs15_scene_float_grow(&v,1,s))return false;
    std::memcpy(reinterpret_cast<void*>(v.end),&value,4);v.end+=4;return true;
}
}
extern "C" int fs15_scene_float_reserve(FS15SceneVector* v,size_t capacity,const FS15SceneLifecycleServices* s) {
    if(!v||!allocator(s)||capacity>limit)return 0;
    try {
        const size_t old=checked(*v),used=v->end-v->begin;if(capacity<=old)return 1;
        void* memory=s->allocate(s->user,capacity*4);if(!memory)return 0;
        if(used)std::memcpy(memory,reinterpret_cast<void*>(v->begin),used);
        if(v->begin)s->deallocate(s->user,reinterpret_cast<void*>(v->begin));
        v->begin=reinterpret_cast<uintptr_t>(memory);v->end=v->begin+used;v->capacity=v->begin+capacity*4;return 1;
    }catch(...){return 0;}
}
extern "C" int fs15_scene_float_grow(FS15SceneVector* v,size_t extra,const FS15SceneLifecycleServices* s) {
    if(!v||!allocator(s))return 0;
    try {
        const size_t capacity=checked(*v),count=(v->end-v->begin)/4;if(extra>limit-count)return 0;
        const size_t requested=count+extra;if(requested<=capacity)return 1;
        return fs15_scene_float_reserve(v,std::max(requested,capacity+capacity/2),s);
    }catch(...){return 0;}
}
extern "C" int fs15_scene_float_vector(const char* text,size_t bytes,uint32_t scan_length,FS15SceneVector* vector,const FS15SceneLifecycleServices* memory) {
    if(!vector||!allocator(memory))return 0;
    try {
        const size_t text_length=length(text,bytes);checked(*vector);if(scan_length>text_length)return 0;
        size_t start=0;
        for(size_t i=0;i<scan_length;++i)if(text[i]==' '){if(!append(*vector,text+start,bytes-start,memory))return 0;start=i+1;}
        return append(*vector,text+start,bytes-start,memory);
    }catch(...){return 0;}
}
extern "C" int fs15_scene_group_begin(FS15SceneStateRecord** current,const FS15SceneXmlAttribute* attrs,size_t count,const FS15SceneGroupServices* services,FS15SceneGroupResult* result) {
    if(!current||!*current||!services||!result||!allocator(services->common.memory)||!services->constructed||!services->link)return 0;
    *result={nullptr,0,0};const auto* memory=services->common.memory;FS15SceneVector distances{};
    try {
        const char* name=nullptr;if(!fs15_scene_xml_find(attrs,count,"name",5,&name))return 0;if(!name)return 1;
        const FS15SceneXmlAttribute* lod=nullptr;
        // Instruction evidence retains the first name pointer in RDX while the
        // name search advances RCX, then restarts the LOD search from RDX.
        for(size_t i=0;i<count;++i)if(std::strcmp(attrs[i].name,"lodDistance")==0){lod=attrs+i;break;}
        const bool is_lod=lod&&lod->value;
        if(is_lod) {
            const size_t n=length(lod->value,lod->value_bytes);
            if(n&&!fs15_scene_float_vector(lod->value,lod->value_bytes,static_cast<uint32_t>(n),&distances,memory)) {
                if(distances.begin)memory->deallocate(memory->user,reinterpret_cast<void*>(distances.begin));
                return 0;
            }
        }
        const size_t bytes=is_lod?sizeof(FS15SceneLodRecord):sizeof(FS15SceneStateRecord);
        auto* node=static_cast<FS15SceneStateRecord*>(memory->allocate(memory->user,bytes));
        if(!node){if(distances.begin)memory->deallocate(memory->user,reinterpret_cast<void*>(distances.begin));return 0;}
        const bool constructed=is_lod?fs15_scene_lod_construct(reinterpret_cast<FS15SceneLodRecord*>(node),bytes,name,std::strlen(name)+1,reinterpret_cast<const float*>(distances.begin),static_cast<uint32_t>((distances.end-distances.begin)/4),memory)!=0:
            fs15_scene_construct(node,bytes,FS15_OBJECT_TRS,name,std::strlen(name)+1,0,1,memory)!=0;
        if(distances.begin)memory->deallocate(memory->user,reinterpret_cast<void*>(distances.begin));
        distances={0,0,0};
        if(!constructed){memory->deallocate(memory->user,node);return 0;}
        *result={node,bytes,is_lod?2u:1u};
        if(!services->constructed(services->common.user,node,result->kind,bytes)||
            !fs15_scene_xml_apply(node,attrs,count,1,1,1,"",1,&services->common)||
            !services->link(services->common.user,*current,node,UINT32_MAX))return 0;
        *current=node;return 1;
    }catch(...){if(distances.begin)memory->deallocate(memory->user,reinterpret_cast<void*>(distances.begin));return 0;}
}
extern "C" int fs15_scene_group_end(FS15SceneStateRecord** current) {
    if(!current||!*current)return 0;
    auto* closed=*current;*current=reinterpret_cast<FS15SceneStateRecord*>(closed->parent);return !(closed->type_flags&0x800u);
}
