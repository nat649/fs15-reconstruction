#include "scene_projection.h"
#include "scene_rsqrt.h"
#include <cmath>
#include <cstring>
namespace {
bool finite(const float* p,size_t n){if(!p)return false;for(size_t i=0;i<n;++i)if(!std::isfinite(p[i]))return false;return true;}
int commit(float* out,const float* p){if(!finite(p,16))return 0;std::memcpy(out,p,16*sizeof(float));return 1;}
float negative(float f){return fs15_scene_math_detail::value(fs15_scene_math_detail::bits(f)^0x80000000u);}
bool outside(float f){return f>1.00001f||f<0.99999f;}
float dot(const float* a,const float* b){return (a[0]*b[0]+a[1]*b[1])+a[2]*b[2];}
void normalize(float* a){const float inv=fs15_scene_math_detail::inverse_length(dot(a,a));if(outside(inv))for(size_t i=0;i<3;++i)a[i]=a[i]*inv;}
bool bounds(float l,float r,float b,float t,float n,float f){const float values[]={l,r,b,t,n,f};return finite(values,6)&&r!=l&&t!=b&&f!=n;}
}
extern "C" int fs15_scene_perspective(float l,float r,float b,float t,float n,float f,float* out,size_t count){
    if(!out||count<16||!bounds(l,r,b,t,n,f))return 0;
    const float x=1.0f/(r-l),y=1.0f/(t-b),twice=n*2.0f,depth=(n*f)*-2.0f;
    float a[16]{};a[0]=twice*x;a[5]=twice*y;a[8]=(l+r)*x;a[9]=(b+t)*y;a[10]=negative((n+f)/(f-n));a[11]=-1.0f;a[14]=depth/(f-n);
    return commit(out,a);
}
extern "C" int fs15_scene_orthographic(float l,float r,float b,float t,float n,float f,float* out,size_t count){
    if(!out||count<16||!bounds(l,r,b,t,n,f))return 0;
    const float x=1.0f/(r-l),y=1.0f/(t-b);
    float a[16]{};a[0]=x*2.0f;a[5]=y*2.0f;a[10]=-2.0f/(f-n);a[12]=negative((l+r)*x);a[13]=negative((b+t)*y);a[14]=negative((n+f)/(f-n));a[15]=1.0f;
    return commit(out,a);
}
extern "C" int fs15_scene_projective_multiply(const float* a,size_t na,const float* b,size_t nb,float* out,size_t no){
    if(!out||na<16||nb<16||no<16||!finite(a,16)||!finite(b,16))return 0;
    float p[16];for(size_t c=0;c<4;++c){const size_t i=4*c;
        p[i]=((a[0]*b[i]+b[i+1]*a[4])+b[i+2]*a[8])+a[12]*b[i+3];
        p[i+1]=((b[i]*a[1]+b[i+1]*a[5])+b[i+2]*a[9])+a[13]*b[i+3];
        p[i+2]=((b[i+1]*a[6]+a[2]*b[i])+b[i+2]*a[10])+a[14]*b[i+3];
        p[i+3]=((a[7]*b[i+1]+a[3]*b[i])+a[11]*b[i+2])+a[15]*b[i+3];
    }
    return commit(out,p);
}
extern "C" int fs15_scene_camera_basis(float* out,size_t count){
    if(count<16||!finite(out,16))return 0;
    float a[16];std::memcpy(a,out,sizeof(a));normalize(a);
    const float d=dot(a+4,a);if(outside(d))for(size_t i=0;i<3;++i)a[4+i]=a[4+i]-a[i]*d;
    normalize(a+4);
    const float y=dot(a+8,a+4),x=dot(a+8,a);
    if(outside(x)||outside(y))for(size_t i=0;i<3;++i)a[8+i]=a[8+i]-(a[4+i]*y+a[i]*x);
    normalize(a+8);return commit(out,a);
}
