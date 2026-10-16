// Native culling-grid storage and Light bounds/update, without GPU resources.
#include "scene_spatial.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
namespace {
bool memory(const FS15SceneLifecycleServices* s){return s&&s->allocate&&s->deallocate;}
bool finite(float f){return std::isfinite(f);}
bool sphere(const FS15SceneSpatialSphere* s){return !s||(finite(s->center[0])&&finite(s->center[1])&&finite(s->center[2])&&finite(s->radius));}
bool cell(const FS15SceneSpatialCell* c){return c&&c->count<=c->capacity&&c->capacity<=65504&&(!c->capacity||c->entries);}
uint32_t level_count(uint32_t d){uint32_t n=1;while(d>1){d>>=1;++n;}return n;}
uint32_t cell_count(uint32_t levels){uint32_t n=0,width=1;for(uint32_t i=0;i<levels;++i){n+=width*width;width*=2;}return n;}
bool grid(const FS15SceneSpatialGrid* g){return g&&g->cells&&g->dimension&&g->dimension<=128&&!(g->dimension&(g->dimension-1))&&g->levels==level_count(g->dimension)&&g->count==cell_count(g->levels)&&finite(g->scale_x)&&finite(g->scale_z);}
uint32_t index(const FS15SceneSpatialGrid& g,uint32_t x,uint32_t z,uint32_t level){
    uint32_t morton=0;for(unsigned i=0;i<7;++i){morton|=((x>>i)&1u)<<(2*i);morton|=((z>>i)&1u)<<(2*i+1);}
    const uint32_t shifted=morton<<(2*(g.levels-level-1));uint32_t result=level+shifted;
    for(uint32_t i=1;i+1<g.levels;++i)result+=shifted>>(2*i);
    return result;
}
uint32_t packed(uint32_t slot,uint32_t level,uint32_t x,uint32_t z){return (std::min(slot,0x7fffu)<<17)|(z<<10)|(x<<3)|level;}
void fields(FS15SceneSpatialEntry& e,const FS15SceneSpatialSphere* s,float near_clip,float far_clip,uint32_t token){
    if(s){std::memcpy(e.center,s->center,12);e.radius=s->radius;e.clip[0]=near_clip;e.clip[1]=far_clip;}
    else {for(float& f:e.center)f=0;e.radius=e.clip[0]=std::numeric_limits<float>::max();e.clip[1]=-std::numeric_limits<float>::max();}
    e.lod_token=token;
}
bool coordinate(float value,uint32_t size,uint32_t& out){
    if(!finite(value)||value>=2147483648.0f||value< -2147483648.0f)return false;
    const int32_t n=static_cast<int32_t>(value);out=n<0?0:std::min(static_cast<uint32_t>(n),size-1);return true;
}
bool locate(const FS15SceneSpatialGrid& g,uint32_t token,uint32_t& i,uint32_t& slot){
    const uint32_t level=token&7,x=(token>>3)&127,z=(token>>10)&127;
    if(level>=g.levels||x>=(1u<<level)||z>=(1u<<level))return false;
    i=index(g,x,z,level);slot=token>>17;return i<g.count&&cell(g.cells+i);
}
uint32_t find(const FS15SceneSpatialCell& c,uint32_t slot,const uint32_t* token){
    if(slot!=0x7fff)return slot;
    for(uint32_t i=0x7fff;i<c.count;++i)if(c.entries[i].token==token)return i;
    return c.count;
}
bool light(const FS15SceneLightRecord* n){return n&&n->node.vtable==0x1404c1d98;}
FS15SceneSpatialSphere* bounds(FS15SceneLightRecord* n){return &n->spatial;}
uint32_t* handle(FS15SceneLightRecord* n){return &n->spatial_token;}
bool pending(const FS15SceneVector& v){return v.end>=v.begin&&v.capacity>=v.end&&(v.end-v.begin)%8==0&&(v.capacity-v.begin)%8==0&&(!v.capacity||v.begin)&&(v.capacity-v.begin)/8<=1048576;}
}
extern "C" int fs15_scene_spatial_index(const FS15SceneSpatialGrid* g,uint32_t x,uint32_t z,uint32_t level,uint32_t* out){
    if(!g||!out||g->levels<1||g->levels>8||level>=g->levels||x>=(1u<<level)||z>=(1u<<level))return 0;
    *out=index(*g,x,z,level);return 1;
}
extern "C" int fs15_scene_spatial_cell_append(FS15SceneSpatialCell* c,uint32_t* token,const FS15SceneSpatialSphere* s,float near_clip,float far_clip,uint32_t lod,uint32_t* out,const FS15SceneLifecycleServices* m){
    if(!cell(c)||!token||!out||!sphere(s)||!finite(near_clip)||!finite(far_clip)||!memory(m)||c->count>=65504)return 0;
    if(c->count>=c->capacity){const uint16_t capacity=static_cast<uint16_t>(c->capacity+(c->capacity<64?8:32));if(capacity>65504)return 0;auto* next=static_cast<FS15SceneSpatialEntry*>(m->allocate(m->user,size_t(capacity)*sizeof(FS15SceneSpatialEntry)));if(!next)return 0;
        if(c->count)std::memcpy(next,c->entries,size_t(c->count)*sizeof(*next));
        m->deallocate(m->user,c->entries);c->entries=next;c->capacity=capacity;}
    auto& e=c->entries[c->count];fields(e,s,near_clip,far_clip,lod);e.token=token;*out=c->count++;return 1;
}
extern "C" int fs15_scene_spatial_cell_move(FS15SceneSpatialCell* c,uint32_t slot,const FS15SceneSpatialSphere* s,float near_clip,float far_clip,uint32_t lod){
    if(!cell(c)||slot>=c->count||!sphere(s)||!finite(near_clip)||!finite(far_clip))return 0;
    fields(c->entries[slot],s,near_clip,far_clip,lod);return 1;
}
extern "C" int fs15_scene_spatial_cell_remove(FS15SceneSpatialCell* c,uint32_t slot){
    if(!cell(c)||slot>=c->count)return 0;
    --c->count;if(slot<c->count){auto& e=c->entries[slot];const auto& last=c->entries[c->count];
        // The native copy leaves destination +0x24 opaque bytes untouched.
        std::memcpy(&e,&last,0x24);if(!last.token)return 0;*last.token=(*last.token&0x1ffff)|(std::min(slot,0x7fffu)<<17);}
    return 1;
}
extern "C" int fs15_scene_spatial_select(const FS15SceneSpatialGrid* g,const FS15SceneSpatialSphere* s,uint32_t* level,uint32_t* x,uint32_t* z,uint32_t* out){
    if(!grid(g)||!s||!sphere(s)||!level||!x||!z||!out)return 0;
    uint32_t ax,az,bx,bz;if(!coordinate(((s->center[0]-s->radius)-g->min_x)*g->scale_x,g->dimension,ax)||!coordinate(((s->center[2]-s->radius)-g->min_z)*g->scale_z,g->dimension,az)||!coordinate(((s->radius+s->center[0])-g->min_x)*g->scale_x,g->dimension,bx)||!coordinate(((s->center[2]+s->radius)-g->min_z)*g->scale_z,g->dimension,bz))return 0;
    uint32_t different=std::max(ax^bx,az^bz),shift=0;while(different){++shift;different>>=1;}
    *level=g->levels-shift-1;*x=ax>>shift;*z=az>>shift;*out=index(*g,*x,*z,*level);return 1;
}
extern "C" int fs15_scene_spatial_add(FS15SceneSpatialGrid* g,uint32_t* token,const FS15SceneSpatialSphere* s,float near_clip,float far_clip,uint32_t lod,const FS15SceneLifecycleServices* m){
    if(!grid(g)||!token)return 0;
    uint32_t level=0,x=0,z=0,i=0,slot=0;if(s&&!fs15_scene_spatial_select(g,s,&level,&x,&z,&i))return 0;
    if(!fs15_scene_spatial_cell_append(g->cells+i,token,s,near_clip,far_clip,lod,&slot,m))return 0;
    *token=packed(slot,level,x,z);return 1;
}
extern "C" int fs15_scene_spatial_remove(FS15SceneSpatialGrid* g,uint32_t* token){
    if(!grid(g)||!token)return 0;
    uint32_t i,slot;if(!locate(*g,*token,i,slot))return 0;auto& c=g->cells[i];slot=find(c,slot,token);
    if(slot>=c.count)return 1;
    return fs15_scene_spatial_cell_remove(&c,slot);
}
extern "C" int fs15_scene_spatial_move(FS15SceneSpatialGrid* g,uint32_t* token,const FS15SceneSpatialSphere* s,float near_clip,float far_clip,uint32_t lod,const FS15SceneLifecycleServices* m){
    if(!grid(g)||!token||!memory(m))return 0;
    uint32_t old,slot,level=0,x=0,z=0,next=0;if(!locate(*g,*token,old,slot))return 0;
    if(s&&!fs15_scene_spatial_select(g,s,&level,&x,&z,&next))return 0;
    slot=find(g->cells[old],slot,token);
    if(old==next){if(slot>=g->cells[old].count)return 1;return fs15_scene_spatial_cell_move(g->cells+old,slot,s,near_clip,far_clip,lod);}
    if(slot<g->cells[old].count&&!fs15_scene_spatial_cell_remove(g->cells+old,slot))return 0;
    return fs15_scene_spatial_add(g,token,s,near_clip,far_clip,lod,m);
}
extern "C" int fs15_scene_spatial_init(FS15SceneSpatialGrid* g,const float* low,const float* high,int32_t requested,const FS15SceneLifecycleServices* m){
    if(!g||!low||!high||!memory(m))return 0;
    for(unsigned i=0;i<3;++i)if(!finite(low[i])||!finite(high[i]))return 0;
    if(!(low[0]<high[0])||!(low[2]<high[2])||!finite(high[0]-low[0])||!finite(high[2]-low[2]))return 0;
    uint32_t dimension=1;while(dimension<128&&static_cast<int32_t>(dimension)<requested)dimension*=2;
    if(g->cells&&(!grid(g)))return 0;
    if(g->cells&&g->dimension==dimension&&g->min_x==low[0]&&g->min_y==low[1]&&g->min_z==low[2]&&g->max_x==high[0]&&g->max_y==high[1]&&g->max_z==high[2])return 1;
    FS15SceneSpatialGrid next=*g;next.dimension=dimension;next.levels=level_count(dimension);next.count=cell_count(next.levels);next.min_x=low[0];next.min_y=low[1];next.min_z=low[2];next.max_x=high[0];next.max_y=high[1];next.max_z=high[2];next.scale_x=float(dimension)/(high[0]-low[0]);next.scale_z=float(dimension)/(high[2]-low[2]);if(!finite(next.scale_x)||!finite(next.scale_z))return 0;
    next.cells=static_cast<FS15SceneSpatialCell*>(m->allocate(m->user,size_t(next.count)*sizeof(*next.cells)));if(!next.cells)return 0;
    float width=high[0]-low[0],height=high[2]-low[2];uint32_t side=1;
    for(uint32_t level=0;level<next.levels;++level){const float half_x=width*0.5f,half_z=height*0.5f;float cz=half_z+low[2];
        for(uint32_t z=0;z<side;++z){float cx=half_x+low[0];for(uint32_t x=0;x<side;++x){auto& c=next.cells[index(next,x,z,level)];c.entries=nullptr;c.count=c.capacity=0;c.center_x=cx;c.center_z=cz;c.half_x=half_x;c.half_z=half_z;c.skip=index(next,x,z,level)+cell_count(next.levels-level);cx+=width;}cz+=height;}
        width*=0.5f;height=half_z;side*=2;}
    auto* previous=g->cells;const uint32_t previous_count=g->cells?g->count:0;*g=next;
    for(uint32_t i=0;i<previous_count;++i){const auto& c=previous[i];if(!cell(&c))return 0;for(uint32_t j=0;j<c.count;++j){const auto& e=c.entries[j];FS15SceneSpatialSphere s{};std::memcpy(s.center,e.center,12);s.radius=e.radius;
            const auto* pointer=e.radius==std::numeric_limits<float>::max()?nullptr:&s;
            if(!fs15_scene_spatial_add(g,e.token,pointer,e.clip[0],e.clip[1],e.lod_token,m))return 0;}}
    if(previous)m->deallocate(m->user,previous);
    return 1;
}
extern "C" int fs15_scene_spatial_release(FS15SceneSpatialGrid* g,const FS15SceneLifecycleServices* m){
    if(!g||!memory(m)||(g->cells&&!grid(g)))return 0;
    if(g->cells){for(uint32_t i=0;i<g->count;++i)if(!cell(g->cells+i))return 0;for(uint32_t i=0;i<g->count;++i)if(g->cells[i].entries)m->deallocate(m->user,g->cells[i].entries);m->deallocate(m->user,g->cells);}
    g->cells=nullptr;g->count=0;return 1;
}
extern "C" int fs15_scene_light_bounds(FS15SceneLightRecord* n,const FS15SceneLightUpdateServices* s){
    if(!light(n)||!s)return 0;
    if(!(n->node.flags&0x100u))return 1;
    n->node.flags&=~uint32_t(0x100);float matrix[16];if(!s->world_matrix||!s->world_matrix(s->user,&n->node,matrix))return 0;
    for(float f:matrix)if(!finite(f))return 0;
    auto* b=bounds(n);
    if(n->type==2){const float distance=-((n->range*0.5f)/n->inner_cos);if(!finite(distance))return 0;for(unsigned i=0;i<3;++i)b->center[i]=((matrix[4+i]*0.0f+matrix[i]*0.0f)+distance*matrix[8+i])+matrix[12+i];}
    else std::memcpy(b->center,matrix+12,12);
    b->radius=n->range;return sphere(b);
}
extern "C" int fs15_scene_light_update(FS15SceneLightRecord* n,FS15SceneSpatialGrid* g,const FS15SceneLightUpdateServices* s){
    if(!light(n)||!grid(g)||!s||!memory(s->memory))return 0;
    n->node.flags&=~uint32_t(0x800);if(!(n->node.flags&0x400u))return 1;
    n->node.flags&=~uint32_t(0x400);if(!fs15_scene_light_bounds(n,s))return 0;
    if((n->node.flags&2u)&&(n->light_flags&1u)){
        if(n->lod_token==0xffffffff&&(n->node.flags&8u)&&!fs15_scene_inherited_lod_token(&n->node,&n->lod_token))return 0;
        const auto* b=n->node.flags&4u?nullptr:bounds(n);
        if(n->node.flags&0x200u)return fs15_scene_spatial_move(g,handle(n),b,n->node.world_clip[0],n->node.world_clip[1],n->lod_token,s->memory);
        n->node.flags|=0x200;return fs15_scene_spatial_add(g,handle(n),b,n->node.world_clip[0],n->node.world_clip[1],n->lod_token,s->memory);
    }
    if(n->node.flags&0x200u){n->node.flags&=~uint32_t(0x200);return fs15_scene_spatial_remove(g,handle(n));}return 1;
}
extern "C" int fs15_scene_light_spatial_destruct(FS15SceneLightRecord* n,size_t bytes,uint32_t flags,FS15SceneSpatialGrid* g,FS15SceneLightContext* c,const FS15SceneLifecycleServices* s){
    if(!light(n)||bytes<sizeof(*n)||n->render_resources||!grid(g)||!c||!pending(c->pending)||!memory(s))return 0;
    // Native pending removal precedes spatial removal and inherited cleanup.
    if(n->node.flags&0x800u){auto& v=c->pending;auto* values=reinterpret_cast<uintptr_t*>(v.begin);const size_t count=(v.end-v.begin)/8;for(size_t i=0;i<count;++i)if(values[i]==reinterpret_cast<uintptr_t>(n)){if(i+1<count)values[i]=values[count-1];v.end-=8;break;}}
    if(n->node.flags&0x200u){n->node.flags&=~uint32_t(0x200);if(!fs15_scene_spatial_remove(g,handle(n)))return 0;}
    // Keep queued bit bytes, but avoid running pending removal twice.
    FS15SceneLightContext empty{};return fs15_scene_light_destruct(n,bytes,flags,&empty,s);
}
extern "C" int fs15_scene_lights_flush(FS15SceneLodContext* lod,const FS15SceneLodServices* ls,FS15SceneLightContext* lights,FS15SceneSpatialGrid* g,const FS15SceneLightUpdateServices* s){
    if(!lod||!lights||!ls||!s||!pending(lod->pending)||!pending(lights->pending)||!grid(g))return 0;
    const size_t lod_count=(lod->pending.end-lod->pending.begin)/8;
    for(size_t i=0;i<lod_count;++i)if(!fs15_scene_lod_update(reinterpret_cast<FS15SceneLodRecord*>(reinterpret_cast<uintptr_t*>(lod->pending.begin)[i]),lod,ls))return 0;
    const size_t count=(lights->pending.end-lights->pending.begin)/8;
    for(size_t i=0;i<count;++i)if(!fs15_scene_light_update(reinterpret_cast<FS15SceneLightRecord*>(reinterpret_cast<uintptr_t*>(lights->pending.begin)[i]),g,s))return 0;
    lights->pending.end=lights->pending.begin;lod->pending.end=lod->pending.begin;return 1;
}
