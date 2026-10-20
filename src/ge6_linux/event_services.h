#pragma once
// Authored unnamed-event OS transport used by the original script system.
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
namespace ge6_events {
#define GE6_EVENT_MS __attribute__((ms_abi))
inline void(*trace)(const char*)=nullptr;
inline void(*set_error)(uint32_t)=nullptr;
struct Event {std::mutex mutex;std::condition_variable changed;bool manual,signaled;Event(bool m,bool s):manual(m),signaled(s){}};
inline std::mutex registry_mutex;
inline std::map<uintptr_t,std::shared_ptr<Event>> registry;
inline uintptr_t next=0x71000000;
inline std::shared_ptr<Event> get(uintptr_t handle){std::lock_guard<std::mutex> lock(registry_mutex);auto found=registry.find(handle);return found==registry.end()?nullptr:found->second;}
inline bool owns(uintptr_t handle){return bool(get(handle));}
inline GE6_EVENT_MS uintptr_t create(void* security,int manual,int initial,const char* name){trace("CreateEventA");if(security||name){set_error(50);return 0;}auto event=std::make_shared<Event>(manual!=0,initial!=0);std::lock_guard<std::mutex> lock(registry_mutex);auto handle=next++;registry.emplace(handle,std::move(event));return handle;}
inline GE6_EVENT_MS int set(uintptr_t handle){trace("SetEvent");auto event=get(handle);if(!event){set_error(6);return 0;}std::lock_guard<std::mutex> lock(event->mutex);event->signaled=true;if(event->manual)event->changed.notify_all();else event->changed.notify_one();return 1;}
inline GE6_EVENT_MS int reset(uintptr_t handle){trace("ResetEvent");auto event=get(handle);if(!event){set_error(6);return 0;}std::lock_guard<std::mutex> lock(event->mutex);event->signaled=false;return 1;}
inline GE6_EVENT_MS uint32_t wait(uintptr_t handle,uint32_t milliseconds){trace("WaitForSingleObject");auto event=get(handle);if(!event){set_error(6);return 0xffffffffu;}std::unique_lock<std::mutex> lock(event->mutex);auto ready=[&](){return event->signaled;};if(milliseconds==0xffffffffu)event->changed.wait(lock,ready);else if(!event->changed.wait_for(lock,std::chrono::milliseconds(milliseconds),ready))return 258;if(!event->manual)event->signaled=false;return 0;}
inline GE6_EVENT_MS int close(uintptr_t handle){trace("CloseHandle(event)");std::lock_guard<std::mutex> lock(registry_mutex);if(registry.erase(handle))return 1;set_error(6);return 0;}
#undef GE6_EVENT_MS
}
