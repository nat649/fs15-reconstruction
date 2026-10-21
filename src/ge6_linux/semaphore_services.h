#pragma once
// Authored process-local Windows counting semaphores using Linux C++ waits.
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
namespace ge6_semaphores {
#define GE6_SEM_MS __attribute__((ms_abi))
inline void(*trace)(const char*)=nullptr;
inline void(*set_error)(uint32_t)=nullptr;
inline void(*record)(const char*,uintptr_t,int32_t,int32_t,uintptr_t)=nullptr;
struct Semaphore {std::mutex mutex;std::condition_variable changed;int32_t count,maximum;Semaphore(int32_t initial,int32_t limit):count(initial),maximum(limit){}};
inline std::mutex registry_mutex;
inline std::map<uintptr_t,std::shared_ptr<Semaphore>> registry;
inline uintptr_t next=0x76000000;
inline std::shared_ptr<Semaphore> get(uintptr_t handle){std::lock_guard<std::mutex> lock(registry_mutex);auto found=registry.find(handle);return found==registry.end()?nullptr:found->second;}
inline bool owns(uintptr_t handle){return bool(get(handle));}
inline GE6_SEM_MS uintptr_t create(void* security,int32_t initial,int32_t maximum,const char* name){
    trace("CreateSemaphoreA");if(security||name){set_error(50);return 0;}if(maximum<=0||initial<0||initial>maximum){set_error(87);return 0;}
    auto value=std::make_shared<Semaphore>(initial,maximum);uintptr_t handle;{std::lock_guard<std::mutex> lock(registry_mutex);handle=next++;registry.emplace(handle,std::move(value));}
    record("CREATE",handle,initial,maximum,reinterpret_cast<uintptr_t>(__builtin_return_address(0)));return handle;
}
inline GE6_SEM_MS int release(uintptr_t handle,int32_t count,int32_t* previous){
    trace("ReleaseSemaphore");auto value=get(handle);if(!value){set_error(6);return 0;}if(count<=0){set_error(87);return 0;}
    std::lock_guard<std::mutex> lock(value->mutex);if(count>value->maximum-value->count){set_error(298);return 0;}if(previous)*previous=value->count;value->count+=count;value->changed.notify_all();return 1;
}
inline GE6_SEM_MS uint32_t wait(uintptr_t handle,uint32_t milliseconds){
    trace("WaitForSingleObject(semaphore)");auto value=get(handle);if(!value){set_error(6);return 0xffffffffu;}std::unique_lock<std::mutex> lock(value->mutex);auto ready=[&](){return value->count>0;};
    if(milliseconds==0xffffffffu)value->changed.wait(lock,ready);else if(!value->changed.wait_for(lock,std::chrono::milliseconds(milliseconds),ready))return 258;
    --value->count;return 0;
}
inline GE6_SEM_MS int close(uintptr_t handle){trace("CloseHandle(semaphore)");std::lock_guard<std::mutex> lock(registry_mutex);if(registry.erase(handle))return 1;set_error(6);return 0;}
#undef GE6_SEM_MS
}
