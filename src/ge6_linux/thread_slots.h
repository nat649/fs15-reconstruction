#pragma once
// Separate TLS/FLS index pools and per-thread values for native CRT state.
// Fibers and Windows exception/unwind dispatch are not implemented here.
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <vector>
namespace ge6_slots {
#define GE6_SLOT_MS __attribute__((ms_abi))
using Callback=void(GE6_SLOT_MS*)(void*);
inline void(*trace)(const char*)=nullptr;
inline void(*set_error)(uint32_t)=nullptr;
struct Slot {bool used=false;Callback callback=nullptr;};
struct Values {std::vector<void*> tls=std::vector<void*>(1088),fls=std::vector<void*>(128);};
inline std::vector<Slot> tls(1088),fls(128);
inline std::mutex mutex;
inline uint64_t next=1;
inline std::map<uint64_t,std::shared_ptr<Values>> contexts;
inline thread_local uint64_t key=0;
inline Values& current(){if(!key){key=next++;contexts.emplace(key,std::make_shared<Values>());}return *contexts.at(key);}// mutex held by caller
inline uint32_t allocate(bool fiber,Callback callback){std::lock_guard<std::mutex> guard(mutex);auto& pool=fiber?fls:tls;for(uint32_t i=0;i<pool.size();++i)if(!pool[i].used){pool[i]={true,callback};return i;}set_error(8);return 0xffffffffu;}
inline GE6_SLOT_MS uint32_t tls_alloc(){trace("TlsAlloc");return allocate(false,nullptr);}
inline GE6_SLOT_MS uint32_t fls_alloc(Callback callback){trace("FlsAlloc");return allocate(true,callback);}
inline int set(bool fiber,uint32_t index,void* value){std::lock_guard<std::mutex> guard(mutex);auto& pool=fiber?fls:tls;if(index>=pool.size()||!pool[index].used){set_error(87);return 0;}auto& values=current();(fiber?values.fls:values.tls)[index]=value;return 1;}
inline void* get(bool fiber,uint32_t index){std::lock_guard<std::mutex> guard(mutex);auto& pool=fiber?fls:tls;if(index>=pool.size()||!pool[index].used){set_error(87);return nullptr;}auto& values=current();set_error(0);return (fiber?values.fls:values.tls)[index];}
inline int release(bool fiber,uint32_t index){std::vector<std::pair<Callback,void*>> callbacks;{
    std::lock_guard<std::mutex> guard(mutex);auto& pool=fiber?fls:tls;if(index>=pool.size()||!pool[index].used){set_error(87);return 0;}auto callback=pool[index].callback;pool[index]={};
    for(auto& item:contexts){auto& values=fiber?item.second->fls:item.second->tls;if(values[index]&&callback)callbacks.emplace_back(callback,values[index]);values[index]=nullptr;}
}for(auto item:callbacks)item.first(item.second);return 1;}
inline GE6_SLOT_MS int tls_set(uint32_t i,void* v){return set(false,i,v);}
inline GE6_SLOT_MS int fls_set(uint32_t i,void* v){return set(true,i,v);}
inline GE6_SLOT_MS void* tls_get(uint32_t i){return get(false,i);}
inline GE6_SLOT_MS void* fls_get(uint32_t i){return get(true,i);}
inline GE6_SLOT_MS int tls_free(uint32_t i){return release(false,i);}
inline GE6_SLOT_MS int fls_free(uint32_t i){return release(true,i);}
inline void cleanup(){
    for(int round=0;round<4;++round){std::vector<std::pair<Callback,void*>> callbacks;{
        std::lock_guard<std::mutex> guard(mutex);if(!key)return;auto& values=current().fls;for(size_t i=0;i<fls.size();++i)if(fls[i].used&&fls[i].callback&&values[i]){callbacks.emplace_back(fls[i].callback,values[i]);values[i]=nullptr;}
    }if(callbacks.empty())break;for(auto item:callbacks)item.first(item.second);}
    std::lock_guard<std::mutex> guard(mutex);contexts.erase(key);key=0;
}
#undef GE6_SLOT_MS
}
