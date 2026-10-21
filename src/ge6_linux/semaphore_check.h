#pragma once
// Authored contention/lifetime fixture; no game code or resources.
#include "semaphore_services.h"
#include <atomic>
#include <thread>
#include <stdexcept>
namespace ge6_semaphore_test {
inline void check(uint32_t& error){
    auto require=[](bool value,const char* reason){if(!value)throw std::runtime_error(reason);};
    using namespace ge6_semaphores;
    require(!create(nullptr,-1,2,nullptr)&&error==87&&!create(nullptr,3,2,nullptr)&&error==87&&!create(nullptr,0,0,nullptr)&&error==87,"Invalid semaphore count accepted");
    require(!create(nullptr,0,1,"named")&&error==50&&!create(reinterpret_cast<void*>(1),0,1,nullptr)&&error==50,"Unsupported named/security semaphore accepted");
    auto handle=create(nullptr,2,3,nullptr);require(handle&&wait(handle,0)==0&&wait(handle,0)==0&&wait(handle,0)==258,"Semaphore count not consumed independently");
    int32_t previous=1234;require(!release(handle,4,&previous)&&error==298&&previous==1234&&wait(handle,0)==258,"Overflow changed count/previous value");
    require(release(handle,2,&previous)&&previous==0&&wait(handle,0)==0&&release(handle,2,&previous)&&previous==1&&!release(handle,1,nullptr)&&error==298,"Release count/limit differs");
    require(!release(handle,0,&previous)&&error==87&&!release(handle,-1,&previous)&&error==87,"Nonpositive release accepted");
    for(int i=0;i<3;++i)require(wait(handle,0)==0,"Released permits lost");
    std::atomic<unsigned> entered{0};uint32_t a=999,b=999;
    std::thread first([&](){++entered;a=wait(handle,2000);});std::thread second([&](){++entered;b=wait(handle,2000);});
    while(entered!=2)std::this_thread::yield();
    require(release(handle,1,nullptr),"Cannot release waiter");first.join();second.join();require((a==0&&b==258)||(b==0&&a==258),"One permit woke two consumers or was lost");
    std::thread infinite([&](){a=wait(handle,0xffffffffu);});require(release(handle,1,nullptr),"Cannot wake infinite waiter");infinite.join();require(a==0,"Infinite wait not released");
    require(close(handle)&&!close(handle)&&error==6&&!release(handle,1,nullptr)&&error==6&&wait(handle,0)==0xffffffffu&&error==6,"Semaphore handle lifetime differs");
}
}
