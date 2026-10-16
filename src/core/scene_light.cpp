// Recovered Light records, dirty/marker dispatch, pending lifetime and I3D XML.
#include "scene_light.h"
#include "scene_owned.h"
#include <cmath>
#include <cstring>
#include <set>
#include <stdexcept>
namespace {
constexpr uintptr_t table=0x1404c1d98;
bool memory(const FS15SceneLifecycleServices* s){return s&&s->allocate&&s->deallocate;}
bool light(const FS15SceneLightRecord* n){return n&&n->node.vtable==table;}
bool finite(const float* p,size_t count){for(size_t i=0;i<count;++i)if(!std::isfinite(p[i]))return false;return true;}
bool vector(const FS15SceneVector& v){return v.end>=v.begin&&v.capacity>=v.end&&(v.end-v.begin)%8==0&&(v.capacity-v.begin)%8==0&&(v.begin||(!v.end&&!v.capacity))&&(v.capacity-v.begin)/8<=1048576;}
int pending(FS15SceneLightRecord* n,FS15SceneLightContext* context,const FS15SceneLifecycleServices* s) {
    if(!(n->node.flags&0x800u)&&(n->node.flags&0x202u)){n->node.flags|=0x800;return fs15_scene_light_enqueue(context,n,s);}return 1;
}
bool known(const FS15SceneStateRecord* n){return n&&(n->vtable==table||n->vtable==0x1404c0cd8||n->vtable==0x1404c5d98||n->vtable==0x1404c1078||n->vtable==0x1404c1c48);}
void preflight(FS15SceneStateRecord* n,unsigned depth,std::set<FS15SceneStateRecord*>& seen) {
    if(!known(n)||depth>=256||seen.size()>=1048576||!seen.insert(n).second||!vector({n->children_begin,n->children_end,n->children_capacity}))throw std::runtime_error("Invalid light marker subtree");
    for(uintptr_t p=n->children_begin;p<n->children_end;p+=8)preflight(*reinterpret_cast<FS15SceneStateRecord**>(p),depth+1,seen);
}
int mark(FS15SceneStateRecord* n,uint32_t token,FS15SceneLightContext* c,const FS15SceneLifecycleServices* s) {
    if(n->vtable==table){auto* l=reinterpret_cast<FS15SceneLightRecord*>(n);if(l->lod_token!=token){n->flags|=0x400;l->lod_token=token;if(!pending(l,c,s))return 0;}}
    n->flags|=8;
    for(uintptr_t p=n->children_begin;p<n->children_end;p+=8)if(!mark(*reinterpret_cast<FS15SceneStateRecord**>(p),token,c,s))return 0;
    return 1;
}
struct Attributes {
    const FS15SceneXmlAttribute* a;size_t count;
    const FS15SceneXmlAttribute* field(const char* name)const{for(size_t i=0;i<count;++i)if(std::strcmp(a[i].name,name)==0)return a+i;return nullptr;}
    const char* text(const char* name)const{const auto* p=field(name);return p?p->value:nullptr;}
    bool equal(const char* p,const char* expected)const {
        if(!p)return false;
        for(size_t i=0;;++i){char c=p[i];if(c>='A'&&c<='Z')c=static_cast<char>(c+32);if(c!=expected[i])return false;if(!c)return true;}
    }
    bool boolean(const char* name,bool fallback)const{const char* p=text(name);return p?equal(p,"true"):fallback;}
    void number(const char* name,float& value)const{const auto* p=field(name);if(!p||!p->value)return;size_t consumed=0;if(!fs15_parse_scene_float(p->value,p->value_bytes,&value,&consumed)||!std::isfinite(value))throw std::runtime_error("Invalid light scalar");}
    void triple(const char* name,float* out)const {
        const auto* p=field(name);if(!p||!p->value)return;size_t offset=0;const size_t bytes=std::strlen(p->value)+1;
        for(unsigned i=0;i<3;++i){size_t consumed=0;if(offset>=bytes||!fs15_parse_scene_float(p->value+offset,bytes-offset,out+i,&consumed)||!std::isfinite(out[i]))throw std::runtime_error("Invalid light color");offset+=consumed+1;}
    }
    void digits(const char* name,uint32_t& out)const{const char* p=text(name);if(!p)return;out=0;for(;*p>='0'&&*p<='9';++p)out=out*10u+static_cast<uint32_t>(*p-'0');}
};
}
extern "C" int fs15_scene_light_enqueue(FS15SceneLightContext* context,FS15SceneLightRecord* n,const FS15SceneLifecycleServices* s) {
    if(!context||!light(n))return 0;
    const uintptr_t pointer=reinterpret_cast<uintptr_t>(n);return fs15_scene_pointer_append(&context->pending,&pointer,s);
}
extern "C" int fs15_scene_light_dirty(FS15SceneLightRecord* n,FS15SceneLightContext* c,const FS15SceneLifecycleServices* s) {
    if(!light(n)||!std::isfinite(n->cone_angle)||!std::isfinite(n->drop_off))return 0;
    const float angle=((n->cone_angle*0.5f)*3.1415927410125732421875f)/180.0f;if(!std::isfinite(angle))return 0;
    n->outer_cos=std::cos(angle);if(n->drop_off>5)n->drop_off=5;if(n->drop_off<0)n->drop_off=0;
    n->inner_cos=std::cos(angle-(angle/5.0f)*n->drop_off);n->node.flags|=0x500;return pending(n,c,s);
}
extern "C" int fs15_scene_light_construct(FS15SceneLightRecord* n,size_t bytes,const char* name,size_t name_bytes,const FS15SceneLightParameters* p,FS15SceneLightContext* context,const FS15SceneLifecycleServices* s) {
    if(!n||bytes<sizeof(*n)||!p||!finite(p->diffuse,3)||!finite(p->specular,3))return 0;
    const float values[]={p->depth_bias,p->depth_slope_bias,p->depth_slope_clamp,p->range,p->cone_angle,p->drop_off,p->shadow_far,p->shadow_extrusion,p->shadow_perspective,p->shadow_split_parameter};if(!finite(values,10))return 0;
    if(!std::isfinite(((p->cone_angle*0.5f)*3.1415927410125732421875f)/180.0f))return 0;
    if(!fs15_scene_construct(n,bytes,FS15_OBJECT_TRS_TYPED,name,name_bytes,0x17,1,s))return 0;
    n->node.vtable=table;n->node.type_flags|=0x2000;n->spatial_table=0x1404d9068;n->spatial_links[0]=n->spatial_links[1]=n->render_resources=0;n->spatial_count=0;
    n->type=p->type;std::memcpy(n->diffuse,p->diffuse,12);std::memcpy(n->specular,p->specular,12);n->range=p->range;n->decay=p->decay?p->decay:1;n->cone_angle=p->cone_angle;n->drop_off=p->drop_off;
    n->light_flags=1;if(p->emit_diffuse)n->light_flags=3;if(p->emit_specular)n->light_flags|=4;if(p->cast_shadow)n->light_flags|=0x20;if(p->perspective)n->light_flags|=0x40;
    n->depth_bias=p->depth_bias;n->depth_slope_bias=p->depth_slope_bias;n->depth_slope_clamp=p->depth_slope_clamp;n->depth_resolution=p->depth_resolution;n->shadow_far=p->shadow_far;n->shadow_extrusion=p->shadow_extrusion;n->shadow_perspective=p->shadow_perspective;n->shadow_split_parameter=p->shadow_split_parameter;
    n->shadow_splits=p->shadow_splits?(p->shadow_splits>4?4:p->shadow_splits):1;if(n->type)n->shadow_splits=1;n->affected_masks=n->lod_token=0xffffffff;return fs15_scene_light_dirty(n,context,s);
}
extern "C" int fs15_scene_light_post(FS15SceneLightRecord* n,uint32_t mask,FS15SceneLightContext* c,const FS15SceneLifecycleServices* s) {
    if(!light(n))return 0;
    if(mask&0xcu){if(mask&4u)n->lod_token=0xffffffff;n->node.flags|=0x400;if(!pending(n,c,s))return 0;}
    if(mask&3u){n->node.flags|=0x500;if(!pending(n,c,s))return 0;}return 1;
}
extern "C" int fs15_scene_light_mark_tree(FS15SceneStateRecord* n,uint32_t token,FS15SceneLightContext* c,const FS15SceneLifecycleServices* s) {
    try {std::set<FS15SceneStateRecord*> seen;preflight(n,0,seen);return mark(n,token,c,s);}catch(...){return 0;}
}
extern "C" int fs15_scene_light_destruct(FS15SceneLightRecord* n,size_t bytes,uint32_t flags,FS15SceneLightContext* c,const FS15SceneLifecycleServices* s) {
    if(!light(n)||bytes<sizeof(*n)||!c||!memory(s)||!vector(c->pending)||(n->node.flags&0x200u)||n->render_resources)return 0;
    if(n->node.flags&0x800u){auto& v=c->pending;auto* values=reinterpret_cast<uintptr_t*>(v.begin);const size_t count=(v.end-v.begin)/8;for(size_t i=0;i<count;++i)if(values[i]==reinterpret_cast<uintptr_t>(n)){if(i+1<count)values[i]=values[count-1];v.end-=8;break;}}
    n->spatial_table=0x1404c7ce8;return fs15_scene_destruct(n,bytes,FS15_OBJECT_TRS_TYPED,flags,s);
}
extern "C" int fs15_scene_light_begin(FS15SceneStateRecord** current,const FS15SceneXmlAttribute* attrs,size_t count,const FS15SceneLightXmlServices* s,FS15SceneLightContext* context,FS15SceneGroupResult* result) {
    if(!current||!*current||!s||!result||!memory(s->scene.common.memory)||!s->scene.constructed||!s->scene.link)return 0;
    *result={nullptr,0,0};const char* name=nullptr;if(!fs15_scene_xml_find(attrs,count,"name",5,&name))return 0;if(!name)return 1;
    try {
        Attributes a{attrs,count};FS15SceneLightParameters p{};p.type=1;p.range=1;p.decay=2;p.cone_angle=60;p.drop_off=4;p.depth_bias=p.depth_slope_bias=0.005f;p.depth_slope_clamp=0.02f;p.depth_resolution=512;p.shadow_far=80;p.shadow_extrusion=200;p.shadow_perspective=5;p.shadow_splits=1;p.shadow_split_parameter=0.4f;
        for(unsigned i=0;i<3;++i)p.diffuse[i]=p.specular[i]=1;
        const char* type=a.text("type");if(a.equal(type,"directional"))p.type=0;else if(a.equal(type,"spot"))p.type=2;
        a.triple("diffuseColor",p.diffuse);a.triple("specularColor",p.specular);p.emit_diffuse=a.boolean("emitDiffuse",true);p.emit_specular=a.boolean("emitSpecular",true);p.cast_shadow=a.boolean("castShadowMap",false);
        if(p.cast_shadow) {
            a.number("depthMapBias",p.depth_bias);a.number("depthMapSlopeScaleBias",p.depth_slope_bias);a.number("depthMapSlopeClamp",p.depth_slope_clamp);
            if(a.text("depthMapResolution")&&(!s->scene.common.integer||!s->scene.common.integer(s->scene.common.user,a.text("depthMapResolution"),&p.depth_resolution)))return 0;
            p.perspective=a.boolean("shadowPerspective",false);a.number("shadowPerspectiveParameter",p.shadow_perspective);a.number("shadowFarDistance",p.shadow_far);a.number("shadowExtrusionDistance",p.shadow_extrusion);a.digits("numShadowMapSplits",p.shadow_splits);a.number("shadowMapSplitDistancesParameter",p.shadow_split_parameter);
        }
        a.number("range",p.range);a.digits("decayRate",p.decay);if(p.type==2){a.number("coneAngle",p.cone_angle);a.number("dropOff",p.drop_off);}
        const auto* m=s->scene.common.memory;auto* n=static_cast<FS15SceneLightRecord*>(m->allocate(m->user,sizeof(FS15SceneLightRecord)));if(!n)return 0;
        if(!fs15_scene_light_construct(n,sizeof(*n),name,std::strlen(name)+1,&p,context,m)){m->deallocate(m->user,n);return 0;}
        *result={&n->node,sizeof(*n),3};
        if(!s->scene.constructed(s->scene.common.user,&n->node,3,sizeof(*n)))return 0;
        if(a.text("affectedLightMasks")&&(!s->unsigned_integer||!s->unsigned_integer(s->scene.common.user,a.text("affectedLightMasks"),&n->affected_masks)))return 0;
        if(!fs15_scene_xml_apply(&n->node,attrs,count,0,1,0,"",1,&s->scene.common)||!s->scene.link(s->scene.common.user,*current,&n->node,UINT32_MAX))return 0;
        *current=&n->node;return 1;
    }catch(...){return 0;}
}
