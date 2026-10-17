#include "scene_view.h"
#include "scene_math.h"
#include "scene_projection.h"
#include "scene_rsqrt.h"
#include <cmath>
#include <cstring>
namespace {
bool finite(const float* p,size_t n){if(!p)return false;for(size_t i=0;i<n;++i)if(!std::isfinite(p[i]))return false;return true;}
bool settings(const FS15SceneViewSettings* s){return s&&finite(s->values20,3)&&std::isfinite(s->scalar30)&&std::isfinite(s->scalar34)&&finite(s->values38,3)&&std::isfinite(s->scalar48)&&std::isfinite(s->scalar4c)&&std::isfinite(s->scalar50)&&finite(s->values54,3)&&std::isfinite(s->scalar60)&&std::isfinite(s->scalar64);}
bool buffer(const FS15SceneViewServices* s){return s&&s->map&&s->unmap;}
float negative(float f){return fs15_scene_math_detail::value(fs15_scene_math_detail::bits(f)^0x80000000u);}
float determinant(const float* m){return ((m[5]*m[10]-m[6]*m[9])*m[0]-(m[1]*m[10]-m[2]*m[9])*m[4])+(m[1]*m[6]-m[2]*m[5])*m[8];}
}
extern "C" int fs15_scene_view_upload(FS15SceneRenderView* v,size_t bytes,double milliseconds,const FS15SceneViewServices* s){
    if(!v||bytes<sizeof(*v)||!buffer(s)||!v->buffer||!settings(v->settings)||!finite(v->world,16)||!finite(v->projection,16)||!std::isfinite(milliseconds))return 0;
    const float time=static_cast<float>(milliseconds*0.001);if(!std::isfinite(time))return 0;
    float axes[3][3];for(size_t c=0;c<3;++c){std::memcpy(axes[c],v->world+c*4,12);if(!fs15_scene_normalize3(axes[c],3))return 0;}
    std::memcpy(v->position,v->world+12,12);v->position[3]=0;std::memcpy(v->basis,axes,sizeof(axes));v->volume=reinterpret_cast<uintptr_t>(v);
    size_t count=0;float* out=s->map(s->user,v->buffer,&count);if(!out)return 0;
    if(count<49){s->unmap(s->user,v->buffer);return 0;}
    for(size_t row=0;row<3;++row)for(size_t c=0;c<4;++c)out[row*4+c]=v->world[c*4+row];
    for(size_t row=0;row<4;++row)for(size_t c=0;c<4;++c)out[12+row*4+c]=v->projection[c*4+row];
    std::memcpy(out+28,v->position,16);
    const auto* p=v->settings;
    std::memcpy(out+36,p->values54,12);std::memcpy(out+32,p->values38,12);std::memcpy(out+40,p->values20,12);
    out[46]=p->scalar60;out[47]=p->scalar64;out[35]=p->scalar30;out[39]=p->scalar34;out[43]=p->scalar48;out[44]=p->scalar4c;out[45]=p->scalar50;out[48]=time;
    return s->unmap(s->user,v->buffer)?1:0;
}
extern "C" int fs15_scene_view_prepare(FS15SceneRenderView* v,size_t bytes,FS15SceneCameraRecord* c,float aspect,uint8_t stereo,uint8_t eye,double milliseconds,const FS15SceneViewServices* s){
    if(!v||bytes<sizeof(*v)||!c||c->node.vtable!=0x1404c1078||!buffer(s)||!s->world||!std::isfinite(aspect)||!settings(v->settings))return 0;
    const float params[]={c->fov,c->near_clip,c->far_clip,c->orthographic_height,c->unknown180,c->unknown184};if(!finite(params,6))return 0;
    FS15SceneRenderView a=*v;if(!s->world(s->user,&c->node,a.world)||!fs15_scene_camera_basis(a.world,16))return 0;
    float left,right,bottom,top;const float near=c->near_clip,far=c->far_clip;
    if(!c->orthographic){
        const float radians=((c->fov*0.5f)*3.1415927410125732f)/180.0f;
        const float tangent=s->tangent?s->tangent(s->user,radians):std::tan(radians);
        top=tangent*near;bottom=negative(top);left=bottom*aspect;right=negative(left);
    }else{top=c->orthographic_height;bottom=negative(top);right=top*aspect;left=negative(right);}
    if(stereo){
        float offset=c->unknown180*0.5f;if(eye)offset=negative(offset);
        for(size_t i=0;i<3;++i)a.world[12+i]=a.world[i]*offset+a.world[12+i];
        const float convergence=c->unknown184<near?near:c->unknown184;const float shift=(offset*near)/convergence;
        left=left-shift;right=right-shift;
    }
    if(!(c->orthographic?fs15_scene_orthographic(left,right,bottom,top,near,far,a.projection,16):fs15_scene_perspective(left,right,bottom,top,near,far,a.projection,16)))return 0;
    if(!fs15_affine_inverse(a.world,16,a.inverse,16)){
        const float det=determinant(a.world);if(!std::isfinite(det)||std::fabs(det)>1.0e-36f||!finite(a.inverse,16))return 0;
    }
    if(!fs15_scene_projective_multiply(a.projection,16,a.inverse,16,a.combined,16)||!fs15_scene_frustum_matrix(&a.frustum,a.combined,16))return 0;
    *v=a;if(!fs15_scene_view_upload(v,bytes,milliseconds,s))return 0;v->camera=reinterpret_cast<uintptr_t>(c);return 1;
}
