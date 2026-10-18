#include "physics_dispatch.h"
#include <cmath>
namespace {
bool valid(FS15PhysicsDispatchState* p,size_t n){return p&&n>=sizeof(*p);}
bool backend(const FS15PhysicsDispatchServices* s){return s&&s->simulate&&s->world&&s->collect;}
}
extern "C" int fs15_physics_step(FS15PhysicsDispatchState* p,size_t n,float seconds,const FS15PhysicsDispatchServices* s){
    if(!valid(p,n)||!backend(s)||!std::isfinite(seconds)||!std::isfinite(p->fixed_step))return 0;
    if(!s->simulate(s->user,p->scene,seconds,p->iterations,p->fixed_step))return 0;
    const uintptr_t world=s->world(s->user,p->scene);
    return s->collect(s->user,reinterpret_cast<uint8_t*>(p)+0x48,world)?1:0;
}
extern "C" int fs15_physics_dispatch(FS15PhysicsDispatchState* p,size_t n,float milliseconds,const FS15PhysicsDispatchServices* s){
    if(!valid(p,n)||!std::isfinite(milliseconds))return 0;
    if(p->busy||milliseconds<=0||!p->active)return 1;
    if(!s||(p->asynchronous?!s->signal:!backend(s))||!std::isfinite(p->fixed_step)||!std::isfinite(p->elapsed_ms)||!std::isfinite(p->clamped_ms))return 0;
    float limit=(static_cast<float>(p->iterations)*p->fixed_step)*1000.f;
    if(milliseconds<=limit)limit=milliseconds;
    const float elapsed=milliseconds+p->elapsed_ms,clamped=limit+p->clamped_ms,seconds=milliseconds*0.001f;
    if(!std::isfinite(elapsed)||!std::isfinite(clamped)||!std::isfinite(seconds))return 0;
    p->elapsed_ms=elapsed;p->busy=1;p->clamped_ms=clamped;
    if(p->asynchronous){p->pending_seconds=seconds;return s->signal(s->user,p->request_event)?1:0;}
    return fs15_physics_step(p,n,seconds,s);
}
extern "C" int fs15_physics_worker(FS15PhysicsDispatchState* p,size_t n,const uint8_t* stop,const FS15PhysicsDispatchServices* s){
    if(!valid(p,n)||!stop||!backend(s)||!s->wait||!s->signal)return 0;
    for(;;){
        if(s->wait(s->user,p->request_event,UINT32_MAX,0)!=0)return 0;
        if(*stop)return 1;
        if(!fs15_physics_step(p,n,p->pending_seconds,s))return 0;
        p->pending_seconds=0;
        if(!s->signal(s->user,p->completion_event))return 0;
    }
}
