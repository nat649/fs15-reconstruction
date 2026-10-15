// Bounded reconstructions from the pinned GE6 6.0.2 x64 analysis image.
#include "scene_math.h"
#include <cmath>
#include <cstring>

namespace {
bool finite(const float* values,size_t count) {
    if(!values)return false;
    for(size_t i=0;i<count;++i)if(!std::isfinite(values[i]))return false;
    return true;
}
bool matrix(const float* values,size_t count){return count>=16&&finite(values,16);}
bool commit(const float* values,size_t count,float* out) {
    if(!finite(values,count))return false;
    std::memcpy(out,values,count*sizeof(float));return true;
}
}

extern "C" FS15_CORE_API int fs15_affine_multiply(const float* a,size_t an,
    const float* b,size_t bn,float* result,size_t rn) {
    if(!result||rn<16||!matrix(a,an)||!matrix(b,bn))return 0;
    float out[16]{};
    // 0x14009da60: preserve the different addition order of the third row.
    for(unsigned column=0;column<4;++column) {
        const unsigned c=column*4;
        out[c]=(a[0]*b[c]+a[4]*b[c+1])+a[8]*b[c+2];
        out[c+1]=(a[1]*b[c]+a[5]*b[c+1])+a[9]*b[c+2];
        out[c+2]=(b[c+1]*a[6]+a[2]*b[c])+b[c+2]*a[10];
        if(column==3){out[c]+=a[12];out[c+1]+=a[13];out[c+2]+=a[14];}
    }
    out[15]=1;
    return commit(out,16,result);
}

extern "C" FS15_CORE_API int fs15_affine_inverse(const float* m,size_t mn,
    float* result,size_t rn) {
    if(!result||rn<16||!matrix(m,mn))return 0;
    // 0x14009e160: original determinant threshold, including its strict edge.
    const float determinant=((m[5]*m[10]-m[6]*m[9])*m[0]-
        (m[1]*m[10]-m[2]*m[9])*m[4])+(m[1]*m[6]-m[2]*m[5])*m[8];
    if(!std::isfinite(determinant)||std::fabs(determinant)<=1.0e-36f)return 0;
    const float r=1.0f/determinant;
    float out[16]{};
    out[0]=(m[10]*m[5]-m[6]*m[9])*r;
    out[1]=-((m[10]*m[1]-m[9]*m[2])*r);
    out[2]=(m[6]*m[1]-m[5]*m[2])*r;
    out[4]=-((m[10]*m[4]-m[6]*m[8])*r);
    out[5]=(m[10]*m[0]-m[8]*m[2])*r;
    out[6]=-((m[6]*m[0]-m[2]*m[4])*r);
    out[8]=(m[9]*m[4]-m[5]*m[8])*r;
    out[9]=-((m[9]*m[0]-m[1]*m[8])*r);
    out[10]=(m[5]*m[0]-m[1]*m[4])*r;
    out[12]=-((m[13]*out[4]+out[0]*m[12])+m[14]*out[8]);
    out[13]=-((out[1]*m[12]+m[13]*out[5])+m[14]*out[9]);
    out[14]=-((m[13]*out[6]+m[12]*out[2])+m[14]*out[10]);
    out[15]=1;
    return commit(out,16,result);
}

extern "C" FS15_CORE_API int fs15_rigid_inverse_point(const float* m,size_t mn,
    const float* point,float* result,size_t rn) {
    if(!result||rn<3||!matrix(m,mn)||!finite(point,3))return 0;
    // 0x14009e0a0; intentionally retain the native nonuniform-scale behavior.
    const float x=point[0]-m[12],y=point[1]-m[13],z=point[2]-m[14];
    const float out[]={(y*m[1]+x*m[0])+z*m[2],
        (x*m[4]+y*m[5])+z*m[6],(x*m[8]+y*m[9])+z*m[10]};
    return commit(out,3,result);
}

extern "C" FS15_CORE_API int fs15_quaternion_to_matrix(const float* q,size_t qn,
    float* result,size_t rn) {
    if(qn<4||!finite(q,4)||!result||rn<16)return 0;
    // 0x14009df40: nine partial writes, WXYZ rather than XYZW.
    const float w=q[0],x=q[1],y=q[2],z=q[3];
    float out[16];std::memcpy(out,result,sizeof(out));
    out[0]=1-(z*z+y*y)*2;out[1]=(y*x+w*z)*2;out[4]=(y*x-w*z)*2;
    out[2]=(z*x-w*y)*2;out[9]=(z*y-w*x)*2;out[10]=1-(y*y+x*x)*2;
    out[5]=1-(z*z+x*x)*2;out[6]=(z*y+w*x)*2;out[8]=(z*x+w*y)*2;
    const unsigned lanes[]={0,1,2,4,5,6,8,9,10};
    for(unsigned lane:lanes)if(!std::isfinite(out[lane]))return 0;
    std::memcpy(result,out,sizeof(out));return 1;
}

extern "C" FS15_CORE_API int fs15_matrix_to_quaternion(const float* m,size_t mn,
    float* result,size_t rn) {
    if(!result||rn<4||!matrix(m,mn))return 0;
    // 0x140280380: preserve trace order and strict dominant-diagonal ties.
    const float trace=m[10]+(m[0]+m[5]);
    float out[4],s,k;
    if(trace>=0) {
        s=std::sqrt(trace+1);k=0.5f/s;
        out[0]=s*0.5f;out[1]=(m[6]-m[9])*k;
        out[2]=(m[8]-m[2])*k;out[3]=(m[1]-m[4])*k;
    }else {
        unsigned axis=m[0]<m[5]?1u:0u;
        if(m[axis*5]<m[10])axis=2;
        if(axis==0) {
            s=std::sqrt((m[0]-(m[10]+m[5]))+1);k=0.5f/s;
            out[1]=s*0.5f;out[2]=(m[4]+m[1])*k;
            out[3]=(m[8]+m[2])*k;out[0]=(m[6]-m[9])*k;
        }else if(axis==1) {
            s=std::sqrt((m[5]-(m[10]+m[0]))+1);k=0.5f/s;
            out[2]=s*0.5f;out[3]=(m[9]+m[6])*k;
            out[1]=(m[4]+m[1])*k;out[0]=(m[8]-m[2])*k;
        }else {
            s=std::sqrt((m[10]-(m[0]+m[5]))+1);k=0.5f/s;
            out[3]=s*0.5f;out[1]=(m[8]+m[2])*k;
            out[2]=(m[9]+m[6])*k;out[0]=(m[1]-m[4])*k;
        }
    }
    return commit(out,4,result);
}

extern "C" FS15_CORE_API int fs15_matrix_scale(const float* m,size_t mn,
    float* result,size_t rn) {
    if(!result||rn<3||!matrix(m,mn))return 0;
    // 0x14009dcb0: snap magnitudes in the original open interval to one.
    float out[]={std::sqrt((m[1]*m[1]+m[0]*m[0])+m[2]*m[2]),
        std::sqrt((m[4]*m[4]+m[5]*m[5])+m[6]*m[6]),
        std::sqrt((m[9]*m[9]+m[8]*m[8])+m[10]*m[10])};
    for(float& scale:out)if(scale<1.0000100135803223f&&scale>0.9999899864196777f)scale=1;
    return commit(out,3,result);
}
