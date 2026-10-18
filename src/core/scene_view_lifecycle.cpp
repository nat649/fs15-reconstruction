#include "scene_view_lifecycle.h"
#include <cstring>
namespace {
void identity(float* p){std::memset(p,0,64);p[0]=p[5]=p[10]=p[15]=1;}
}
extern "C" int fs15_scene_view_construct(FS15SceneRenderView* v,size_t bytes,
    const FS15SceneViewContext* c,size_t context_bytes,const FS15SceneViewLifetimeServices* s){
    if(!v||bytes<sizeof(*v)||!c||context_bytes<sizeof(*c)||!s||!s->create)return 0;
    v->frustum.vtable=0x1404c7cc0;
    // Native initializes each plane to (0,1,0,0), not a valid camera volume.
    auto* raw=reinterpret_cast<uint8_t*>(v);
    const float plane[]={0,1,0,0};for(size_t i=0;i<6;++i)std::memcpy(raw+8+i*16,plane,16);
    v->settings=&c->settings;
    identity(v->world);identity(v->inverse);identity(v->projection);
    std::memset(raw+0x168,0,0x34); // Position + basis; unknown19c is untouched.
    v->camera=0;v->volume=0;
    std::memset(raw+0x1b0,0,8);
    std::memcpy(raw+0x1b8,&c->width,8);
    std::memset(raw+0x1c0,0,8);
    const uint32_t flags=0x1000000;std::memcpy(raw+0x1c8,&flags,4);
    v->buffer=s->create(s->user,196,0,8);
    v->secondary_buffer=s->create(s->user,176,0,8);
    return 1;
}
extern "C" int fs15_scene_view_destruct(FS15SceneRenderView* v,size_t bytes,const FS15SceneViewLifetimeServices* s){
    if(!v||bytes<sizeof(*v)||!s||!s->destroy)return 0;
    if(v->buffer)s->destroy(s->user,v->buffer,1);
    if(v->secondary_buffer)s->destroy(s->user,v->secondary_buffer,1);
    v->frustum.vtable=0x1404c1010;return 1;
}
