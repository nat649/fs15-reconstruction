// Recovered CharacterSet/data lifetime, binding and default property ownership.
#include "shared_scene.h"
#include "scene_properties.h"
#include "scene_math.h"
#include "scene_trs.h"
#include <cstring>
#include <limits>
#include <initializer_list>
namespace {
constexpr uint64_t data_table=0x1404c96b0,set_table=0x1404c9678,
    set_ref_table=0x1404c9688,set_observer_table=0x1404c9698,base_ref=0x1404b6280,
    base_observer=0x1404c89f8,object_table=0x1404b88a0,property_table=0x1404c0ca8;
template<class T>T read(const void* p,size_t at){T x;std::memcpy(&x,static_cast<const uint8_t*>(p)+at,sizeof(x));return x;}
template<class T>void write(void* p,size_t at,T x){std::memcpy(static_cast<uint8_t*>(p)+at,&x,sizeof(x));}
bool allocator(const FS15SceneLifecycleServices* s){return s&&s->allocate&&s->deallocate;}
bool vector_ok(const FS15SceneVector& v,size_t stride) {
    return stride&&v.end>=v.begin&&v.capacity>=v.end&&(v.end-v.begin)%stride==0&&(v.capacity-v.begin)%stride==0&&
        (v.begin||(!v.end&&!v.capacity));
}
bool reserve(FS15SceneVector& v,size_t count,size_t stride,const FS15SceneLifecycleServices& s) {
    if(!vector_ok(v,stride)||count>std::numeric_limits<size_t>::max()/stride)return false;
    if(count<=(v.capacity-v.begin)/stride)return true;
    auto* memory=s.allocate(s.user,count*stride);if(!memory)return false;
    const size_t used=v.end-v.begin;if(used)std::memcpy(memory,reinterpret_cast<void*>(v.begin),used);
    if(v.begin)s.deallocate(s.user,reinterpret_cast<void*>(v.begin));
    v.begin=reinterpret_cast<uintptr_t>(memory);v.end=v.begin+used;v.capacity=v.begin+count*stride;return true;
}
bool append(FS15SceneVector& v,const void* value,size_t stride,const FS15SceneLifecycleServices& s) {
    if(!vector_ok(v,stride))return false;
    const size_t count=(v.end-v.begin)/stride,capacity=(v.capacity-v.begin)/stride;
    if(count==capacity) {
        const size_t grown=capacity<=std::numeric_limits<size_t>::max()-capacity/2?capacity+capacity/2:0;
        if(!reserve(v,grown>count?grown:count+1,stride,s))return false;
    }
    std::memcpy(reinterpret_cast<void*>(v.end),value,stride);v.end+=stride;return true;
}
void clear(FS15SceneVector& v,const FS15SceneLifecycleServices& s){if(v.begin)s.deallocate(s.user,reinterpret_cast<void*>(v.begin));v={0,0,0};}
void erase(FS15SceneVector& v,uintptr_t value) {
    auto* items=reinterpret_cast<uintptr_t*>(v.begin);const size_t count=(v.end-v.begin)/8;
    for(size_t i=0;i<count;++i)if(items[i]==value){std::memmove(items+i,items+i+1,(count-i-1)*8);v.end-=8;break;}
}
bool context_ok(const FS15SharedSceneContext* c){return c&&vector_ok(c->active,8)&&vector_ok(c->pending,8);}
void string_destruct(void* text,const FS15SceneLifecycleServices& s) {
    if(read<size_t>(text,24)>15)s.deallocate(s.user,reinterpret_cast<void*>(read<uintptr_t>(text,0)));
    write<size_t>(text,24,15);write<size_t>(text,16,0);write<uint8_t>(text,0,0);
}
bool clip_ok(void* clip) {
    if(!clip)return true;
    const auto channels=read<FS15SceneVector>(clip,0x30);
    if(!vector_ok(channels,8))return false;
    for(auto p=channels.begin;p<channels.end;p+=8)if(const auto channel=read<uintptr_t>(reinterpret_cast<void*>(p),0))
        if(!vector_ok(read<FS15SceneVector>(reinterpret_cast<void*>(channel),8),8))return false;
    return true;
}
void clip_destruct(void* clip,const FS15SceneLifecycleServices& s) {
    auto channels=read<FS15SceneVector>(clip,0x30);
    for(auto p=channels.begin;p<channels.end;p+=8)if(const auto channel=read<uintptr_t>(reinterpret_cast<void*>(p),0)) {
        auto keys=read<FS15SceneVector>(reinterpret_cast<void*>(channel),8);
        for(auto k=keys.begin;k<keys.end;k+=8)s.deallocate(s.user,reinterpret_cast<void*>(read<uintptr_t>(reinterpret_cast<void*>(k),0)));
        clear(keys,s);write(reinterpret_cast<void*>(channel),8,keys);s.deallocate(s.user,reinterpret_cast<void*>(channel));
    }
    clear(channels,s);write(clip,0x30,channels);string_destruct(static_cast<uint8_t*>(clip)+8,s);
}
bool data_ok(const FS15CharacterData* d) {
    if(!d||d->vtable!=data_table||!vector_ok(d->translation,12)||!vector_ok(d->rotation,16)||
        !vector_ok(d->scale,12)||!vector_ok(d->visibility_words,4)||!vector_ok(d->clips,8))return false;
    for(auto p=d->clips.begin;p<d->clips.end;p+=8)if(!clip_ok(reinterpret_cast<void*>(read<uintptr_t>(reinterpret_cast<void*>(p),0))))return false;
    return true;
}
bool refresh(FS15SceneStateRecord& node) {
    if(!(node.flags&0x80))return true;
    float scale[3],rotation[3];if(!fs15_matrix_scale(node.local,16,scale,3)||!fs15_matrix_euler(node.local,16,scale,rotation,3))return false;
    std::memcpy(node.translation,node.local+12,12);std::memcpy(node.scale,scale,12);std::memcpy(node.rotation,rotation,12);node.flags&=~uint32_t(0x80);return true;
}
bool visibility(FS15CharacterData& d,uint8_t visible,const FS15SceneLifecycleServices& s) {
    const size_t word=d.visibility_count/32,bit=d.visibility_count%32;
    if(!bit){const uint32_t zero=0;if(!append(d.visibility_words,&zero,4,s))return false;}
    auto* p=reinterpret_cast<uint32_t*>(d.visibility_words.begin)+word;
    if(visible)*p|=uint32_t(1)<<bit;else *p&=~(uint32_t(1)<<bit);
    ++d.visibility_count;return true;
}
}
extern "C" FS15_CORE_API int fs15_character_data_construct(void* memory,size_t bytes) {
    if(!memory||bytes<sizeof(FS15CharacterData))return 0;
    auto& d=*static_cast<FS15CharacterData*>(memory);
    d.vtable=data_table;d.references=d.flags=0;d.translation=d.rotation=d.scale=d.visibility_words=d.clips={0,0,0};d.visibility_count=0;return 1;
}
extern "C" FS15_CORE_API int fs15_character_data_destruct(void* memory,size_t bytes,uint32_t flags,const FS15SceneLifecycleServices* s) {
    auto* d=static_cast<FS15CharacterData*>(memory);if(!allocator(s)||bytes<sizeof(*d)||!data_ok(d))return 0;
    for(auto p=d->clips.begin;p<d->clips.end;p+=8)if(const auto clip=read<uintptr_t>(reinterpret_cast<void*>(p),0)) {
        clip_destruct(reinterpret_cast<void*>(clip),*s);s->deallocate(s->user,reinterpret_cast<void*>(clip));
    }
    clear(d->clips,*s);d->visibility_count=0;clear(d->visibility_words,*s);clear(d->scale,*s);clear(d->rotation,*s);clear(d->translation,*s);
    d->vtable=base_ref;if(flags&1)s->deallocate(s->user,d);return 1;
}
extern "C" FS15_CORE_API int fs15_character_set_construct(void* memory,size_t bytes,const char* name,size_t name_bytes,
    FS15CharacterData* data,FS15SharedSceneContext* context,const FS15SceneLifecycleServices* s) {
    if(!memory||bytes<sizeof(FS15CharacterSet)||!allocator(s)||!context_ok(context)||(data&&!data_ok(data)))return 0;
    if(!fs15_scene_construct(memory,bytes,FS15_OBJECT_BASE,name,name_bytes,0xb,1,s))return 0;
    auto& set=*static_cast<FS15CharacterSet*>(memory);set.references=0;set.pending=0;write(memory,0,set_table);
    set.reference_table=set_ref_table;set.observer_table=set_observer_table;set.transforms={0,0,0};
    write<uint32_t>(memory,0x1c,read<uint32_t>(memory,0x1c)|0x40000);
    const bool created=data==nullptr;
    if(!data) {
        data=static_cast<FS15CharacterData*>(s->allocate(s->user,sizeof(*data)));
        if(!data){fs15_scene_destruct(memory,bytes,FS15_OBJECT_BASE,0,s);return 0;}
        fs15_character_data_construct(data,sizeof(*data));
    }
    set.data=data;++data->references;
    for(size_t i=0;i<20;++i)set.state[i]=((i>=8&&i<12)||i>=16)?1.0f:0.0f;
    set.flags=0;const uintptr_t observer=reinterpret_cast<uintptr_t>(&set)+0x38;
    if(!append(context->active,&observer,8,*s)) {
        --data->references;
        if(created)fs15_character_data_destruct(data,sizeof(*data),1,s);
        fs15_scene_destruct(memory,bytes,FS15_OBJECT_BASE,0,s);return 0;
    }
    set.registered_observer=1;return 1;
}
extern "C" FS15_CORE_API int fs15_character_set_destruct(void* memory,size_t bytes,uint32_t flags,
    FS15SharedSceneContext* context,const FS15SceneLifecycleServices* s) {
    if(!memory||bytes<sizeof(FS15CharacterSet)||!allocator(s)||!context_ok(context))return 0;
    auto& set=*static_cast<FS15CharacterSet*>(memory);
    if(!data_ok(set.data)||!vector_ok(set.transforms,8))return 0;
    write(memory,0,set_table);set.reference_table=set_ref_table;set.observer_table=set_observer_table;
    if(set.registered_observer){erase(context->active,reinterpret_cast<uintptr_t>(&set)+0x38);if(set.pending)erase(context->pending,reinterpret_cast<uintptr_t>(&set)+0x38);}
    if(!fs15_shared_scene_release(set.data,sizeof(*set.data),context,s))return 0;
    clear(set.transforms,*s);set.observer_table=base_observer;set.reference_table=base_ref;write(memory,0,object_table);
    if(read<uint8_t>(memory,8))s->unregistered(s->user,read<uint32_t>(memory,0x18),memory);
    s->deallocate(s->user,reinterpret_cast<void*>(read<uintptr_t>(memory,0x10)));
    if(flags&1)s->deallocate(s->user,memory);
    return 1;
}
extern "C" FS15_CORE_API int fs15_shared_scene_release(void* resource,size_t bytes,
    FS15SharedSceneContext* context,const FS15SceneLifecycleServices* s) {
    if(!resource||bytes<0x10||!allocator(s))return 0;
    const auto table=read<uint64_t>(resource,0);const uint32_t refs=read<uint32_t>(resource,8);
    if(table!=data_table&&table!=set_ref_table)return 0;
    if(refs>=2){write(resource,8,refs-1);return 1;}
    if(table==data_table)return fs15_character_data_destruct(resource,bytes,1,s);
    return fs15_character_set_destruct(static_cast<uint8_t*>(resource)-0x28,sizeof(FS15CharacterSet),1,context,s);
}
extern "C" FS15_CORE_API int fs15_character_set_bind(FS15CharacterSet* set,FS15SceneStateRecord* const* nodes,size_t count,
    FS15SharedSceneContext* context,const FS15SceneLifecycleServices* s,int* diagnostic) {
    if(!set||(!nodes&&count)||count>0xffffffff||!allocator(s)||!context_ok(context)||!diagnostic||!data_ok(set->data)||!vector_ok(set->transforms,8))return 0;
    *diagnostic=0;const bool defaults=set->data->translation.end==set->data->translation.begin;
    for(size_t i=0;i<count;++i) {
        auto* node=nodes[i];if(!node)return 0;
        if(!(node->type_flags&0x40)){*diagnostic=1;return 1;}
        const uintptr_t pointer=reinterpret_cast<uintptr_t>(node);if(!append(set->transforms,&pointer,8,*s))return 0;
        if(defaults) {
            if(!refresh(*node)||!append(set->data->translation,node->translation,12,*s))return 0;
            const float zero[3]={0,0,0},one[3]={1,1,1};float matrix[16],quaternion[4];
            if(!fs15_compose_trs(node->rotation,one,zero,matrix,16)||!fs15_matrix_to_quaternion(matrix,16,quaternion,4)||
                !append(set->data->rotation,quaternion,16,*s)||!append(set->data->scale,node->scale,12,*s)||!visibility(*set->data,uint8_t(node->flags&1),*s))return 0;
        }
        if(node->shared_resource) {
            if(s->release_shared){if(!s->release_shared(s->user,reinterpret_cast<void*>(node->shared_resource)))return 0;}
            else if(!fs15_shared_scene_release(reinterpret_cast<void*>(node->shared_resource),0x10,context,s))return 0;
        }
        ++set->references;node->shared_resource=reinterpret_cast<uintptr_t>(set)+0x28;
    }
    const size_t transforms=(set->transforms.end-set->transforms.begin)/8;
    for(auto p=set->data->clips.begin;p<set->data->clips.end;p+=8) {
        const auto clip=read<uintptr_t>(reinterpret_cast<void*>(p),0);if(!clip)return 0;
        const auto channels=read<FS15SceneVector>(reinterpret_cast<void*>(clip),0x30);
        if((channels.end-channels.begin)/8<transforms){*diagnostic=2;return 1;}
    }
    set->flags=0x101;return 1;
}
extern "C" FS15_CORE_API int fs15_scene_property_clone(const FS15ScenePropertyView* source,size_t bytes,
    const FS15SceneLifecycleServices* s,FS15ScenePropertyView** out) {
    if(!source||bytes<sizeof(*source)||source->unknown00!=property_table||!allocator(s)||!out)return 0;
    auto* result=static_cast<FS15ScenePropertyView*>(s->allocate(s->user,sizeof(FS15ScenePropertyView)));if(!result)return 0;
    result->unknown00=property_table;result->flags=source->flags;*out=result;return 1;
}
extern "C" FS15_CORE_API int fs15_scene_property_replace(void* record,size_t bytes,FS15ScenePropertyView* replacement,const FS15SceneLifecycleServices* s) {
    if(!record||bytes<0x120||!allocator(s)||(replacement&&replacement->unknown00!=property_table&&!fs15_scene_property_detached(replacement)))return 0;
    const auto old=read<uintptr_t>(record,0x118);
    if(old==reinterpret_cast<uintptr_t>(replacement)&&old)return 0;
    if(old){
        auto* property=reinterpret_cast<void*>(old);
        if(read<uint64_t>(property,0)==property_table)s->deallocate(s->user,property);
        else if(!fs15_scene_property_destruct_detached(property,1,s))return 0;
    }
    write(record,0x118,reinterpret_cast<uintptr_t>(replacement));return 1;
}
