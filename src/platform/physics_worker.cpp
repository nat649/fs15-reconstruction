#include "physics_worker.h"
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <cstdlib>
#include <cstring>
#include <algorithm>
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
PhysicsWorker::PhysicsWorker(uintptr_t scene,float step,uint32_t iterations,const FS15PhysicsDispatchServices& backend,const PostBackend* post):backend_(backend),services_{this,simulate,world,collect,wait,signal}{
    if(!backend.simulate||!backend.world||!backend.collect||!std::isfinite(step)||step<=0||!iterations)throw std::invalid_argument("Invalid physics worker backend/settings");
    state_.scene=scene;state_.fixed_step=step;state_.iterations=iterations;state_.active=1;state_.asynchronous=1;
    state_.request_event=1;state_.completion_event=2;
    for(auto* head:{&empty_objects_,&empty_wheels_}){head->left=head->right=head->parent=head;head->nil=1;}
    auto* object_head=&empty_objects_;auto* wheel_head=&empty_wheels_;
    std::memcpy(reinterpret_cast<uint8_t*>(&state_)+0x3a8,&object_head,sizeof(object_head));
    std::memcpy(reinterpret_cast<uint8_t*>(&state_)+0x250,&wheel_head,sizeof(wheel_head));
    if(post)post_backend_=post->services;
    if(bool(post_backend_.allocate)!=bool(post_backend_.release))throw std::invalid_argument("Physics post-step allocator must have matching release");
    post_services_={this,wait,object,frame,arctangent,allocate,release,observer,pending,cancel,post_backend_.part_limit,nullptr,nullptr,post_backend_.contact_row_limit};
    if(post&&post->configure&&!post->configure(post_backend_.user,&state_))throw std::invalid_argument("Physics post-step configuration failed");
    uintptr_t listener_begin=0,listener_end=0;
    std::memcpy(&listener_begin,reinterpret_cast<uint8_t*>(&state_)+0x198,sizeof(listener_begin));
    std::memcpy(&listener_end,reinterpret_cast<uint8_t*>(&state_)+0x1a0,sizeof(listener_end));
    if(listener_begin!=listener_end&&!post_backend_.observer)throw std::invalid_argument("Physics contact listeners require an observer service");
    thread_=std::thread([this]{result_=fs15_physics_worker(&state_,sizeof(state_),&stop_,&services_);completion_.signal();});
}
PhysicsWorker::~PhysicsWorker(){stop();while(!post_allocations_.empty())release(this,post_allocations_.back());}
int PhysicsWorker::simulate(void* p,uintptr_t scene,float seconds,uint32_t iterations,float step){auto& self=*static_cast<PhysicsWorker*>(p);return self.backend_.simulate(self.backend_.user,scene,seconds,iterations,step);}
uintptr_t PhysicsWorker::world(void* p,uintptr_t scene){auto& self=*static_cast<PhysicsWorker*>(p);return self.backend_.world(self.backend_.user,scene);}
int PhysicsWorker::collect(void* p,void* observations,uintptr_t world_token){auto& self=*static_cast<PhysicsWorker*>(p);return self.backend_.collect(self.backend_.user,observations,world_token);}
uint32_t PhysicsWorker::wait(void* p,uintptr_t token,uint32_t timeout,uint8_t alertable){auto& self=*static_cast<PhysicsWorker*>(p);if(timeout!=UINT32_MAX||alertable)return UINT32_MAX;if(token==1)return self.request_.wait(false)?0:UINT32_MAX;if(token==2)return self.completion_.wait()&&self.result_==-1?0:UINT32_MAX;return UINT32_MAX;}
int PhysicsWorker::signal(void* p,uintptr_t token){auto& self=*static_cast<PhysicsWorker*>(p);if(token==1)self.request_.signal();else if(token==2)self.completion_.signal();else return 0;return 1;}
bool PhysicsWorker::tick(float milliseconds){
    std::lock_guard<std::mutex> lock(control_);
    if(stop_||failed_||result_!=-1||!std::isfinite(milliseconds))return false;
    if(!fs15_physics_dispatch(&state_,sizeof(state_),milliseconds,&services_))return false;
    if(!state_.busy)return true;
    if(!fs15_physics_post_step(&state_,sizeof(state_),&post_services_)){failed_=true;return false;}
    ++completed_;return true;
}
bool PhysicsWorker::stop(){
    std::lock_guard<std::mutex> lock(control_);
    if(!thread_.joinable())return result_==1;
    stop_=1;request_.signal();thread_.join();return result_==1;
}
int PhysicsWorker::object(void* p,uintptr_t token){auto& self=*static_cast<PhysicsWorker*>(p);return self.post_backend_.object&&self.post_backend_.object(self.post_backend_.user,token);}
int PhysicsWorker::frame(void* p,const void* set,int32_t index,float* matrix){auto& self=*static_cast<PhysicsWorker*>(p);return self.post_backend_.frame&&self.post_backend_.frame(self.post_backend_.user,set,index,matrix);}
float PhysicsWorker::arctangent(void* p,float value){auto& self=*static_cast<PhysicsWorker*>(p);return self.post_backend_.arctangent?self.post_backend_.arctangent(self.post_backend_.user,value):std::atan(value);}
void* PhysicsWorker::allocate(void* p,size_t bytes,size_t alignment){auto& self=*static_cast<PhysicsWorker*>(p);void* value=self.post_backend_.allocate?self.post_backend_.allocate(self.post_backend_.user,bytes,alignment):std::malloc(bytes);if(value){try{self.post_allocations_.push_back(value);}catch(...){if(self.post_backend_.release)self.post_backend_.release(self.post_backend_.user,value);else std::free(value);return nullptr;}}return value;}
void PhysicsWorker::release(void* p,void* value){auto& self=*static_cast<PhysicsWorker*>(p);auto at=std::find(self.post_allocations_.begin(),self.post_allocations_.end(),value);if(at!=self.post_allocations_.end())self.post_allocations_.erase(at);if(self.post_backend_.release)self.post_backend_.release(self.post_backend_.user,value);else std::free(value);}
int PhysicsWorker::observer(void* p,uintptr_t listener,uintptr_t a,uintptr_t b,uintptr_t target,uint32_t event,uint32_t part){auto& self=*static_cast<PhysicsWorker*>(p);return self.post_backend_.observer?self.post_backend_.observer(self.post_backend_.user,listener,a,b,target,event,part):0;}
int PhysicsWorker::pending(void* p,uintptr_t callback,uint32_t owner,float item,uint8_t* should_cancel){auto& self=*static_cast<PhysicsWorker*>(p);return self.post_backend_.pending&&self.post_backend_.pending(self.post_backend_.user,callback,owner,item,should_cancel);}
int PhysicsWorker::cancel(void* p,void* manager,uint32_t id){auto& self=*static_cast<PhysicsWorker*>(p);return self.post_backend_.cancel&&self.post_backend_.cancel(self.post_backend_.user,manager,id);}
}
