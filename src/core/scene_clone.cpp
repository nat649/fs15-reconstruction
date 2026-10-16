// Recovered TRS/property/attribute copy and Camera/LOD clone wrappers.
#include "scene_clone.h"
#include "scene_light.h"
#include "scene_attributes.h"
#include "shared_scene.h"
#include <cmath>
#include <cstring>
namespace {
bool known(const FS15SceneStateRecord* n){return n&&(n->vtable==0x1404c5d98||n->vtable==0x1404c1c48||n->vtable==0x1404c1078||n->vtable==0x1404c1d98);}
bool memory(const FS15SceneLifecycleServices* s){return s&&s->allocate&&s->deallocate;}
bool property(uintptr_t pointer,bool target) {
    if(!pointer)return false;
    const auto* p=reinterpret_cast<const void*>(pointer);const auto table=*static_cast<const uint64_t*>(p);
    return table==0x1404c0ca8||((table==0x1404c7a90||table==0x1404c8158)&&(!target||fs15_scene_property_detached(p)));
}
bool finite(const float* values,size_t count){for(size_t i=0;i<count;++i)if(!std::isfinite(values[i]))return false;return true;}
}
extern "C" int fs15_scene_trs_copy(FS15SceneStateRecord* target,FS15SceneStateRecord* source,const FS15SceneCloneServices* s) {
    if(!known(target)||!known(source)||!s||!memory(s->scene.memory)||!s->refresh||!s->scene.rebuild||!s->scene.propagate||
        !property(target->properties,true)||!property(source->properties,false)||!finite(source->local_clip,2)||
        !fs15_scene_attributes_validate(target,sizeof(*target))||!fs15_scene_attributes_validate(source,sizeof(*source)))return 0;
    if((source->flags&0x80u)&&!s->refresh(s->scene.user,source))return 0;
    if(source->flags&0x80u){if(!s->refresh(s->scene.user,source))return 0;if((source->flags&0x80u)&&!s->refresh(s->scene.user,source))return 0;}
    if(!finite(source->translation,3)||!finite(source->rotation,3)||!finite(source->scale,3))return 0;
    std::memmove(target->translation,source->translation,12);std::memmove(target->rotation,source->rotation,12);std::memmove(target->scale,source->scale,12);
    target->flags&=~uint32_t(0x80);if(!s->scene.rebuild(s->scene.user,target))return 0;
    target->flags=(target->flags&~uint32_t(0x20))|(source->flags&0x20u);
    void* copied=nullptr;
    if(!fs15_scene_property_clone_owned(reinterpret_cast<const void*>(source->properties),target,s->scene.memory,&copied))return 0;
    if(!fs15_scene_property_replace(target,sizeof(*target),static_cast<FS15ScenePropertyView*>(copied),s->scene.memory)) {
        if(*static_cast<const uint64_t*>(copied)==0x1404c0ca8)s->scene.memory->deallocate(s->scene.memory->user,copied);
        else fs15_scene_property_destruct_detached(copied,1,s->scene.memory);
        return 0;
    }
    for(unsigned i=0;i<2;++i)if(target->local_clip[i]!=source->local_clip[i]){target->local_clip[i]=source->local_clip[i];if(!s->scene.propagate(s->scene.user,target,8,0))return 0;}
    for(unsigned i=0;i<2;++i)if(target->local_masks[i]!=source->local_masks[i]){target->local_masks[i]=source->local_masks[i];if(!s->scene.propagate(s->scene.user,target,0x10u<<i,0))return 0;}
    if((target->flags&1u)!=(source->flags&1u)){target->flags=(target->flags&~uint32_t(1))|(source->flags&1u);if(!s->scene.propagate(s->scene.user,target,2,0))return 0;}
    target->flags=(target->flags&~uint32_t(4))|(source->flags&4u);
    return fs15_scene_attributes_copy(target,sizeof(*target),source,sizeof(*source),s->scene.memory);
}
extern "C" int fs15_scene_clone_node(FS15SceneStateRecord* source,const FS15SceneCloneServices* s,FS15SceneGroupResult* result) {
    if(!known(source)||!s||!result||!memory(s->scene.memory)||!s->constructed)return 0;
    *result={nullptr,0,0};const unsigned kind=source->vtable==0x1404c1d98?3u:source->vtable==0x1404c1c48?2u:1u;
    const size_t bytes=kind==3?sizeof(FS15SceneLightRecord):source->vtable==0x1404c1078?sizeof(FS15SceneCameraRecord):kind==2?sizeof(FS15SceneLodRecord):sizeof(FS15SceneStateRecord);
    const char* name=source->name?reinterpret_cast<const char*>(source->name):"";const size_t name_bytes=std::strlen(name)+1;
    const auto* m=s->scene.memory;auto* target=static_cast<FS15SceneStateRecord*>(m->allocate(m->user,bytes));if(!target)return 0;
    int ok=0;
    if(kind==3) {
        const auto& light=*reinterpret_cast<const FS15SceneLightRecord*>(source);FS15SceneLightParameters p{};
        p.type=light.type;std::memcpy(p.diffuse,light.diffuse,12);std::memcpy(p.specular,light.specular,12);
        p.emit_diffuse=p.emit_specular=p.cast_shadow=p.perspective=1;p.depth_bias=light.depth_bias;p.depth_slope_bias=light.depth_slope_bias;p.depth_slope_clamp=light.depth_slope_clamp;p.depth_resolution=light.depth_resolution;p.range=light.range;p.decay=light.decay;p.cone_angle=light.cone_angle;p.drop_off=light.drop_off;p.shadow_far=light.shadow_far;p.shadow_extrusion=light.shadow_extrusion;p.shadow_perspective=light.shadow_perspective;p.shadow_splits=light.shadow_splits;p.shadow_split_parameter=light.shadow_split_parameter;
        ok=fs15_scene_light_construct(reinterpret_cast<FS15SceneLightRecord*>(target),bytes,name,name_bytes,&p,nullptr,m);
        if(ok)reinterpret_cast<FS15SceneLightRecord*>(target)->light_flags=light.light_flags;
    }else if(source->vtable==0x1404c1078) {
        const auto* camera=reinterpret_cast<const FS15SceneCameraRecord*>(source);
        ok=fs15_scene_camera_construct(reinterpret_cast<FS15SceneCameraRecord*>(target),bytes,name,name_bytes,camera->fov,camera->near_clip,camera->far_clip,camera->orthographic,camera->orthographic_height,m);
    }else if(kind==2) {
        const auto* lod=reinterpret_cast<const FS15SceneLodRecord*>(source);uint32_t count=1;float previous=0;
        for(;count<4;++count){const float distance=lod->distances[count];if(previous==distance)break;previous=distance;}
        if(count==4)count=3;
        ok=fs15_scene_lod_construct(reinterpret_cast<FS15SceneLodRecord*>(target),bytes,name,name_bytes,lod->distances,count,m);
    }else ok=fs15_scene_construct(target,bytes,FS15_OBJECT_TRS,name,name_bytes,0,1,m);
    if(!ok){m->deallocate(m->user,target);return 0;}
    *result={target,bytes,kind};
    if(!s->constructed(s->scene.user,target,kind,bytes))return 0;
    return fs15_scene_trs_copy(target,source,s);
}
