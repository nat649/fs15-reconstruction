#include "physics_worker.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>
#include <cstdlib>
#include <cstring>
namespace {
void check(bool b){if(!b)throw std::runtime_error("Physics event/thread fixture failed");}
struct Backend {
    std::thread::id main=std::this_thread::get_id(),worker{};unsigned calls=0,collected=0;bool fail=false;
    FS15PhysicsDispatchServices services{this,simulate,world,collect,nullptr,nullptr};
    static int simulate(void* p,uintptr_t scene,float seconds,uint32_t iterations,float step){auto& b=*static_cast<Backend*>(p);b.worker=std::this_thread::get_id();check(b.worker!=b.main&&scene==9&&seconds>0&&iterations==4&&step==0.01f);++b.calls;return !b.fail;}
    static uintptr_t world(void* p,uintptr_t scene){auto& b=*static_cast<Backend*>(p);check(scene==9&&b.worker==std::this_thread::get_id());return 10;}
    static int collect(void* p,void* observations,uintptr_t token){auto& b=*static_cast<Backend*>(p);check(observations&&token==10&&b.worker==std::this_thread::get_id());++b.collected;return 1;}
};
struct Contacts {
    uint8_t endpoints[2][0x20]{},bodies[2][0x118]{},metadata[2][0x20]{};
    FS15PhysicsPair pair{};int32_t bucket=0;uintptr_t listener=55;unsigned allocated=0,released=0,events=0;
    template<class T>static void put(void* p,size_t offset,T value){std::memcpy(static_cast<uint8_t*>(p)+offset,&value,sizeof(value));}
    static int configure(void* p,FS15PhysicsDispatchState* state){auto& c=*static_cast<Contacts*>(p);
        for(size_t i=0;i<2;++i){put(c.endpoints[i],0,reinterpret_cast<uintptr_t>(c.bodies[i]));put(c.endpoints[i],0x18,uint32_t(i+1));put(c.bodies[i],0x110,reinterpret_cast<uintptr_t>(c.metadata[i]));put(c.metadata[i],8,uintptr_t(100+i));}
        c.pair.first=reinterpret_cast<uintptr_t>(c.endpoints[0]);c.pair.second=reinterpret_cast<uintptr_t>(c.endpoints[1]);c.pair.mask=1;c.pair.next=-1;
        auto* cache=reinterpret_cast<FS15PhysicsContacts*>(reinterpret_cast<uint8_t*>(state)+0x48);cache->count=cache->capacity=1;cache->current=&c.pair;cache->buckets=&c.bucket;put(state,0x198,&c.listener);put(state,0x1a0,&c.listener+1);return 1;
    }
    static void* allocate(void* p,size_t n,size_t alignment){auto& c=*static_cast<Contacts*>(p);check(alignment==16);++c.allocated;return std::malloc(n);}
    static void release(void* p,void* value){++static_cast<Contacts*>(p)->released;std::free(value);}
    static int observer(void* p,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uint32_t,uint32_t){++static_cast<Contacts*>(p)->events;return 0;}
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
    Contacts contacts;Backend connected;FS15PhysicsPostServices services{};services.user=&contacts;services.allocate=Contacts::allocate;services.release=Contacts::release;services.observer=Contacts::observer;
    fs15::platform::PhysicsWorker::PostBackend post{services,Contacts::configure};
    {fs15::platform::PhysicsWorker joined(9,0.01f,4,connected.services,&post);check(joined.tick(16)&&joined.tick(17)&&joined.stop());check(contacts.allocated==1&&contacts.released==0&&contacts.events==2);}
    check(contacts.released==1);
    Backend invalid_graph;fs15::platform::PhysicsWorker::PostBackend invalid_post{};
    invalid_post.configure=[](void*,FS15PhysicsDispatchState* state){uintptr_t empty=0;std::memcpy(reinterpret_cast<uint8_t*>(state)+0x250,&empty,8);return 1;};
    fs15::platform::PhysicsWorker broken(9,0.01f,4,invalid_graph.services,&invalid_post);
    check(!broken.tick(16)&&!broken.tick(17)&&broken.stop()&&broken.completed()==0);
    std::cout<<"Native dispatch over real auto-reset event/thread transport passed: 24 lifetimes, backend failure and clean stop\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
