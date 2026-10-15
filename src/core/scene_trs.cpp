// GE6 0x14009de00, 0x14009d8c0, 0x140057980; platform libm is explicit.
#include "scene_trs.h"
#include <cmath>
#include <cstring>

namespace {
bool finite(const float* a,size_t n) {
    if(!a)return false;
    for(size_t i=0;i<n;++i)if(!std::isfinite(a[i]))return false;
    return true;
}
bool commit(const float* a,size_t n,float* out) {
    if(!finite(a,n))return false;
    std::memcpy(out,a,n*sizeof(float));return true;
}
void rotate(float* m,float angle,unsigned axis) {
    const unsigned first[]={4,8,0},second[]={8,0,4};
    const unsigned a=first[axis],b=second[axis];
    const float sine=std::sin(angle),cosine=std::cos(angle);
    for(unsigned i=0;i<4;++i) {
        const float x=m[a+i],y=m[b+i];
        m[a+i]=x*cosine+y*sine;m[b+i]=y*cosine-x*sine;
    }
}
}
extern "C" FS15_CORE_API int fs15_rotate_affine_axis(const float* matrix,size_t mn,
    float angle,unsigned axis,float* out,size_t count) {
    if(mn<16||count<16||!out||axis>2||!std::isfinite(angle)||!finite(matrix,16))return 0;
    float m[16];std::memcpy(m,matrix,sizeof(m));rotate(m,angle,axis);
    return commit(m,16,out);
}
extern "C" FS15_CORE_API int fs15_compose_trs(const float* rotation,const float* scale,
    const float* translation,float* out,size_t count) {
    if(!out||count<16||!finite(rotation,3)||!finite(scale,3)||!finite(translation,3))return 0;
    float m[]={1,0,0,0,0,1,0,0,0,0,1,0,translation[0],translation[1],translation[2],1};
    rotate(m,rotation[2],2);rotate(m,rotation[1],1);rotate(m,rotation[0],0);
    for(unsigned column=0;column<3;++column)for(unsigned row=0;row<3;++row)m[column*4+row]*=scale[column];
    return commit(m,16,out);
}
extern "C" FS15_CORE_API int fs15_matrix_euler(const float* m,size_t mn,const float* scale,
    float* out,size_t count) {
    if(mn<16||count<3||!out||!finite(m,16)||(scale&&!finite(scale,3)))return 0;
    float reciprocal[]={1,1,1};
    if(scale)for(unsigned i=0;i<3;++i)if(scale[i]!=0)reciprocal[i]=1.0f/scale[i];
    const float sine_y=-(reciprocal[0]*m[2]);
    const float cosine_squared=1.0f-sine_y*sine_y;
    float cosine_y=0,z_numerator=0,z_denominator=1,x;
    if(cosine_squared<=1.0e-6f)x=std::atan2(-(reciprocal[2]*m[9]),reciprocal[1]*m[5]);
    else {
        cosine_y=std::sqrt(cosine_squared);
        x=std::atan2((reciprocal[1]*m[6])/cosine_y,(reciprocal[2]*m[10])/cosine_y);
        z_denominator=(reciprocal[0]*m[0])/cosine_y;
        z_numerator=(reciprocal[0]*m[1])/cosine_y;
    }
    const float result[]={x,std::atan2(sine_y,cosine_y),std::atan2(z_numerator,z_denominator)};
    return commit(result,3,out);
}
