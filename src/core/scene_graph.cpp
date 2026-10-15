// Recovered cached world-transform path 0x1400b1a50 and four script adapters.
#include "scene_graph.h"
#include "scene_math.h"
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

namespace {
constexpr size_t record_bytes=0x118;
template<class T>T read(const unsigned char* p){T v;std::memcpy(&v,p,sizeof(v));return v;}
bool arena(void* data,size_t bytes,size_t stride,size_t index) {
    return data&&stride>=record_bytes&&bytes/stride>index&&
        reinterpret_cast<uintptr_t>(data)<=std::numeric_limits<uintptr_t>::max()-bytes;
}
bool separate(const void* a,size_t an,const void* b,size_t bn) {
    const auto x=reinterpret_cast<uintptr_t>(a),y=reinterpret_cast<uintptr_t>(b);
    return y<=std::numeric_limits<uintptr_t>::max()-bn&&(x+an<=y||y+bn<=x);
}
}
extern "C" FS15_CORE_API int fs15_scene_update_world(void* data,size_t bytes,size_t stride,size_t index) {
    if(!arena(data,bytes,stride,index))return 0;
    static_assert(sizeof(void*)==8,"Observed native node layout requires 64-bit pointers");
    auto* base=static_cast<unsigned char*>(data);
    const size_t count=bytes/stride;
    const uintptr_t start=reinterpret_cast<uintptr_t>(base);
    try {
        // Preflight the entire dirty chain before any mutation. Count bounds
        // reject cycles without dereferencing arbitrary original pointers.
        std::vector<size_t> pending;
        size_t current=index;
        while((read<uint32_t>(base+current*stride+0x50)&0x40)!=0) {
            if(pending.size()>=count)return 0;
            pending.push_back(current);
            const uintptr_t parent=read<uintptr_t>(base+current*stride+0x28);
            if(!parent)break;
            if(parent<start||parent-start>=count*stride||(parent-start)%stride)return 0;
            current=(parent-start)/stride;
        }
        // Stage all matrices as well: overflow/nonfinite failure leaves the
        // original arena unchanged, an authored guard beyond the native code.
        std::vector<float> matrices(pending.size()*16);
        float parent_matrix[16];bool have_parent=false;
        for(size_t n=pending.size();n>0;--n) {
            const auto* node=base+pending[n-1]*stride;
            float local[16];std::memcpy(local,node+0x98,sizeof(local));
            float* world=matrices.data()+(n-1)*16;
            if(read<uintptr_t>(node+0x28)) {
                if(!have_parent) {
                    const auto parent=read<uintptr_t>(node+0x28);
                    std::memcpy(parent_matrix,base+(parent-start)+0xd8,sizeof(parent_matrix));
                }
                if(!fs15_affine_multiply(parent_matrix,16,local,16,world,16))return 0;
            }else {
                for(float value:local)if(!std::isfinite(value))return 0;
                std::memcpy(world,local,sizeof(local));
            }
            std::memcpy(parent_matrix,world,sizeof(parent_matrix));have_parent=true;
        }
        for(size_t n=0;n<pending.size();++n) {
            auto* node=base+pending[n]*stride;
            std::memcpy(node+0xd8,matrices.data()+n*16,64);
            const uint32_t flags=read<uint32_t>(node+0x50)&~uint32_t(0x40);
            std::memcpy(node+0x50,&flags,4);
        }
        return 1;
    }catch(...){return 0;}
}

extern "C" FS15_CORE_API int fs15_scene_transform_frame(unsigned operation,void* data,
    size_t bytes,size_t stride,size_t index,void* frame,size_t frame_bytes) {
    if(operation>3||!frame||frame_bytes<0x11a||!arena(data,bytes,stride,index)||
        !separate(data,bytes,frame,frame_bytes))return 0;
    auto* f=static_cast<unsigned char*>(frame);
    const float x=read<float>(f),y=read<float>(f+0x10),z=read<float>(f+0x20);
    if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z))return 0;
    if(!fs15_scene_update_world(data,bytes,stride,index))return 0;
    float m[16];std::memcpy(m,static_cast<unsigned char*>(data)+index*stride+0xd8,sizeof(m));
    for(float value:m)if(!std::isfinite(value))return 0;
    float out[3];
    if(operation>=2) {
        float inv[16];if(!fs15_affine_inverse(m,16,inv,16))return 0;
        for(unsigned row=0;row<3;++row) {
            out[row]=(inv[row]*x+inv[row+4]*y)+inv[row+8]*z;
            if(operation==2)out[row]+=inv[row+12];
        }
    }else {
        out[0]=(y*m[4]+x*m[0])+z*m[8];
        out[1]=(x*m[1]+y*m[5])+z*m[9];out[2]=(x*m[2]+y*m[6])+z*m[10];
        if(operation==0)for(unsigned row=0;row<3;++row)out[row]+=m[row+12];
    }
    for(float value:out)if(!std::isfinite(value))return 0;
    const uint16_t type=3;
    for(unsigned row=0;row<3;++row) {
        std::memcpy(f+0xf0+row*16,out+row,4);
        std::memcpy(f+0xf8+row*16,&type,2);
    }
    return 1;
}
