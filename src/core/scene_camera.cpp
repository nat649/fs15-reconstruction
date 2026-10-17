// Recovered Camera paths; bounded services and storage validation authored.
#include "scene_camera.h"
#include "scene_math.h"
#include "scene_trs.h"
#include "scene_rsqrt.h"
#include <cmath>
#include <cstring>
namespace {
constexpr uintptr_t camera_table=0x1404c1078;
static_assert(sizeof(FS15SceneCameraRecord)==0x190,"Camera layout");
static_assert(offsetof(FS15SceneCameraRecord,orthographic)==0x188,"Camera tail layout");
bool finite(const float* values,size_t n){if(!values)return false;for(size_t i=0;i<n;++i)if(!std::isfinite(values[i]))return false;return true;}
bool camera(const FS15SceneCameraRecord* n){return n&&n->node.vtable==camera_table;}
bool memory(const FS15SceneLifecycleServices* s){return s&&s->allocate&&s->deallocate;}
bool services(const FS15SceneCameraServices* s){return s&&s->world&&s->refresh&&s->rebuild;}
float scalar(const void* p){float f;std::memcpy(&f,p,4);return f;}
void identity(float* out){std::memset(out,0,64);out[0]=out[5]=out[10]=out[15]=1;}
bool field(const FS15SceneXmlAttribute* attrs,size_t count,const char* key,float& out) {
    const char* text=nullptr;if(!fs15_scene_xml_find(attrs,count,key,std::strlen(key)+1,&text))return false;
    if(!text)return true;
    for(size_t i=0;i<count;++i)if(std::strcmp(attrs[i].name,key)==0){size_t consumed=0;return fs15_parse_scene_float(text,attrs[i].value_bytes,&out,&consumed)&&std::isfinite(out);}
    return false;
}
bool is_true(const char* p) {
    if(!p)return false;
    const char expected[]="true";
    for(unsigned i=0;i<5;++i){unsigned char c=static_cast<unsigned char>(p[i]);if(c>='A'&&c<='Z')c=static_cast<unsigned char>(c+32);if(c!=expected[i])return false;}
    return true;
}
}
extern "C" int fs15_scene_camera_construct(FS15SceneCameraRecord* n,size_t bytes,const char* name,size_t name_bytes,float fov,float near_clip,float far_clip,uint8_t orthographic,float height,const FS15SceneLifecycleServices* s) {
    const float parameters[]={fov,near_clip,far_clip,height};
    if(!n||bytes<sizeof(*n)||!finite(parameters,4))return 0;
    if(!fs15_scene_construct(n,bytes,FS15_OBJECT_TRS_TYPED,name,name_bytes,0x16,1,s))return 0;
    n->node.type_flags|=0x8000;n->node.vtable=camera_table;
    n->fov=fov;n->near_clip=near_clip;n->far_clip=far_clip;n->orthographic_height=height;
    n->unknown180=0.1f;n->unknown184=3.0f;n->orthographic=orthographic;return 1;
}
extern "C" int fs15_scene_camera_destruct(FS15SceneCameraRecord* n,size_t bytes,uint32_t flags,const FS15SceneLifecycleServices* s) {
    if(!camera(n)||bytes<sizeof(*n))return 0;
    return fs15_scene_destruct(n,bytes,FS15_OBJECT_TRS_TYPED,flags,s);
}
extern "C" int fs15_scene_camera_begin(FS15SceneStateRecord** current,const FS15SceneXmlAttribute* attrs,size_t count,const FS15SceneGroupServices* s,FS15SceneGroupResult* result) {
    if(!current||!*current||!s||!result||!memory(s->common.memory)||!s->constructed||!s->link)return 0;
    *result={nullptr,0,0};const char* name=nullptr;const char* orthographic=nullptr;
    if(!fs15_scene_xml_find(attrs,count,"name",5,&name))return 0;
    if(!name)return 1;
    float fov=60,near_clip=0.1f,far_clip=1000,height=1;
    if(!field(attrs,count,"fov",fov)||!field(attrs,count,"nearClip",near_clip)||!field(attrs,count,"farClip",far_clip)||!field(attrs,count,"orthographicHeight",height)||
        !fs15_scene_xml_find(attrs,count,"orthographic",13,&orthographic))return 0;
    const auto* m=s->common.memory;auto* n=static_cast<FS15SceneCameraRecord*>(m->allocate(m->user,sizeof(FS15SceneCameraRecord)));if(!n)return 0;
    if(!fs15_scene_camera_construct(n,sizeof(*n),name,std::strlen(name)+1,fov,near_clip,far_clip,is_true(orthographic),height,m)){m->deallocate(m->user,n);return 0;}
    *result={&n->node,sizeof(*n),1};
    if(!s->constructed(s->common.user,&n->node,1,sizeof(*n))||!fs15_scene_xml_apply(&n->node,attrs,count,0,1,0,"",1,&s->common)||!s->link(s->common.user,*current,&n->node,UINT32_MAX))return 0;
    *current=&n->node;return 1;
}
extern "C" int fs15_scene_camera_script_create(const void* frame,size_t bytes,size_t name_bytes,const FS15SceneLifecycleServices* s,FS15SceneCameraRecord** output) {
    if(!frame||bytes<0x34||!output||!memory(s))return 0;
    *output=nullptr;
    const auto* p=static_cast<const uint8_t*>(frame);const char* name=nullptr;std::memcpy(&name,p,sizeof(name));
    const float fov=scalar(p+0x10),near_clip=scalar(p+0x20),far_clip=scalar(p+0x30);const float parameters[]={fov,near_clip,far_clip};
    if(!finite(parameters,3))return 0;
    auto* n=static_cast<FS15SceneCameraRecord*>(s->allocate(s->user,sizeof(FS15SceneCameraRecord)));if(!n)return 0;
    if(!fs15_scene_camera_construct(n,sizeof(*n),name,name_bytes,fov,near_clip,far_clip,0,1,s)){s->deallocate(s->user,n);return 0;}*output=n;return 1;
}
extern "C" int fs15_scene_normalize3(float* vector,size_t count) {
    if(count<3||!finite(vector,3))return 0;
    const float sum=(vector[0]*vector[0]+vector[1]*vector[1])+vector[2]*vector[2];if(!std::isfinite(sum))return 0;
    const float inv=fs15_scene_math_detail::inverse_length(sum);
    float out[3];for(unsigned i=0;i<3;++i)out[i]=vector[i]*inv;
    if(!finite(out,3))return 0;
    std::memcpy(vector,out,12);return 1;
}
extern "C" int fs15_scene_camera_pose(FS15SceneCameraRecord* n,const float* matrix,size_t count,const FS15SceneCameraServices* s) {
    if(!camera(n)||count<16||!finite(matrix,16)||!services(s))return 0;
    float relative[16];std::memcpy(relative,matrix,64);float rotation[3];
    if(n->node.parent) {
        float inverse[16],parent[16],local[16];identity(inverse);identity(local);
        if(!s->world(s->user,reinterpret_cast<FS15SceneStateRecord*>(n->node.parent),parent)||!finite(parent,16))return 0;
        fs15_affine_inverse(parent,16,inverse,16); // Native ignores singular failure.
        if(!fs15_affine_multiply(inverse,16,relative,16,local,16))return 0;
        std::memcpy(relative,local,64);
    }
    if(!fs15_matrix_euler(relative,16,nullptr,rotation,3))return 0;
    if((n->node.flags&0x80u)&&!s->refresh(s->user,&n->node))return 0;
    std::memcpy(n->node.rotation,rotation,12);if(!s->rebuild(s->user,&n->node))return 0;
    if((n->node.flags&0x80u)&&!s->refresh(s->user,&n->node))return 0;
    std::memcpy(n->node.translation,relative+12,12);return s->rebuild(s->user,&n->node)!=0;
}
extern "C" int fs15_scene_camera_follow(FS15SceneCameraRecord* n,const float* target,size_t count,float distance,const FS15SceneCameraServices* s) {
    if(!camera(n)||count<3||!finite(target,3)||!std::isfinite(distance)||!services(s))return 0;
    float world[16],axis[3];if(!s->world(s->user,&n->node,world)||!finite(world,16))return 0;
    std::memcpy(axis,world+8,12);if(!fs15_scene_normalize3(axis,3))return 0;
    for(unsigned i=0;i<3;++i)world[12+i]=axis[i]*distance+target[i];
    return fs15_scene_camera_pose(n,world,16,s);
}
extern "C" int fs15_scene_camera_smooth(FS15SceneCameraRecord* n,const float* target,size_t count,float distance,float alpha,float factor,const FS15SceneCameraServices* s) {
    const float parameters[]={distance,alpha,factor};if(!camera(n)||count<3||!finite(target,3)||!finite(parameters,3)||!services(s))return 0;
    float world[16],point[3],delta[3];if(!s->world(s->user,&n->node,world)||!finite(world,16))return 0;
    for(unsigned i=0;i<3;++i){point[i]=((world[4+i]*0.0f+world[i]*0.0f)+world[8+i]*0.0f)+world[12+i];delta[i]=point[i]-target[i];}
    const float squared=(delta[1]*delta[1]+delta[0]*delta[0])+delta[2]*delta[2];if(!std::isfinite(squared))return 0;
    // Constants verified against the pinned native image by the differential suite.
    if(squared>0.001f){const float reciprocal=1.0f/squared;alpha=((squared*factor+reciprocal*0.00065f)*reciprocal)*alpha;for(unsigned i=0;i<3;++i)point[i]=point[i]-delta[i]*alpha;}
    return fs15_scene_camera_follow(n,point,3,distance,s);
}
extern "C" int fs15_scene_camera_script_follow(FS15SceneCameraRecord* n,const void* frame,size_t bytes,const FS15SceneCameraServices* s) {
    if(!n)return 1;
    if(!frame||bytes<0x54)return 0;
    const auto* p=static_cast<const uint8_t*>(frame);const float target[]={scalar(p),scalar(p+0x10),scalar(p+0x20)};
    return fs15_scene_camera_smooth(n,target,3,scalar(p+0x30),scalar(p+0x40),scalar(p+0x50),s);
}
