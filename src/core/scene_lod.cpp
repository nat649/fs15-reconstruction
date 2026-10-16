// Recovered LOD metadata, registration, distance cache and known lifecycle.
#include "scene_lod.h"
#include "scene_owned.h"
#include <algorithm>
#include <cmath>
#include <cstring>
namespace {
constexpr uint64_t lod_table=0x1404c1c48;
bool manager(const FS15SceneLodManager* m){return m&&m->count<=m->capacity&&m->capacity<=64512&&(!m->capacity||m->entries);}
bool services(const FS15SceneLodServices* s){return s&&s->memory&&s->memory->allocate&&s->memory->deallocate;}
bool node(const FS15SceneLodRecord* n){return n&&n->node.vtable==lod_table;}
bool children(const FS15SceneStateRecord& n) {
    return n.children_end>=n.children_begin&&(n.children_end-n.children_begin)%8==0&&
        (n.children_end-n.children_begin)/8<=1048576&&(n.children_begin||n.children_end==n.children_begin);
}
bool fields(const float* position,const float* distances,uint32_t count) {
    if(!position)return false;
    for(unsigned i=0;i<3;++i)if(!std::isfinite(position[i]))return false;
    const uint32_t levels=std::min(count,uint32_t(4));if(levels&&!distances)return false;
    for(uint32_t i=0;i<levels;++i)if(!std::isfinite(distances[i])||!std::isfinite(distances[i]*distances[i]))return false;
    return true;
}
void entry(FS15SceneLodEntry& e,const float* position,const float* distances,uint32_t count,uint16_t stamp) {
    std::memcpy(e.position,position,12);e.levels=static_cast<uint8_t>(std::min(count,uint32_t(4)));
    for(unsigned i=0;i<e.levels;++i)e.distance_squared[i]=distances[i]*distances[i];
    e.selected=0;e.stamp=static_cast<uint16_t>(stamp-1u);
}
}
extern "C" int fs15_scene_lod_construct(FS15SceneLodRecord* n,size_t bytes,const char* name,size_t name_bytes,const float* distances,uint32_t count,const FS15SceneLifecycleServices* memory) {
    if(!n||bytes<sizeof(*n)||(count>=2&&!distances))return 0;
    if(count>=2)for(uint32_t i=0;i<std::min(count,uint32_t(4));++i)if(!std::isfinite(distances[i]))return 0;
    if(!fs15_scene_construct(n,bytes,FS15_OBJECT_TRS,name,name_bytes,0,1,memory))return 0;
    n->node.vtable=lod_table;n->node.type_flags|=0x80;
    if(count<2){n->distances[0]=0;n->distances[1]=n->distances[2]=n->distances[3]=100;}
    else for(uint32_t i=0;i<4;++i)n->distances[i]=distances[std::min(i,std::min(count,uint32_t(4))-1)];
    n->index=0x3fffffff;n->node.flags|=8;return 1;
}
extern "C" int fs15_scene_lod_set_index(FS15SceneLodRecord* n,uint32_t index,const FS15SceneLodServices* s) {
    if(!node(n)||!children(n->node))return 0;
    if(n->index==index)return 1;
    const size_t count=(n->node.children_end-n->node.children_begin)/8;
    if(count&&(!s||!s->mark_child))return 0;
    n->index=index;const auto* values=reinterpret_cast<const uintptr_t*>(n->node.children_begin);
    for(size_t i=0;i<count;++i)if(!values[i]||!s->mark_child(s->user,reinterpret_cast<FS15SceneStateRecord*>(values[i]),(static_cast<uint32_t>(i)<<30)|index))return 0;
    return 1;
}
extern "C" int fs15_scene_lod_child_token(const FS15SceneLodRecord* n,const FS15SceneStateRecord* child,uint32_t* token) {
    if(!node(n)||!token||!children(n->node))return 0;
    if(!child)return fs15_scene_inherited_lod_token(&n->node,token);
    if(n->index==0x3fffffffu){*token=0xffffffffu;return 1;}
    const auto* values=reinterpret_cast<const uintptr_t*>(n->node.children_begin);const size_t count=(n->node.children_end-n->node.children_begin)/8;
    for(size_t i=0;i<count;++i)if(values[i]==reinterpret_cast<uintptr_t>(child)){*token=(static_cast<uint32_t>(i)<<30)|n->index;return 1;}
    *token=0xffffffff;return 1;
}
extern "C" int fs15_scene_inherited_lod_token(const FS15SceneStateRecord* n,uint32_t* token) {
    if(!n||!token)return 0;
    const FS15SceneStateRecord* cursor=n;
    for(unsigned depth=0;depth<256;++depth) {
        if(!(cursor->flags&8)||!cursor->parent){*token=0xffffffff;return 1;}
        const auto* parent=reinterpret_cast<const FS15SceneStateRecord*>(cursor->parent);
        if(!(parent->type_flags&0x10)){*token=0xffffffff;return 1;}
        if(parent->vtable==lod_table)return fs15_scene_lod_child_token(reinterpret_cast<const FS15SceneLodRecord*>(parent),cursor,token);
        if(parent->vtable!=0x1404c0cd8&&parent->vtable!=0x1404c5d98)return 0;
        cursor=parent;
    }
    return 0;
}
extern "C" int fs15_scene_lod_register(FS15SceneLodManager* m,FS15SceneLodRecord* n,const float* position,const float* distances,uint32_t count,uint32_t* out,const FS15SceneLifecycleServices* memory) {
    if(!manager(m)||!n||!out||!fields(position,distances,count)||!memory||!memory->allocate||!memory->deallocate)return 0;
    if(m->count==m->capacity) {
        if(m->capacity>=64512)return 0;
        const uint16_t capacity=static_cast<uint16_t>(m->capacity+1024u);
        auto* entries=static_cast<FS15SceneLodEntry*>(memory->allocate(memory->user,size_t(capacity)*sizeof(*m->entries)));if(!entries)return 0;
        if(m->count)std::memcpy(entries,m->entries,size_t(m->count)*sizeof(*entries));
        // Original free accepts null. Portable services have the same contract.
        memory->deallocate(memory->user,m->entries);m->entries=entries;m->capacity=capacity;
    }
    auto& e=m->entries[m->count];e.node=n;entry(e,position,distances,count,static_cast<uint16_t>(m->stamp));*out=m->count;++m->count;return 1;
}
extern "C" int fs15_scene_lod_move(FS15SceneLodManager* m,uint32_t index,const float* position,const float* distances,uint32_t count) {
    if(!manager(m)||index>=m->count||!fields(position,distances,count))return 0;
    entry(m->entries[index],position,distances,count,static_cast<uint16_t>(m->stamp));return 1;
}
extern "C" int fs15_scene_lod_remove(FS15SceneLodManager* m,uint32_t index,const FS15SceneLodServices* s) {
    if(!manager(m))return 0;
    if(index>=m->count)return 1;
    --m->count;
    if(index<m->count){m->entries[index]=m->entries[m->count];return fs15_scene_lod_set_index(m->entries[index].node,index,s);}
    return 1;
}
extern "C" int fs15_scene_lod_test(FS15SceneLodManager* m,uint32_t token,int* visible) {
    if(!manager(m)||!visible||(token&0x3fffffffu)>=m->count)return 0;
    for(float v:m->viewer)if(!std::isfinite(v))return 0;
    if(!std::isfinite(m->distance_factor_squared))return 0;
    auto& e=m->entries[token&0x3fffffffu];if(e.levels>4)return 0;
    if(e.stamp!=static_cast<uint16_t>(m->stamp)) {
        e.stamp=static_cast<uint16_t>(m->stamp);const float x=e.position[0]-m->viewer[0],y=e.position[1]-m->viewer[1],z=e.position[2]-m->viewer[2];
        const float squared=z*z+(x*x+y*y);if(!std::isfinite(squared))return 0;
        e.selected=static_cast<uint8_t>(uint32_t(e.levels)-1u);
        if(e.levels>1)for(uint32_t i=1;i<e.levels;++i)if(!(m->distance_factor_squared*e.distance_squared[i]<=squared)){e.selected=static_cast<uint8_t>(i-1);break;}
    }
    *visible=(token>>30)==e.selected;return 1;
}
extern "C" int fs15_scene_lod_enqueue(FS15SceneLodContext* context,FS15SceneLodRecord* n,const FS15SceneLifecycleServices* memory) {
    if(!context||!node(n))return 0;
    const uintptr_t p=reinterpret_cast<uintptr_t>(n);return fs15_scene_pointer_append(&context->pending,&p,memory);
}
extern "C" int fs15_scene_lod_state(FS15SceneLodRecord* n,uint32_t mask,int32_t origin,FS15SceneLodContext* context,const FS15SceneLodServices* s) {
    if(!node(n)||!services(s)||!s->base_state||!s->base_state(s->user,&n->node,mask,origin))return 0;
    if(mask&3u) {
        n->node.flags|=0x100;
        if(!(n->node.flags&0x400u)&&(n->node.flags&0x202u)){n->node.flags|=0x400;return fs15_scene_lod_enqueue(context,n,s->memory);}
    }
    return 1;
}
extern "C" int fs15_scene_lod_update(FS15SceneLodRecord* n,FS15SceneLodContext* context,const FS15SceneLodServices* s) {
    if(!node(n)||!context||!manager(&context->manager)||!services(s))return 0;
    n->node.flags&=~uint32_t(0x400);
    if(!(n->node.flags&0x100u))return 1;
    n->node.flags&=~uint32_t(0x100);
    if(!(n->node.flags&2u)) {
        if(n->node.flags&0x200u){n->node.flags&=~uint32_t(0x200);if(!fs15_scene_lod_remove(&context->manager,n->index,s))return 0;n->index=0x3fffffff;}
        return 1;
    }
    float position[3];if(!s->world_position||!s->world_position(s->user,&n->node,position))return 0;
    const uint32_t count=static_cast<uint32_t>(std::min((n->node.children_end-n->node.children_begin)/8,uintptr_t(4)));
    if(!(n->node.flags&0x200u)) {
        n->node.flags|=0x200;uint32_t index=0;
        if(!fs15_scene_lod_register(&context->manager,n,position,n->distances,count,&index,s->memory))return 0;
        return fs15_scene_lod_set_index(n,index,s);
    }
    return n->index<context->manager.count?fs15_scene_lod_move(&context->manager,n->index,position,n->distances,count):1;
}
extern "C" int fs15_scene_lod_destruct(FS15SceneLodRecord* n,size_t bytes,uint32_t flags,FS15SceneLodContext* context,const FS15SceneLodServices* s) {
    if(!node(n)||bytes<sizeof(*n)||!context||!manager(&context->manager)||!services(s))return 0;
    auto& pending=context->pending;
    if(pending.end<pending.begin||pending.capacity<pending.end||(pending.end-pending.begin)%8||(!pending.begin&&pending.end))return 0;
    if(n->node.flags&0x400u) {
        n->node.flags&=~uint32_t(0x400);auto* values=reinterpret_cast<uintptr_t*>(pending.begin);const size_t count=(pending.end-pending.begin)/8;
        for(size_t i=0;i<count;++i)if(values[i]==reinterpret_cast<uintptr_t>(n)){if(i<count-1)values[i]=values[count-1];pending.end-=8;break;}
    }
    if((n->node.flags&0x200u)&&!fs15_scene_lod_remove(&context->manager,n->index,s))return 0;
    return fs15_scene_destruct(n,bytes,FS15_OBJECT_TRS,flags,s->memory);
}
