#include "physics_dispatch.h"
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
namespace {
void check(bool b){if(!b)throw std::runtime_error("Physics dispatch fixture failed");}
struct Backend {
    FS15PhysicsDispatchState state{};uint8_t stop=0;unsigned waits=0,steps=0,collections=0;std::vector<int> trace;
    bool fail_wait=false,fail_signal=false,fail_step=false;
    FS15PhysicsDispatchServices services{this,simulate,world,collect,wait,signal};
    Backend(){state.scene=19;state.active=1;state.iterations=2;state.fixed_step=0.01f;state.request_event=31;state.completion_event=32;}
    static int simulate(void* p,uintptr_t scene,float seconds,uint32_t iterations,float step){auto& b=*static_cast<Backend*>(p);check(scene==19&&iterations==2&&step==0.01f&&seconds>0);b.trace.push_back(2);++b.steps;return !b.fail_step;}
    static uintptr_t world(void* p,uintptr_t scene){auto& b=*static_cast<Backend*>(p);check(scene==19);b.trace.push_back(3);return 20;}
    static int collect(void* p,void* observations,uintptr_t world_token){auto& b=*static_cast<Backend*>(p);check(observations==reinterpret_cast<uint8_t*>(&b.state)+0x48&&world_token==20);b.trace.push_back(4);++b.collections;return 1;}
    static uint32_t wait(void* p,uintptr_t token,uint32_t timeout,uint8_t alertable){auto& b=*static_cast<Backend*>(p);check(token==31&&timeout==UINT32_MAX&&!alertable);b.trace.push_back(1);++b.waits;if(b.fail_wait)return UINT32_MAX;if(b.waits>3)b.stop=1;else b.state.pending_seconds=0.016f;return 0;}
    static int signal(void* p,uintptr_t token){auto& b=*static_cast<Backend*>(p);if(token==32){check(b.state.pending_seconds==0);b.trace.push_back(5);}else {check(token==31&&b.state.pending_seconds>0);b.trace.push_back(6);}return !b.fail_signal;}
};
}
int main(){try{
    Backend b;check(fs15_physics_dispatch(&b.state,sizeof(b.state),50,&b.services));
    check(b.state.busy&&b.state.elapsed_ms==50&&b.state.clamped_ms==20&&b.trace==std::vector<int>({2,3,4}));
    const auto saved=b.state;check(fs15_physics_dispatch(&b.state,sizeof(b.state),5,&b.services));check(std::memcmp(&saved,&b.state,sizeof(saved))==0);
    b.state.busy=0;b.state.asynchronous=1;b.trace.clear();check(fs15_physics_dispatch(&b.state,sizeof(b.state),10,&b.services));
    check(b.state.pending_seconds==10*0.001f&&b.trace==std::vector<int>({6}));
    b.trace.clear();check(fs15_physics_worker(&b.state,sizeof(b.state),&b.stop,&b.services));
    check(b.waits==4&&b.steps==4&&b.collections==4&&b.state.pending_seconds==0);
    check(b.trace==std::vector<int>({1,2,3,4,5,1,2,3,4,5,1,2,3,4,5,1}));
    Backend failure;failure.fail_wait=true;check(!fs15_physics_worker(&failure.state,sizeof(failure.state),&failure.stop,&failure.services)&&failure.steps==0);
    Backend step_failure;step_failure.fail_step=true;check(!fs15_physics_worker(&step_failure.state,sizeof(step_failure.state),&step_failure.stop,&step_failure.services)&&step_failure.collections==0&&step_failure.state.pending_seconds==0.016f);
    Backend signal_failure;signal_failure.fail_signal=true;check(!fs15_physics_worker(&signal_failure.state,sizeof(signal_failure.state),&signal_failure.stop,&signal_failure.services)&&signal_failure.collections==1&&signal_failure.state.pending_seconds==0);
    Backend ignored;for(float delta:{0.f,-1.f})check(fs15_physics_dispatch(&ignored.state,sizeof(ignored.state),delta,&ignored.services)&&ignored.steps==0&&!ignored.state.busy);
    ignored.state.active=0;check(fs15_physics_dispatch(&ignored.state,sizeof(ignored.state),10,&ignored.services)&&ignored.steps==0);
    check(!fs15_physics_dispatch(&ignored.state,sizeof(ignored.state),std::numeric_limits<float>::infinity(),&ignored.services));
    check(!fs15_physics_step(&ignored.state,sizeof(ignored.state)-1,0.01f,&ignored.services));
    std::cout<<"Recovered synchronous/asynchronous physics dispatch and worker fixtures passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
