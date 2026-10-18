#include "file_stream.h"
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
void check(bool result){if(!result)throw std::runtime_error("Recovered file stream assertion failed");}
struct IO {
    uint32_t actual=0,error=0,position=0,size=0,calls=0,corruptions=0,origin=0;
    int32_t distance=0;
    uint64_t handle=0;
};
uint32_t read(void* user,uint64_t h,void*,uint32_t,uint32_t* count){auto& s=*static_cast<IO*>(user);s.handle=h;++s.calls;*count=s.actual;return s.error;}
uint32_t seek(void* user,uint64_t h,int32_t distance,uint32_t origin){auto& s=*static_cast<IO*>(user);s.handle=h;s.distance=distance;s.origin=origin;++s.calls;return s.position;}
uint32_t size(void* user,uint64_t h){auto& s=*static_cast<IO*>(user);s.handle=h;++s.calls;return s.size;}
void corrupt(void* user,const void*,uint32_t){++static_cast<IO*>(user)->corruptions;}
}
int main(){try{
    IO io;FS15_FileServices service{&io,read,seek,size,corrupt};
    std::array<unsigned char,0x38> record;record.fill(0xa5);
    const uint64_t handle=0x1122334455667788;std::memcpy(record.data()+8,&handle,8);
    const auto initial=record;char output[32]{};
    io.actual=10;check(fs15_file_read(record.data(),record.size(),output,10,&service)==10&&record[0x10]==0&&io.handle==handle);
    io.actual=3;check(fs15_file_read(record.data(),record.size(),output,10,&service)==3&&record[0x10]==1);
    io.actual=0;check(fs15_file_read(record.data(),record.size(),nullptr,0,&service)==0&&record[0x10]==0);
    for(auto error:{6u,31u,0x570u,0x571u}){io.error=error;record[0x10]=0xaa;check(fs15_file_read(record.data(),record.size(),output,10,&service)==UINT32_MAX&&record[0x10]==0);}
    check(io.corruptions==2);io.error=0;
    io.position=50;check(fs15_file_seek(record.data(),record.size(),100,&service)&&record[0x10]==1&&io.distance==100&&io.origin==0);
    io.position=100;check(fs15_file_seek(record.data(),record.size(),100,&service)&&record[0x10]==0);
    io.position=UINT32_MAX;record[0x10]=0x73;
    check(!fs15_file_seek(record.data(),record.size(),0x80000000,&service)&&io.distance==INT32_MIN&&record[0x10]==0x73);
    check(!fs15_file_seek(record.data(),record.size(),UINT32_MAX,&service)&&io.distance==-1);
    io.position=10;check(fs15_file_tell(record.data(),record.size(),&service)==10&&io.origin==1&&io.distance==0&&record[0x10]==0x73);
    io.size=0x80000001;check(fs15_file_size(record.data(),record.size(),&service)==0x80000001);
    for(size_t i=0;i<record.size();++i)if(i!=0x10)check(record[i]==initial[i]);
    const uint64_t invalid=UINT64_MAX;std::memcpy(record.data()+8,&invalid,8);const auto before=record;const auto calls=io.calls;
    check(fs15_file_read(record.data(),record.size(),output,1,&service)==UINT32_MAX);
    check(!fs15_file_seek(record.data(),record.size(),0,&service));
    check(fs15_file_size(record.data(),record.size(),&service)==UINT32_MAX&&fs15_file_tell(record.data(),record.size(),&service)==UINT32_MAX);
    check(record==before&&calls==io.calls);
    check(fs15_file_read(nullptr,0,nullptr,0,&service)==UINT32_MAX);
    std::memcpy(record.data()+8,&handle,8);
    check(fs15_file_read(record.data(),0x10,output,1,&service)==UINT32_MAX);
    check(fs15_file_read(record.data(),record.size(),nullptr,1,&service)==UINT32_MAX);
    check(!fs15_file_seek(record.data(),record.size(),0,nullptr));
    std::cout<<"Recovered file read/seek/size/tell checks passed\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
