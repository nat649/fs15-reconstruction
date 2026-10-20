#pragma once
// Authored signal callbacks; no original GE6 instructions in these fixtures.
#include "console_services.h"
#include <chrono>
#include <condition_variable>
#include <stdexcept>
namespace ge6_console_test {
inline std::mutex mutex;
inline std::condition_variable changed;
inline std::vector<uint32_t> calls;
inline uint32_t tls=0,fls=0,main_tid=0;
inline std::atomic<uint32_t> cleaned{0};
inline bool broken=false;
inline __attribute__((ms_abi)) void cleanup(void*){++cleaned;}
inline __attribute__((ms_abi)) int handle(uint32_t event){
    uintptr_t self=0;asm("movq %%gs:0x30,%0":"=r"(self));uintptr_t stack=reinterpret_cast<uintptr_t>(&self),lower=0,upper=0;
    std::memcpy(&lower,reinterpret_cast<void*>(self+0x10),8);std::memcpy(&upper,reinterpret_cast<void*>(self+8),8);
    bool ok=lower<stack&&stack<upper&&uint32_t(syscall(SYS_gettid))!=main_tid&&ge6_slots::tls_get(tls)==nullptr;
    ge6_slots::fls_set(fls,reinterpret_cast<void*>(1));
    {std::lock_guard<std::mutex> lock(mutex);broken=broken||!ok;calls.push_back(event*10+1);}changed.notify_all();return 1;
}
inline __attribute__((ms_abi)) int decline(uint32_t event){std::lock_guard<std::mutex> lock(mutex);calls.push_back(event*10+2);return 0;}
inline __attribute__((ms_abi)) int remove_self(uint32_t event){ge6_console::set_handler(remove_self,0);return decline(event);}
inline void check(){
    auto require=[](bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);};
    ge6_console::Lifetime lifetime;ge6_native::Context context;main_tid=uint32_t(syscall(SYS_gettid));tls=ge6_slots::tls_alloc();fls=ge6_slots::fls_alloc(cleanup);ge6_slots::tls_set(tls,reinterpret_cast<void*>(123));
    auto wait=[&](size_t count){std::unique_lock<std::mutex> lock(mutex);require(changed.wait_for(lock,std::chrono::seconds(3),[&](){return calls.size()>=count;}),"Console callback did not arrive");};
    require(!ge6_console::set_handler(handle,0),"Removed an unregistered handler");
    require(ge6_console::set_handler(handle,1)&&ge6_console::set_handler(decline,1),"Cannot register control handlers");
    require(kill(getpid(),SIGINT)==0,"Cannot send SIGINT");wait(2);
    require(kill(getpid(),SIGQUIT)==0,"Cannot send SIGQUIT");wait(4);
    require(ge6_console::set_handler(nullptr,1),"Cannot ignore CTRL_C");
    require(kill(getpid(),SIGINT)==0&&kill(getpid(),SIGQUIT)==0,"Cannot send ignored/control signals");wait(6);
    require(ge6_console::set_handler(nullptr,0)&&ge6_console::set_handler(decline,0)&&ge6_console::set_handler(remove_self,1),"Control handler removal failed");
    require(kill(getpid(),SIGINT)==0,"Cannot send final SIGINT");wait(8);
    ge6_console::service().shutdown(); // Complete callback/FLS cleanup before examining results.
    {std::lock_guard<std::mutex> lock(mutex);require(!broken&&calls==std::vector<uint32_t>({2,1,12,11,12,11,2,1}),"Console order, ignore, thread/GS/TLS or removal differs");}
    require(cleaned==4&&ge6_slots::tls_get(tls)==reinterpret_cast<void*>(123),"Control callback FLS cleanup/parent TLS differs");
    require(ge6_slots::tls_free(tls)&&ge6_slots::fls_free(fls),"Cannot release parent slot fixtures");ge6_slots::cleanup();
}
}
