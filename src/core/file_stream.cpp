#include "file_stream.h"
#include <cstring>
#include <limits>

namespace {
constexpr uint32_t failed=UINT32_MAX;
bool handle(const void* record,size_t length,uint64_t& value){
    if(!record||length<0x11)return false;
    std::memcpy(&value,static_cast<const unsigned char*>(record)+8,8);
    return value!=UINT64_MAX;
}
}
uint32_t fs15_file_read(void* record,size_t length,void* output,uint32_t count,const FS15_FileServices* services){
    uint64_t token;
    if(!handle(record,length,token)||!services||!services->read||(!output&&count))return failed;
    auto* eof=static_cast<unsigned char*>(record)+0x10;
    *eof=0;
    uint32_t consumed=0;
    const auto error=services->read(services->user,token,output,count,&consumed);
    if(error==0){
        *eof=consumed!=count;
        return consumed;
    }
    if((error==0x570||error==0x571)&&services->corrupt)services->corrupt(services->user,record,error);
    return failed;
}
int fs15_file_seek(void* record,size_t length,uint32_t position,const FS15_FileServices* services){
    uint64_t token;
    if(!handle(record,length,token)||!services||!services->seek)return 0;
    // SetFilePointer takes a signed LONG when its high-word pointer is null.
    // Preserve that interpretation instead of silently adding 64-bit seeking.
    const auto distance=position<=INT32_MAX?static_cast<int32_t>(position):
        static_cast<int32_t>(static_cast<int64_t>(position)-INT64_C(0x100000000));
    const auto actual=services->seek(services->user,token,distance,0);
    if(actual==failed)return 0;
    static_cast<unsigned char*>(record)[0x10]=actual<position;
    return 1;
}
uint32_t fs15_file_size(const void* record,size_t length,const FS15_FileServices* services){
    uint64_t token;
    return handle(record,length,token)&&services&&services->size?services->size(services->user,token):failed;
}
uint32_t fs15_file_tell(const void* record,size_t length,const FS15_FileServices* services){
    uint64_t token;
    return handle(record,length,token)&&services&&services->seek?services->seek(services->user,token,0,1):failed;
}
