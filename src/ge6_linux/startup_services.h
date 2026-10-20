#pragma once
// Authored transport for the observed main-thread timer/affinity startup calls.
#include <algorithm>
#include <cstdint>
#include <map>
#include <pthread.h>
#include <sched.h>
#include <sys/prctl.h>
#include <time.h>
namespace ge6_startup {
#define GE6_START_MS __attribute__((ms_abi))
inline void(*trace)(const char*)=nullptr;
inline void(*set_error)(uint32_t)=nullptr;
inline thread_local std::map<uint32_t,uint32_t> periods;
inline thread_local unsigned long original_slack=0;
inline thread_local cpu_set_t allowed;
inline thread_local bool affinity_ready=false;
inline bool capture_affinity(){affinity_ready=pthread_getaffinity_np(pthread_self(),sizeof(allowed),&allowed)==0;return affinity_ready;}
inline GE6_START_MS uint32_t begin(uint32_t period){
    trace("timeBeginPeriod");if(period<1||period>1000)return 97;timespec resolution{};if(clock_getres(CLOCK_MONOTONIC,&resolution)||uint64_t(resolution.tv_sec)*1000000000+uint64_t(resolution.tv_nsec)>uint64_t(period)*1000000)return 97;
    if(periods.empty()){auto slack=prctl(PR_GET_TIMERSLACK);if(slack<0)return 97;original_slack=static_cast<unsigned long>(slack);}
    auto minimum=periods.empty()?period:std::min(period,periods.begin()->first);auto slack=std::min(original_slack,static_cast<unsigned long>(minimum)*1000000ul);if(prctl(PR_SET_TIMERSLACK,slack))return 97;++periods[period];return 0;
}
inline GE6_START_MS uint32_t end(uint32_t period){trace("timeEndPeriod");auto found=periods.find(period);if(found==periods.end())return 97;auto next=periods;if(--next[period]==0)next.erase(period);auto slack=next.empty()?original_slack:std::min(original_slack,static_cast<unsigned long>(next.begin()->first)*1000000ul);if(prctl(PR_SET_TIMERSLACK,slack))return 97;periods=std::move(next);return 0;}
inline GE6_START_MS uint32_t ticks(){trace("timeGetTime");timespec now{};clock_gettime(CLOCK_MONOTONIC,&now);return uint32_t(uint64_t(now.tv_sec)*1000+uint64_t(now.tv_nsec)/1000000);}
inline GE6_START_MS uintptr_t current_thread(){trace("GetCurrentThread");return ~uintptr_t(1);}
inline GE6_START_MS uintptr_t affinity(uintptr_t thread,uintptr_t mask){
    trace("SetThreadAffinityMask");if(thread!=~uintptr_t(1)){set_error(6);return 0;}if(!mask){set_error(87);return 0;}if(!affinity_ready&&!capture_affinity()){set_error(31);return 0;}
    cpu_set_t previous{},desired{};CPU_ZERO(&desired);if(pthread_getaffinity_np(pthread_self(),sizeof(previous),&previous)){set_error(31);return 0;}uintptr_t old=0;
    for(int i=0;i<CPU_SETSIZE;++i){if(CPU_ISSET(i,&previous)){if(i>=64){set_error(50);return 0;}old|=uintptr_t(1)<<i;}}
    for(int i=0;i<64;++i)if(mask&(uintptr_t(1)<<i)){if(!CPU_ISSET(i,&allowed)){set_error(87);return 0;}CPU_SET(i,&desired);}
    int error=pthread_setaffinity_np(pthread_self(),sizeof(desired),&desired);if(error){set_error(error==EINVAL?87:5);return 0;}return old;
}
#undef GE6_START_MS
}
