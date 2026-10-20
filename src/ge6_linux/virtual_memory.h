#pragma once
// Authored reserve/commit transport for the original Lua allocator's NT calls.
// Only owned mappings are managed; executable protections are not supported.
#include <algorithm>
#include <cstdint>
#include <map>
#include <mutex>
#include <sys/mman.h>
#include <unistd.h>
#include <vector>
namespace ge6_vm {
#define GE6_VM_MS __attribute__((ms_abi))
inline void(*trace)(const char*)=nullptr;
inline void(*set_error)(uint32_t)=nullptr;
struct Region {size_t size;uint32_t protection;std::vector<uint32_t> pages;};
inline std::map<uintptr_t,Region> regions;
inline std::mutex mutex;
inline size_t page(){return size_t(sysconf(_SC_PAGESIZE));}
inline int protection(uint32_t value){switch(value){case 1:return PROT_NONE;case 2:return PROT_READ;case 4:return PROT_READ|PROT_WRITE;default:return -1;}}
inline auto containing(uintptr_t address,size_t bytes){auto found=regions.upper_bound(address);if(found==regions.begin())return regions.end();--found;return address>=found->first&&address-found->first<found->second.size&&bytes<=found->second.size-(address-found->first)?found:regions.end();}
inline GE6_VM_MS int32_t allocate(uintptr_t process,void** address,uintptr_t zero_bits,size_t* bytes,uint32_t type,uint32_t protect){
    trace("NtAllocateVirtualMemory");if(process!=~uintptr_t(0))return int32_t(0xc0000008u);if(!address||!bytes||!*bytes||*bytes>0x40000000||zero_bits>32||(type!=0x1000&&type!=0x2000&&type!=0x3000))return int32_t(0xc000000du);auto permissions=protection(protect);if(permissions<0)return int32_t(0xc0000045u);
    auto unit=page(),requested=reinterpret_cast<uintptr_t>(*address),start=requested&~uintptr_t(unit-1),size=(*bytes+(requested-start)+unit-1)&~(unit-1);if(start+size<start)return int32_t(0xc000000du);std::lock_guard<std::mutex> guard(mutex);
    if(requested&&type==0x1000){auto found=containing(start,size);if(found==regions.end())return int32_t(0xc0000018u);if(mprotect(reinterpret_cast<void*>(start),size,permissions))return int32_t(0xc0000017u);auto first=(start-found->first)/unit;std::fill_n(found->second.pages.begin()+first,size/unit,protect);*address=reinterpret_cast<void*>(start);*bytes=size;return 0;}
    int flags=MAP_PRIVATE|MAP_ANONYMOUS;if(requested)flags|=MAP_FIXED_NOREPLACE;else if(zero_bits)flags|=MAP_32BIT;bool committed=(type&0x1000)!=0;auto mapping=mmap(reinterpret_cast<void*>(start),size,committed?permissions:PROT_NONE,flags,-1,0);if(mapping==MAP_FAILED)return int32_t(errno==EEXIST?0xc0000018u:0xc0000017u);auto actual=reinterpret_cast<uintptr_t>(mapping);
    if(!requested&&zero_bits&&((actual+size-1)>>(64-zero_bits))){munmap(mapping,size);return int32_t(0xc0000017u);}regions.emplace(actual,Region{size,protect,std::vector<uint32_t>(size/unit,committed?protect:0)});*address=mapping;*bytes=size;return 0;
}
inline GE6_VM_MS int free(void* address,size_t bytes,uint32_t type){trace("VirtualFree");auto start=reinterpret_cast<uintptr_t>(address);std::lock_guard<std::mutex> guard(mutex);if(type==0x8000){auto found=regions.find(start);if(bytes||found==regions.end()){set_error(87);return 0;}if(munmap(address,found->second.size)){set_error(487);return 0;}regions.erase(found);return 1;}if(type!=0x4000||!bytes){set_error(87);return 0;}auto unit=page(),aligned=start&~uintptr_t(unit-1);if(bytes>0x40000000){set_error(87);return 0;}auto size=(bytes+(start-aligned)+unit-1)&~(unit-1);auto found=containing(aligned,size);if(found==regions.end()){set_error(487);return 0;}if(madvise(reinterpret_cast<void*>(aligned),size,MADV_DONTNEED)||mprotect(reinterpret_cast<void*>(aligned),size,PROT_NONE)){set_error(487);return 0;}std::fill_n(found->second.pages.begin()+(aligned-found->first)/unit,size/unit,0);return 1;}
struct Information {uint64_t address,allocation;uint32_t allocation_protection;uint16_t partition,padding;uint64_t size;uint32_t state,protection,type,padding2;};
static_assert(sizeof(Information)==48);
inline GE6_VM_MS size_t query(const void* address,Information* out,size_t capacity){trace("VirtualQuery");if(!out||capacity<sizeof(*out)){set_error(87);return 0;}auto start=reinterpret_cast<uintptr_t>(address)&~uintptr_t(page()-1);std::lock_guard<std::mutex> guard(mutex);auto found=containing(start,1);if(found==regions.end()){set_error(487);return 0;}auto index=(start-found->first)/page(),end=index+1;auto protect=found->second.pages[index];while(end<found->second.pages.size()&&found->second.pages[end]==protect)++end;*out=Information{start,found->first,found->second.protection,0,0,(end-index)*page(),protect?0x1000u:0x2000u,protect,0x20000u,0};return sizeof(*out);}
#undef GE6_VM_MS
}
