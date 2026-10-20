#pragma once
// Authored pthread lifecycle and Microsoft-ABI callback transport.
#include "native_thread.h"
#include "thread_slots.h"
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <memory>
#include <mutex>
#include <pthread.h>
namespace ge6_threads {
#define GE6_THREAD_MS __attribute__((ms_abi))
using Entry=uint32_t(GE6_THREAD_MS*)(void*);
inline void(*trace)(const char*)=nullptr;
inline void(*set_error)(uint32_t)=nullptr;
inline void(*record)(const char*,uintptr_t,uint32_t,uint32_t)=nullptr;
struct State {std::mutex mutex;std::condition_variable changed;pthread_t native{};Entry entry;void* argument;uint32_t tid=0,code=259,suspended=0;bool ready=false,done=false;};
inline std::mutex registry_mutex;
inline std::map<uintptr_t,std::shared_ptr<State>> registry;
inline uintptr_t next=0x72000000;
inline size_t default_stack=1048576;
inline std::atomic<uint32_t> running{0};
inline thread_local uintptr_t active_entry=0;
inline thread_local bool traced_import=false;
inline std::shared_ptr<State> get(uintptr_t handle){std::lock_guard<std::mutex> guard(registry_mutex);auto found=registry.find(handle);return found==registry.end()?nullptr:found->second;}
inline bool owns(uintptr_t handle){return bool(get(handle));}
inline void* start(void* raw){
    std::unique_ptr<std::shared_ptr<State>> transfer(static_cast<std::shared_ptr<State>*>(raw));auto state=*transfer;transfer.reset();
    try{uint32_t tid=uint32_t(syscall(SYS_gettid)),result=0;{ge6_native::Context context;{
        std::unique_lock<std::mutex> lock(state->mutex);state->tid=tid;state->ready=true;state->changed.notify_all();state->changed.wait(lock,[&](){return !state->suspended;});}
        active_entry=reinterpret_cast<uintptr_t>(state->entry);record("ENTER",active_entry,tid,0);result=state->entry(state->argument);ge6_slots::cleanup();record("EXIT",active_entry,tid,result);active_entry=0;}
        {std::lock_guard<std::mutex> lock(state->mutex);state->code=result;--running;state->done=true;}state->changed.notify_all();
    }catch(const std::exception& e){std::cerr<<"GE6_THREAD_TRANSPORT_ERROR "<<e.what()<<'\n';std::_Exit(125);}return nullptr;
}
inline GE6_THREAD_MS uintptr_t create(void* security,size_t stack,Entry entry,void* argument,uint32_t flags,uint32_t* tid){
    trace("CreateThread");if(security){set_error(50);return 0;}if(!entry||(flags&~0x10004u)||stack>64*1024*1024){set_error(87);return 0;}
    auto bytes=stack?((flags&0x10000u)?stack:std::max(stack,default_stack)):default_stack;auto page=size_t(sysconf(_SC_PAGESIZE));bytes=(std::max(bytes,size_t(PTHREAD_STACK_MIN))+page-1)&~(page-1);
    auto state=std::make_shared<State>();state->entry=entry;state->argument=argument;state->suspended=(flags&4)?1:0;uintptr_t handle;{std::lock_guard<std::mutex> guard(registry_mutex);handle=next++;registry.emplace(handle,state);}
    pthread_attr_t attr;int error=pthread_attr_init(&attr);if(!error){error=pthread_attr_setstacksize(&attr,bytes);if(!error)error=pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);if(!error){auto* transfer=new std::shared_ptr<State>(state);++running;error=pthread_create(&state->native,&attr,start,transfer);if(error){--running;delete transfer;}}pthread_attr_destroy(&attr);}
    if(error){std::lock_guard<std::mutex> guard(registry_mutex);registry.erase(handle);set_error(error==EINVAL?87:8);return 0;}
    std::unique_lock<std::mutex> lock(state->mutex);state->changed.wait(lock,[&](){return state->ready;});if(tid)*tid=state->tid;return handle;
}
inline GE6_THREAD_MS uint32_t resume(uintptr_t handle){trace("ResumeThread");auto state=get(handle);if(!state){set_error(6);return 0xffffffffu;}std::lock_guard<std::mutex> lock(state->mutex);auto count=state->suspended;if(count)--state->suspended;state->changed.notify_all();return count;}
inline GE6_THREAD_MS uint32_t wait(uintptr_t handle,uint32_t milliseconds){trace("WaitForSingleObject(thread)");auto state=get(handle);if(!state){set_error(6);return 0xffffffffu;}std::unique_lock<std::mutex> lock(state->mutex);auto ready=[&](){return state->done;};if(milliseconds==0xffffffffu)state->changed.wait(lock,ready);else if(!state->changed.wait_for(lock,std::chrono::milliseconds(milliseconds),ready))return 258;return 0;}
inline GE6_THREAD_MS int exit_code(uintptr_t handle,uint32_t* code){trace("GetExitCodeThread");auto state=get(handle);if(!state){set_error(6);return 0;}if(!code){set_error(87);return 0;}std::lock_guard<std::mutex> lock(state->mutex);*code=state->done?state->code:259;return 1;}
inline GE6_THREAD_MS int close(uintptr_t handle){trace("CloseHandle(thread)");std::lock_guard<std::mutex> guard(registry_mutex);if(registry.erase(handle))return 1;set_error(6);return 0;}
inline bool name(uint32_t tid,const char* raw){
    trace("ThreadNameNotification");std::string label(raw);label.resize(std::min(label.size(),size_t(15)));int error=ESRCH;
    if(tid==0xffffffffu||tid==uint32_t(syscall(SYS_gettid)))error=pthread_setname_np(pthread_self(),label.c_str());
    else{std::shared_ptr<State> state;{std::lock_guard<std::mutex> guard(registry_mutex);for(auto& item:registry)if(item.second->tid==tid){state=item.second;break;}}if(state){std::lock_guard<std::mutex> guard(state->mutex);if(!state->done)error=pthread_setname_np(state->native,label.c_str());}}
    if(error){set_error(error==ESRCH?87:5);return false;}return true;
}
#undef GE6_THREAD_MS
}
