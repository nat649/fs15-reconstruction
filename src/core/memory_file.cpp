#include "memory_file.h"
#include <cstring>
#include <algorithm>
namespace {
uint32_t get(const void* p,size_t offset){uint32_t v;std::memcpy(&v,static_cast<const unsigned char*>(p)+offset,4);return v;}
uint64_t data(const void* p){uint64_t v;std::memcpy(&v,static_cast<const unsigned char*>(p)+8,8);return v;}
bool record(const void* p,size_t bytes){return p&&bytes>=0x1c;}
}
int fs15_memory_file_valid(const void* p,size_t bytes){return record(p,bytes)&&data(p)!=0;}
uint32_t fs15_memory_file_read(void* p,size_t bytes,size_t source_capacity,void* output,size_t output_capacity,uint32_t count){
    if(!record(p,bytes)||!data(p))return UINT32_MAX;
    const auto size=get(p,0x14),position=get(p,0x18);
    if(size>source_capacity||position>size||count>UINT32_MAX-position)return UINT32_MAX;
    count=std::min(count,size-position);
    if(count>output_capacity||(!output&&count))return UINT32_MAX;
    if(count)std::memcpy(output,reinterpret_cast<const unsigned char*>(static_cast<uintptr_t>(data(p)))+position,count);
    const auto next=position+count;std::memcpy(static_cast<unsigned char*>(p)+0x18,&next,4);return count;
}
int fs15_memory_file_seek(void* p,size_t bytes,uint32_t position){
    if(!record(p,bytes))return 0;
    position=std::min(position,get(p,0x14));std::memcpy(static_cast<unsigned char*>(p)+0x18,&position,4);return 1;
}
uint32_t fs15_memory_file_size(const void* p,size_t bytes){return record(p,bytes)?get(p,0x14):UINT32_MAX;}
uint32_t fs15_memory_file_tell(const void* p,size_t bytes){return record(p,bytes)&&data(p)?get(p,0x18):UINT32_MAX;}
int fs15_memory_file_eof(const void* p,size_t bytes){return record(p,bytes)&&get(p,0x14)<=get(p,0x18);}
