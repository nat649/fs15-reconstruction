#include "buffered_file.h"
#include <algorithm>
#include <cstring>

namespace {
uint32_t get(const void* p,size_t offset){uint32_t v;std::memcpy(&v,static_cast<const unsigned char*>(p)+offset,4);return v;}
void put(void* p,size_t offset,uint32_t v){std::memcpy(static_cast<unsigned char*>(p)+offset,&v,4);}
bool valid(const void* p,size_t bytes){return p&&bytes>=FS15_BUFFERED_FILE_BYTES&&get(p,0x8048)<=get(p,0x804c)&&get(p,0x804c)<=FS15_FILE_BUFFER_BYTES;}
}
uint32_t fs15_buffered_file_read(void* record,size_t bytes,void* output,size_t capacity,uint32_t count,const FS15_FileServices* services){
    if(!count||!valid(record,bytes)||!output||capacity<count||!services||!services->read)return 0;
    auto* p=static_cast<unsigned char*>(record);auto* target=static_cast<unsigned char*>(output);
    uint32_t total=0;bool short_read=false;
    while(count){
        if(short_read)return total;
        if(get(p,0x8048)==get(p,0x804c)){
            const auto read=fs15_file_read(record,bytes,p+0x48,FS15_FILE_BUFFER_BYTES,services);
            if(read>=0x80000000u){p[0x8054]=1;return total;}
            // A callback returning more than the requested buffer length is
            // outside the OS contract; stop before an invalid buffer access.
            if(read>FS15_FILE_BUFFER_BYTES){p[0x8054]=1;return total;}
            put(p,0x8048,0);put(p,0x804c,read);short_read=read!=FS15_FILE_BUFFER_BYTES;
        }
        const auto copied=std::min(count,get(p,0x804c)-get(p,0x8048));
        if(copied){
            std::memcpy(target+total,p+0x48+get(p,0x8048),copied);
            put(p,0x8048,get(p,0x8048)+copied);
            total+=copied;count-=copied;put(p,0x8050,get(p,0x8050)+copied);
        }
    }
    return total;
}
uint32_t fs15_buffered_file_seek(void* record,size_t bytes,uint32_t offset,int origin,const FS15_FileServices* services){
    if(!record)return 0;
    if(!valid(record,bytes)||!services||!services->seek||!services->size)return UINT32_MAX;
    auto* p=static_cast<unsigned char*>(record);
    auto position=get(p,0x8050);
    if(position==UINT32_MAX)position=fs15_file_tell(record,bytes,services);
    if(origin==1)offset+=position;
    else if(origin==2)offset=fs15_file_size(record,bytes,services)-offset;
    else if(origin!=0)return UINT32_MAX;
    if(get(p,0x8050)==UINT32_MAX){fs15_file_seek(record,bytes,offset,services);}
    else{
        const auto cursor=get(p,0x8048),base=position-cursor;
        if(offset<base+get(p,0x804c)&&base<=offset){put(p,0x8050,offset);put(p,0x8048,cursor-position+offset);}
        else{put(p,0x8048,0);put(p,0x804c,0);fs15_file_seek(record,bytes,offset,services);put(p,0x8050,offset);}
    }
    return 0;
}
uint32_t fs15_buffered_file_status(const void* record,size_t bytes){
    if(!record||bytes<FS15_BUFFERED_FILE_BYTES)return UINT32_MAX;
    const auto* p=static_cast<const unsigned char*>(record);uint64_t handle;std::memcpy(&handle,p+8,8);
    return p[0x8054]||handle==UINT64_MAX?UINT32_MAX:0;
}
