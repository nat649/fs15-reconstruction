// Reconstructed from FS15 1.4.2.0 x64 0x1400bfec0; not a scene graph.
#include "transform.h"
#include <cstring>

extern "C" FS15_CORE_API int fs15_transform_point(const void* object,size_t bytes,
    const float* point,float* result,size_t count) {
    if(!object||bytes<0xa0||!point||!result||count<4)return 0;
    float rows[16];
    std::memcpy(rows,static_cast<const unsigned char*>(object)+0x60,sizeof(rows));
    const float x=point[0],y=point[1],z=point[2];
    // Preserve the native evaluation order and separate binary32 operations.
    const float out[4]={
        ((y*rows[1]+x*rows[0])+z*rows[2])+rows[12],
        ((y*rows[5]+x*rows[4])+z*rows[6])+rows[13],
        ((y*rows[9]+x*rows[8])+z*rows[10])+rows[14],0};
    std::memcpy(result,out,sizeof(out));
    return 1;
}
