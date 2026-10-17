// Recovered native frustum predicates and spatial volume/point traversal.
#include "scene_visibility.h"
#include "scene_rsqrt.h"
#include <cmath>
#include <cstring>
namespace {
bool finite(const float* p,size_t n){if(!p)return false;for(size_t i=0;i<n;++i)if(!std::isfinite(p[i]))return false;return true;}
struct Planes {const float (*data)[4]=nullptr;uint32_t count=0;bool dynamic=false;};
bool valid(const Planes& p){if(p.count>65535||(!p.data&&p.count))return false;for(uint32_t i=0;i<p.count;++i)if(!finite(p.data[i],4))return false;return true;}
Planes planes(const FS15SceneFrustum* p){return {p?p->planes:nullptr,6,false};}
Planes planes(const FS15SceneClipPlanes* p){return {p?p->planes:nullptr,p?p->count:0,true};}
bool volume(const void* p,Planes& out){
    if(!p)return false;
    uintptr_t table;std::memcpy(&table,p,sizeof(table));
    if(table==0x1404c7cc0)out=planes(static_cast<const FS15SceneFrustum*>(p));
    else if(table==0x1404c7da8)out=planes(static_cast<const FS15SceneClipPlanes*>(p));
    else return false;
    return valid(out);
}
float dot(const float* p,const float* x,bool dynamic){
    const float xy=dynamic?p[1]*x[1]+p[0]*x[0]:p[0]*x[0]+p[1]*x[1];return (xy+p[2]*x[2])+p[3];
}
bool point(const Planes& p,const float* x){for(uint32_t i=0;i<p.count;++i)if(dot(p.data[i],x,p.dynamic)<=0)return false;return true;}
bool sphere(const Planes& p,const float* s){for(uint32_t i=0;i<p.count;++i)if(dot(p.data[i],s,p.dynamic)<=-s[3])return false;return true;}
bool box(const Planes& p,const float* center,const float* extent){
    for(uint32_t i=0;i<p.count;++i){const float* v=p.data[i];const float xy=p.dynamic?std::fabs(v[0])*extent[0]+std::fabs(v[1])*extent[1]:std::fabs(v[1])*extent[1]+std::fabs(v[0])*extent[0];const float support=xy+std::fabs(v[2])*extent[2];
        // Both native box bodies group the position dot X/Y first.
        const float position=((v[0]*center[0]+v[1]*center[1])+v[2]*center[2])+v[3];if(support+position<=0)return false;}
    return true;
}
bool lod(FS15SceneLodManager* m,uint32_t token,bool& selected){int out=1;if(token!=0xffffffffu&&!fs15_scene_lod_test(m,token,&out))return false;selected=out!=0;return true;}
bool row(const FS15SceneSpatialEntry& e){return finite(e.center,3)&&std::isfinite(e.radius)&&finite(e.clip,2);}
bool visible(const Planes& p,const float* viewer,float factor,const FS15SceneSpatialEntry& e,FS15SceneLodManager* m,bool& selected){
    if(!row(e)||!lod(m,e.lod_token,selected))return false;
    if(!selected)return true;
    const float x=viewer[0]-e.center[0],y=viewer[1]-e.center[1],z=viewer[2]-e.center[2];const float distance=std::sqrt(z*z+(x*x+y*y));
    if(factor*e.clip[0]<=distance-e.radius||e.radius+distance<factor*e.clip[1]){selected=false;return true;}
    float s[4];std::memcpy(s,e.center,12);s[3]=e.radius;selected=sphere(p,s);return true;
}
bool grid(const FS15SceneSpatialGrid* g){return g&&g->count<=21845&&(!g->count||g->cells)&&std::isfinite(g->min_y)&&std::isfinite(g->max_y);}
bool cell(const FS15SceneSpatialCell& c){return c.count<=c.capacity&&c.capacity<=65504&&(!c.capacity||c.entries)&&std::isfinite(c.center_x)&&std::isfinite(c.center_z)&&std::isfinite(c.half_x)&&std::isfinite(c.half_z);}
bool skip(const FS15SceneSpatialGrid& g,uint32_t index,uint32_t target){return target>index&&target<=g.count;}
int predicate(const Planes& p,const float* x,const float* extent,unsigned kind,int* result){
    if(!result||!valid(p)||!finite(x,kind==1?4:3)||(kind==2&&!finite(extent,3)))return 0;
    *result=kind==0?point(p,x):kind==1?sphere(p,x):box(p,x,extent);return 1;
}
}
extern "C" int fs15_scene_plane_normalize(float* p,size_t count){
    if(count<4||!finite(p,4))return 0;
    const float squared=(p[0]*p[0]+p[1]*p[1])+p[2]*p[2];if(!std::isfinite(squared))return 0;
    const float inv=fs15_scene_math_detail::inverse_length(squared);float out[4];for(unsigned i=0;i<4;++i)out[i]=p[i]*inv;if(!finite(out,4))return 0;std::memcpy(p,out,16);return 1;
}
extern "C" int fs15_scene_frustum_matrix(FS15SceneFrustum* out,const float* matrix,size_t count){
    if(!out||count<16||!finite(matrix,16))return 0;
    FS15SceneFrustum staged{};staged.vtable=out->vtable;const unsigned rows[]={0,0,1,1,2,2};const bool plus[]={false,true,true,false,false,true};
    for(unsigned i=0;i<6;++i){for(unsigned j=0;j<4;++j)staged.planes[i][j]=plus[i]?matrix[rows[i]+4*j]+matrix[3+4*j]:matrix[3+4*j]-matrix[rows[i]+4*j];if(!fs15_scene_plane_normalize(staged.planes[i],4))return 0;}
    *out=staged;return 1;
}
extern "C" int fs15_scene_frustum_point(const FS15SceneFrustum* p,const float* x,int* out){return p?predicate(planes(p),x,nullptr,0,out):0;}
extern "C" int fs15_scene_frustum_sphere(const FS15SceneFrustum* p,const float* x,int* out){return p?predicate(planes(p),x,nullptr,1,out):0;}
extern "C" int fs15_scene_frustum_box(const FS15SceneFrustum* p,const float* x,const float* e,int* out){return p?predicate(planes(p),x,e,2,out):0;}
extern "C" int fs15_scene_clip_point(const FS15SceneClipPlanes* p,const float* x,int* out){return p?predicate(planes(p),x,nullptr,0,out):0;}
extern "C" int fs15_scene_clip_sphere(const FS15SceneClipPlanes* p,const float* x,int* out){return p?predicate(planes(p),x,nullptr,1,out):0;}
extern "C" int fs15_scene_clip_box(const FS15SceneClipPlanes* p,const float* x,const float* e,int* out){return p?predicate(planes(p),x,e,2,out):0;}
extern "C" int fs15_scene_spatial_entry_visible(const void* v,const float* viewer,float factor,const FS15SceneSpatialEntry* e,FS15SceneLodManager* m,int* out){
    Planes p;if(!out||!e||!volume(v,p)||!finite(viewer,3)||!std::isfinite(factor))return 0;bool selected=false;if(!visible(p,viewer,factor,*e,m,selected))return 0;*out=selected;return 1;
}
extern "C" int fs15_scene_spatial_volume_query(const FS15SceneSpatialGrid* g,const void* v,const float* viewer,float factor,FS15SceneLodManager* m,uint32_t** out,uint32_t capacity,uint32_t* total){
    Planes p;if(!grid(g)||!volume(v,p)||!finite(viewer,3)||!std::isfinite(factor)||!total||(capacity&&!out))return 0;*total=0;
    const float y=(g->min_y+g->max_y)*0.5f,half_y=(g->max_y-g->min_y)*0.5f;if(!std::isfinite(y)||!std::isfinite(half_y))return 0;
    for(uint32_t i=0;i<g->count;){const auto& c=g->cells[i];if(!cell(c))return 0;const float center[]={c.center_x,y,c.center_z},extent[]={c.half_x,half_y,c.half_z};
        if(!box(p,center,extent)){if(!skip(*g,i,c.skip))return 0;i=c.skip;continue;}
        for(uint32_t j=0;j<c.count;++j){bool selected=false;if(!visible(p,viewer,factor,c.entries[j],m,selected))return 0;if(selected){if(*total<capacity)out[*total]=c.entries[j].token;++*total;}}
        ++i;
    }return 1;
}
extern "C" int fs15_scene_spatial_point_query(const FS15SceneSpatialGrid* g,const float* viewer,FS15SceneLodManager* m,uint32_t** out,uint32_t capacity,uint32_t* total){
    if(!grid(g)||!finite(viewer,3)||!total||(capacity&&!out))return 0;
    *total=0;
    for(uint32_t i=0;i<g->count;){const auto& c=g->cells[i];if(!cell(c))return 0;
        if(viewer[0]<c.center_x-c.half_x||c.half_x+c.center_x<viewer[0]||viewer[2]<c.center_z-c.half_z||c.half_z+c.center_z<viewer[2]){if(!skip(*g,i,c.skip))return 0;i=c.skip;continue;}
        for(uint32_t j=0;j<c.count;++j){const auto& e=c.entries[j];bool selected=false;if(!row(e)||!lod(m,e.lod_token,selected))return 0;if(!selected)continue;const float x=e.center[0]-viewer[0],y=e.center[1]-viewer[1],z=e.center[2]-viewer[2];if(z*z+(x*x+y*y)<=e.radius*e.radius){if(*total<capacity)out[*total]=e.token;++*total;}}
        ++i;
    }return 1;
}
