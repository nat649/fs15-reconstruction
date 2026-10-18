#include "physics_worker.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>
namespace {
void check(bool b){if(!b)throw std::runtime_error("Physics event/thread fixture failed");}
struct Backend {
    std::thread::id main=std::this_thread::get_id(),worker{};unsigned calls=0,collected=0;bool fail=false;
    FS15PhysicsDispatchServices services{this,simulate,world,collect,nullptr,nullptr};
    static int simulate(void* p,uintptr_t scene,float seconds,uint32_t iterations,float step){auto& b=*static_cast<Backend*>(p);b.worker=std::this_thread::get_id();check(b.worker!=b.main&&scene==9&&seconds>0&&iterations==4&&step==0.01f);++b.calls;return !b.fail;}
    static uintptr_t world(void* p,uintptr_t scene){auto& b=*static_cast<Backend*>(p);check(scene==9&&b.worker==std::this_thread::get_id());return 10;}
    static int collect(void* p,void* observations,uintptr_t token){auto& b=*static_cast<Backend*>(p);check(observations&&token==10&&b.worker==std::this_thread::get_id());++b.collected;return 1;}
};
}
int main(){try{
    for(unsigned lifetime=0;lifetime<24;++lifetime){
        Backend b;fs15::platform::PhysicsWorker worker(9,0.01f,4,b.services);
        check(worker.tick(0)&&worker.tick(-1)&&worker.completed()==0);
        check(!worker.tick(std::numeric_limits<float>::infinity()));
        const unsigned count=lifetime%7;
        for(unsigned i=0;i<count;++i)check(worker.tick(16.0f+i)&&worker.completed()==i+1);
        check(worker.stop()&&worker.stop());check(b.calls==count&&b.collected==count);
        check(!worker.tick(16));
    }
    Backend failing;failing.fail=true;fs15::platform::PhysicsWorker worker(9,0.01f,4,failing.services);
    check(!worker.tick(16)&&!worker.stop()&&failing.calls==1&&!failing.collected);
    bool rejected=false;try{fs15::platform::PhysicsWorker invalid(9,0,4,failing.services);}catch(const std::invalid_argument&){rejected=true;}check(rejected);
    std::cout<<"Native dispatch over real auto-reset event/thread transport passed: 24 lifetimes, backend failure and clean stop\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
