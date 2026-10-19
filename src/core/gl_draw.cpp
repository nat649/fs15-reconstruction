#include "gl_draw.h"
extern "C" int fs15_gl_draw(FS15GLDrawState* state,int32_t count,int32_t first,uint32_t primitive,uint32_t minimum,uint32_t range_count,FS15GLDrawServices* s){
    if(!state||primitive>=5)return 0;
    if(!state->layout)return 1;
    if(!s)return 0;
    constexpr uint32_t modes[]{4,5,1,3,0};const uint32_t mode=modes[primitive];
    if(!mode&&state->point_size!=5.f){if(!s->point_size)return 0;s->point_size(s->user,5.f);state->point_size=5.f;}
    if(!state->index){if(!s->arrays)return 0;s->arrays(s->user,mode,first,count);}
    else{
        const auto type=state->index->type;const uintptr_t offset=uint32_t((type==0x1403?2u:4u)*uint32_t(first));
        if(!range_count){if(!s->elements)return 0;s->elements(s->user,mode,count,type,offset);}
        else{if(!s->range)return 0;s->range(s->user,mode,minimum,minimum+range_count-1u,count,type,offset);}
    }
    return 1;
}
