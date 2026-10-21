#pragma once
// Authored read-only OS inventory transport, not a general Windows registry.
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <sched.h>
#include <sstream>
#include <string>
#include <sys/resource.h>
#include <unistd.h>
#include <cpuid.h>
#include <pwd.h>
#include <vector>
#include "file_services.h"
#include "ansi_reference.h"
namespace ge6_system {
#define GE6_SYS_MS __attribute__((ms_abi))
inline void(*trace)(const char*)=nullptr;
inline void(*set_error)(uint32_t)=nullptr;
inline std::recursive_mutex registry_mutex;
inline std::map<uintptr_t,std::string> cpu_keys;
inline uintptr_t next_key=0x70000000;
inline std::string cpu_name(){std::ifstream file("/proc/cpuinfo");std::string line;while(std::getline(file,line)){auto split=line.find(':');if(split==std::string::npos)continue;auto key=line.substr(0,split);while(!key.empty()&&(key.back()==' '||key.back()=='\t'))key.pop_back();if(key=="model name"){auto value=line.substr(split+1);auto start=value.find_first_not_of(" \t");return start==std::string::npos?"":value.substr(start);}}return "";}
inline std::string lower(std::string text){for(auto& c:text)if(c>='A'&&c<='Z')c=char(c+('a'-'A'));return text;}
inline bool account_name(std::u16string& result){
    long size=sysconf(_SC_GETPW_R_SIZE_MAX);std::vector<char> storage(size>0?size_t(size):1024);passwd value{},*found=nullptr;
    for(;;){int status=getpwuid_r(geteuid(),&value,storage.data(),storage.size(),&found);if(status==ERANGE&&storage.size()<1048576){storage.resize(storage.size()*2);continue;}if(status||!found||!found->pw_name){set_error(1317);return false;}try{result=ge6_fs::utf16(found->pw_name);}catch(const std::range_error&){set_error(1113);return false;}return true;}
}
inline GE6_SYS_MS int user_name(uint16_t* output,uint32_t* count){
    trace("GetUserNameW");if(!count){set_error(87);return 0;}std::u16string actual;if(!account_name(actual))return 0;auto required=uint32_t(actual.size()+1);auto capacity=*count;*count=required;if(capacity<required){set_error(122);return 0;}if(!output){set_error(87);return 0;}std::memcpy(output,actual.c_str(),required*2);return 1;
}
inline GE6_SYS_MS int user_name_a(char* output,uint32_t* count){trace("GetUserNameA");if(!count){set_error(87);return 0;}std::u16string actual;if(!account_name(actual))return 0;std::string bytes;for(auto unit:actual){auto found=std::find(ge6_ansi::unicode.begin(),ge6_ansi::unicode.end(),uint16_t(unit));if(found==ge6_ansi::unicode.end()){set_error(1113);return 0;}bytes.push_back(char(found-ge6_ansi::unicode.begin()));}auto capacity=*count;*count=uint32_t(bytes.size()+1);if(capacity<*count){set_error(122);return 0;}if(!output){set_error(87);return 0;}std::memcpy(output,bytes.c_str(),*count);return 1;}
inline GE6_SYS_MS int computer_name(char* output,uint32_t* count){trace("GetComputerNameA");if(!count){set_error(87);return 0;}char name[256]{};if(gethostname(name,sizeof(name)-1)){set_error(31);return 0;}auto size=std::strlen(name);if(size>15){set_error(50);return 0;}if(*count<=size){*count=uint32_t(size+1);set_error(111);return 0;}if(!output){set_error(87);return 0;}std::memcpy(output,name,size+1);*count=uint32_t(size);return 1;}
inline uint64_t cpu_time(const timeval& value){return uint64_t(value.tv_sec)*10000000+uint64_t(value.tv_usec)*10;}
inline GE6_SYS_MS int process_times(uintptr_t handle,uint64_t* created,uint64_t* exited,uint64_t* kernel,uint64_t* user){
    trace("GetProcessTimes");if(handle!=~uintptr_t(0)){set_error(6);return 0;}if(!created||!exited||!kernel||!user){set_error(87);return 0;}rusage usage{};if(getrusage(RUSAGE_SELF,&usage)){set_error(31);return 0;}
    std::ifstream process("/proc/self/stat");std::string line;std::getline(process,line);auto end=line.rfind(')');if(end==std::string::npos){set_error(31);return 0;}std::istringstream fields(line.substr(end+2));uint64_t start=0;std::string item;for(unsigned i=0;i<=19;++i){if(!(fields>>item)){set_error(31);return 0;}if(i==19){std::istringstream value(item);value>>start;}}
    uint64_t boot=0;std::ifstream stats("/proc/stat");while(std::getline(stats,line)){std::istringstream row(line);if(row>>item&&item=="btime"){row>>boot;break;}}long hz=sysconf(_SC_CLK_TCK);if(!boot||hz<=0){set_error(31);return 0;}
    *created=(boot+11644473600ull)*10000000+start*10000000/uint64_t(hz);*exited=0;*kernel=cpu_time(usage.ru_stime);*user=cpu_time(usage.ru_utime);return 1;
}
inline GE6_SYS_MS int system_times(uint64_t* idle,uint64_t* kernel,uint64_t* user){trace("GetSystemTimes");if(!idle||!kernel||!user){set_error(87);return 0;}std::ifstream file("/proc/stat");std::string cpu;uint64_t usr=0,nice=0,system=0,idling=0,waiting=0,irq=0,soft=0;if(!(file>>cpu>>usr>>nice>>system>>idling>>waiting>>irq>>soft)||cpu!="cpu"){set_error(31);return 0;}long hz=sysconf(_SC_CLK_TCK);if(hz<=0){set_error(31);return 0;}*idle=(idling+waiting)*10000000/uint64_t(hz);*kernel=(system+irq+soft+idling+waiting)*10000000/uint64_t(hz);*user=(usr+nice)*10000000/uint64_t(hz);return 1;}
inline GE6_SYS_MS int hardware_profile_unavailable(void*){trace("GetCurrentHwProfileA(unavailable)");set_error(50);return 0;}
inline GE6_SYS_MS int registry_quota_unavailable(uint32_t*,uint32_t*){trace("GetSystemRegistryQuota(unavailable)");set_error(50);return 0;}
inline GE6_SYS_MS uint32_t adapter_info_unavailable(void*,uint32_t*){trace("GetAdaptersInfo(unavailable)");return 50;}
inline GE6_SYS_MS int32_t open_key(uintptr_t root,const char* name,uint32_t options,uint32_t access,uintptr_t* out){std::lock_guard<std::recursive_mutex> guard(registry_mutex);trace("RegOpenKeyExA");if(!out||!name||options)return 87;if(access&~0x20319u)return 5;*out=0;if(uint32_t(root)!=0x80000002u||lower(name)!="hardware\\description\\system\\centralprocessor\\0")return 2;auto model=cpu_name();if(model.empty())return 2;auto key=next_key++;cpu_keys.emplace(key,std::move(model));*out=key;return 0;}
inline GE6_SYS_MS int32_t query_key(uintptr_t key,const char* name,uint32_t* reserved,uint32_t* type,void* out,uint32_t* size){std::lock_guard<std::recursive_mutex> guard(registry_mutex);trace("RegQueryValueExA");if(reserved||!name||!size)return 87;auto found=cpu_keys.find(key);if(found==cpu_keys.end())return 6;if(lower(name)!="processornamestring")return 2;uint32_t required=uint32_t(found->second.size()+1),capacity=*size;*size=required;if(type)*type=1;if(!out)return 0;if(capacity<required)return 234;std::memcpy(out,found->second.c_str(),required);return 0;}
inline GE6_SYS_MS int32_t close_key(uintptr_t key){std::lock_guard<std::recursive_mutex> guard(registry_mutex);trace("RegCloseKey");return cpu_keys.erase(key)?0:6;}
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
