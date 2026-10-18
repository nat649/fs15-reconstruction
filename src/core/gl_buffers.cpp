#include "gl_buffers.h"
#include <cstring>
namespace {
constexpr uint32_t vertex=0x8892,index=0x8893;
uint32_t usage(uint32_t flags){return (flags&4)?0x88e8:(((~flags&8)|0x111c0)>>1);}
bool create_ok(const FS15GLBufferServices* s){return s&&s->gen&&s->bind&&s->data;}
bool map_ok(const FS15GLBufferServices* s){return s&&s->bind&&s->data&&s->map;}
template<class T>T get(const void* p,size_t o){T v;std::memcpy(&v,static_cast<const uint8_t*>(p)+o,sizeof(v));return v;}
template<class T>void put(void* p,size_t o,T v){std::memcpy(static_cast<uint8_t*>(p)+o,&v,sizeof(v));}
}
extern "C" FS15GLVertexBuffer* fs15_gl_vertex_create(void* driver,size_t n,const FS15GLBufferDesc* d,FS15GLBufferServices* s){
    if(!driver||n<0x190||!d||!create_ok(s)||!s->allocate)return nullptr;
    const auto count=get<uint32_t>(driver,0x138);
    if(count>(0x188-0x140)/sizeof(uintptr_t))return nullptr;
    put<uint32_t>(driver,8,0);
    auto* b=static_cast<FS15GLVertexBuffer*>(s->allocate(s->user,sizeof(FS15GLVertexBuffer)));
    if(b){
        b->vtable=0x1404bd480;s->gen(s->user,1,&b->name);s->bind(s->user,vertex,b->name);
        b->bytes=d->bytes;b->usage=usage(d->flags);b->flags=d->flags;
        s->data(s->user,vertex,b->bytes,d->data,b->usage);b->mapped=0;b->staged=0;s->vertex_bytes+=b->bytes;
    }
    for(uint32_t i=0;i<count;++i)if(get<uintptr_t>(driver,0x140+i*8)==reinterpret_cast<uintptr_t>(b)){
        put<uint32_t>(driver,0x138,0);put<uintptr_t>(driver,0x188,0);break;
    }
    return b;
}
extern "C" int fs15_gl_index_construct(FS15GLIndexBuffer* b,const FS15GLBufferDesc* d,FS15GLBufferServices* s){
    if(!b||!d||!create_ok(s))return 0;
    b->vtable=0x1404bd458;s->gen(s->user,1,&b->name);s->bind(s->user,index,b->name);
    b->bytes=d->bytes;b->usage=usage(d->flags);b->flags=d->flags;
    s->data(s->user,index,b->bytes,d->data,b->usage);b->mapped=0;b->type=d->format?0x1405:0x1403;
    s->index_bytes+=b->bytes;return 1;
}
extern "C" int fs15_gl_vertex_destroy(FS15GLVertexBuffer* b,uint32_t flags,FS15GLBufferServices* s){
    if(!b||!s||!s->erase||((flags&1)&&!s->release))return 0;
    b->vtable=0x1404bd480;s->erase(s->user,1,&b->name);s->vertex_bytes-=b->bytes;b->vtable=0x1404ba0c8;
    if(flags&1)s->release(s->user,b);
    return 1;
}
extern "C" int fs15_gl_index_destroy(FS15GLIndexBuffer* b,uint32_t flags,FS15GLBufferServices* s){
    if(!b||!s||!s->erase||((flags&1)&&!s->release))return 0;
    b->vtable=0x1404bd458;s->erase(s->user,1,&b->name);s->index_bytes-=b->bytes;b->vtable=0x1404ba190;
    if(flags&1)s->release(s->user,b);
    return 1;
}
extern "C" uintptr_t fs15_gl_index_map(FS15GLIndexBuffer* b,uint32_t offset,int mode,FS15GLBufferServices* s){
    if(!b||!map_ok(s))return 0;
    s->bind(s->user,index,b->name);
    if(mode==1)s->data(s->user,index,b->bytes,0,b->usage);
    auto p=s->map(s->user,index,mode==2?0x88b8:0x88b9);b->mapped=1;return p+offset;
}
extern "C" uintptr_t fs15_gl_vertex_map(FS15GLVertexBuffer* b,uint32_t offset,uint32_t length,int mode,FS15GLBufferServices* s){
    if(!b||!map_ok(s))return 0;
    const bool staged=length<=0x1000000&&!s->scratch_busy&&mode!=2;
    if(staged&&(!s->scratch||s->scratch_bytes<length))return 0;
    b->mapped=1;
    if(staged){s->scratch_busy=1;b->staged=1;b->offset=offset;b->length=length;b->discard=mode==1;return s->scratch;}
    s->bind(s->user,vertex,b->name);
    if(mode==1)s->data(s->user,vertex,b->bytes,0,b->usage);
    return s->map(s->user,vertex,mode==2?0x88b8:0x88b9)+offset;
}
extern "C" int fs15_gl_vertex_upload(FS15GLVertexBuffer* b,uint32_t offset,uint32_t length,uint8_t discard,FS15GLBufferServices* s){
    if(!b||!s||!s->bind||!s->data||!s->subdata||!s->scratch||s->scratch_bytes<length)return 0;
    const auto source=s->scratch;
    s->bind(s->user,vertex,b->name);if(discard)s->data(s->user,vertex,b->bytes,0,b->usage);
    s->subdata(s->user,vertex,offset,length,source);return 1;
}
extern "C" int fs15_gl_vertex_unmap(FS15GLVertexBuffer* b,FS15GLBufferServices* s){
    if(!b||!s||!s->bind||!s->unmap)return 0;
    if(b->staged){
        if(!s->data||!s->subdata||!s->scratch||s->scratch_bytes<b->length)return 0;
        b->staged=0;s->scratch_busy=0;
        if(!fs15_gl_vertex_upload(b,b->offset,b->length,b->discard,s))return 0;
    }else{s->bind(s->user,vertex,b->name);s->unmap(s->user,vertex);}
    b->mapped=0;return 1;
}
extern "C" int fs15_gl_index_unmap(FS15GLIndexBuffer* b,FS15GLBufferServices* s){
    if(!b||!s||!s->bind||!s->unmap)return 0;
    s->bind(s->user,index,b->name);s->unmap(s->user,index);b->mapped=0;return 1;
}
