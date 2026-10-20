#pragma once
// Authored read-only OS inventory transport, not a general Windows registry.
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <map>
#include <sched.h>
#include <sstream>
#include <string>
#include <sys/resource.h>
#include <unistd.h>
#include <cpuid.h>
namespace ge6_system {
#define GE6_SYS_MS __attribute__((ms_abi))
inline void(*trace)(const char*)=nullptr;
inline void(*set_error)(uint32_t)=nullptr;
inline std::map<uintptr_t,std::string> cpu_keys;
inline uintptr_t next_key=0x70000000;
inline std::string cpu_name(){std::ifstream file("/proc/cpuinfo");std::string line;while(std::getline(file,line)){auto split=line.find(':');if(split==std::string::npos)continue;auto key=line.substr(0,split);while(!key.empty()&&(key.back()==' '||key.back()=='\t'))key.pop_back();if(key=="model name"){auto value=line.substr(split+1);auto start=value.find_first_not_of(" \t");return start==std::string::npos?"":value.substr(start);}}return "";}
inline std::string lower(std::string text){for(auto& c:text)if(c>='A'&&c<='Z')c=char(c+('a'-'A'));return text;}
inline GE6_SYS_MS int32_t open_key(uintptr_t root,const char* name,uint32_t options,uint32_t access,uintptr_t* out){trace("RegOpenKeyExA");if(!out||!name||options)return 87;if(access&~0x20319u)return 5;*out=0;if(uint32_t(root)!=0x80000002u||lower(name)!="hardware\\description\\system\\centralprocessor\\0")return 2;auto model=cpu_name();if(model.empty())return 2;auto key=next_key++;cpu_keys.emplace(key,std::move(model));*out=key;return 0;}
inline GE6_SYS_MS int32_t query_key(uintptr_t key,const char* name,uint32_t* reserved,uint32_t* type,void* out,uint32_t* size){trace("RegQueryValueExA");if(reserved||!name||!size)return 87;auto found=cpu_keys.find(key);if(found==cpu_keys.end())return 6;if(lower(name)!="processornamestring")return 2;uint32_t required=uint32_t(found->second.size()+1),capacity=*size;*size=required;if(type)*type=1;if(!out)return 0;if(capacity<required)return 234;std::memcpy(out,found->second.c_str(),required);return 0;}
inline GE6_SYS_MS int32_t close_key(uintptr_t key){trace("RegCloseKey");return cpu_keys.erase(key)?0:6;}
struct MemoryStatus {uint32_t size,load;uint64_t physical,available,pagefile,available_pagefile,virtual_total,virtual_available,extended;};
static_assert(sizeof(MemoryStatus)==64 && offsetof(MemoryStatus,physical)==8);
inline uint64_t virtual_used(){std::ifstream maps("/proc/self/maps");std::string line;uint64_t total=0;while(std::getline(maps,line)){std::istringstream fields(line);uint64_t start=0,end=0;char dash=0;if(fields>>std::hex>>start>>dash>>end&&dash=='-'&&end>=start&&start<(1ull<<47))total+=end-start;}return total;}
inline GE6_SYS_MS int memory(MemoryStatus* out){trace("GlobalMemoryStatusEx");if(!out||out->size!=64){set_error(87);return 0;}std::ifstream file("/proc/meminfo");if(!file){set_error(31);return 0;}std::map<std::string,uint64_t> values;std::string line;while(std::getline(file,line)){std::istringstream fields(line);std::string key;uint64_t count=0;if(fields>>key>>count)values[key]=count*1024;}uint64_t total=values["MemTotal:"];if(!total){set_error(31);return 0;}MemoryStatus result{};result.size=64;result.physical=total;result.available=std::min(total,values.count("MemAvailable:")?values["MemAvailable:"]:values["MemFree:"]+values["Buffers:"]+values["Cached:"]);result.load=uint32_t((total-result.available)*100/total);result.pagefile=total+values["SwapTotal:"];result.available_pagefile=result.available+values["SwapFree:"];
    // A conservative low-47-bit guest address budget, bounded by real RLIMIT_AS.
    rlimit limit{};result.virtual_total=(1ull<<47)-65536;if(getrlimit(RLIMIT_AS,&limit)==0&&limit.rlim_cur!=RLIM_INFINITY)result.virtual_total=std::min(result.virtual_total,uint64_t(limit.rlim_cur));auto used=virtual_used();result.virtual_available=result.virtual_total>used?result.virtual_total-used:0;*out=result;return 1;
}
struct SystemInfo {uint16_t architecture,reserved;uint32_t page_size;uint64_t minimum,maximum,mask;uint32_t processors,type,granularity;uint16_t level,revision;};
static_assert(sizeof(SystemInfo)==48 && offsetof(SystemInfo,processors)==32);
inline GE6_SYS_MS void information(SystemInfo* out){trace("GetSystemInfo/GetNativeSystemInfo");std::memset(out,0,sizeof(*out));out->architecture=9;out->page_size=uint32_t(sysconf(_SC_PAGESIZE));out->minimum=65536;out->maximum=(1ull<<47)-65537;out->type=8664;out->granularity=65536;cpu_set_t cpus{};if(sched_getaffinity(0,sizeof(cpus),&cpus)==0){for(int i=0;i<64;++i)if(CPU_ISSET(i,&cpus))out->mask|=1ull<<i;out->processors=std::min(uint32_t(CPU_COUNT(&cpus)),64u);}unsigned a=0,b=0,c=0,d=0;if(__get_cpuid(1,&a,&b,&c,&d)){unsigned family=(a>>8)&15,model=(a>>4)&15;if(family==6||family==15)model|=((a>>16)&15)<<4;if(family==15)family+=(a>>20)&255;out->level=uint16_t(family);out->revision=uint16_t((model<<8)|(a&15));}}
#undef GE6_SYS_MS
}
