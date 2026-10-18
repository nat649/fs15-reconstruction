#include "physics_worker.h"
#include <chrono>
#include <cmath>
#include <stdexcept>
namespace fs15::platform {
void PhysicsWorker::Event::signal(){std::lock_guard<std::mutex> lock(mutex);signaled=true;condition.notify_one();}
bool PhysicsWorker::Event::wait(bool bounded){
    std::unique_lock<std::mutex> lock(mutex);
    // Idle request waits retain native INFINITE behavior. Only a submitting
    // caller's completion wait has an authored failure watchdog.
    if(bounded){if(!condition.wait_for(lock,std::chrono::seconds(10),[this]{return signaled;}))return false;}
    else condition.wait(lock,[this]{return signaled;});
    signaled=false;return true;
}
PhysicsWorker::PhysicsWorker(uintptr_t scene,float step,uint32_t iterations,const FS15PhysicsDispatchServices& backend):backend_(backend),services_{this,simulate,world,collect,wait,signal}{
    if(!backend.simulate||!backend.world||!backend.collect||!std::isfinite(step)||step<=0||!iterations)throw std::invalid_argument("Invalid physics worker backend/settings");
    state_.scene=scene;state_.fixed_step=step;state_.iterations=iterations;state_.active=1;state_.asynchronous=1;
    state_.request_event=1;state_.completion_event=2;
    thread_=std::thread([this]{result_=fs15_physics_worker(&state_,sizeof(state_),&stop_,&services_);completion_.signal();});
}
PhysicsWorker::~PhysicsWorker(){stop();}
int PhysicsWorker::simulate(void* p,uintptr_t scene,float seconds,uint32_t iterations,float step){auto& self=*static_cast<PhysicsWorker*>(p);return self.backend_.simulate(self.backend_.user,scene,seconds,iterations,step);}
uintptr_t PhysicsWorker::world(void* p,uintptr_t scene){auto& self=*static_cast<PhysicsWorker*>(p);return self.backend_.world(self.backend_.user,scene);}
int PhysicsWorker::collect(void* p,void* observations,uintptr_t world_token){auto& self=*static_cast<PhysicsWorker*>(p);return self.backend_.collect(self.backend_.user,observations,world_token);}
uint32_t PhysicsWorker::wait(void* p,uintptr_t token,uint32_t timeout,uint8_t alertable){auto& self=*static_cast<PhysicsWorker*>(p);return token==1&&timeout==UINT32_MAX&&!alertable&&self.request_.wait(false)?0:UINT32_MAX;}
int PhysicsWorker::signal(void* p,uintptr_t token){auto& self=*static_cast<PhysicsWorker*>(p);if(token==1)self.request_.signal();else if(token==2)self.completion_.signal();else return 0;return 1;}
bool PhysicsWorker::tick(float milliseconds){
    std::lock_guard<std::mutex> lock(control_);
    if(stop_||result_!=-1||!std::isfinite(milliseconds))return false;
    if(!fs15_physics_dispatch(&state_,sizeof(state_),milliseconds,&services_))return false;
    if(!state_.busy)return true;
    if(!completion_.wait()||result_!=-1)return false;
    // Authored acknowledgement only; no claim of native actor/contact flush.
    state_.busy=0;++completed_;return true;
}
bool PhysicsWorker::stop(){
    std::lock_guard<std::mutex> lock(control_);
    if(!thread_.joinable())return result_==1;
    stop_=1;request_.signal();thread_.join();return result_==1;
}
}
