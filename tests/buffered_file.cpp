#include "buffered_file.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>
namespace {
void check(bool ok){if(!ok)throw std::runtime_error("Buffered file assertion failed");}
uint32_t get(const void* p,size_t offset){uint32_t n;std::memcpy(&n,static_cast<const char*>(p)+offset,4);return n;}
void put(void* p,size_t offset,uint32_t n){std::memcpy(static_cast<char*>(p)+offset,&n,4);}
struct IO{std::vector<unsigned char> data;uint32_t position=0,reads=0,seeks=0;bool error=false;};
uint32_t read(void* user,uint64_t,void* output,uint32_t count,uint32_t* actual){
    auto& io=*static_cast<IO*>(user);++io.reads;if(io.error)return 6;
    *actual=io.position>=io.data.size()?0:static_cast<uint32_t>(std::min<size_t>(count,io.data.size()-io.position));
    if(*actual)std::memcpy(output,io.data.data()+io.position,*actual);
    io.position+=*actual;return 0;
}
uint32_t seek(void* user,uint64_t,int32_t offset,uint32_t origin){auto& io=*static_cast<IO*>(user);++io.seeks;if(offset<0)return UINT32_MAX;if(!origin)io.position=static_cast<uint32_t>(offset);return io.position;}
uint32_t size(void* user,uint64_t){return static_cast<uint32_t>(static_cast<IO*>(user)->data.size());}
}
int main(){try{
    IO io;io.data.resize(65549);for(size_t i=0;i<io.data.size();++i)io.data[i]=static_cast<unsigned char>(i);
    FS15_FileServices service{&io,read,seek,size,nullptr};std::array<unsigned char,FS15_BUFFERED_FILE_BYTES> record{};
    const uint64_t handle=1;std::memcpy(record.data()+8,&handle,8);std::vector<unsigned char> output(70000);
    auto take=[&](uint32_t n){return fs15_buffered_file_read(record.data(),record.size(),output.data(),output.size(),n,&service);};
    auto jump=[&](uint32_t n,int origin){return fs15_buffered_file_seek(record.data(),record.size(),n,origin,&service);};
    check(take(10)==10&&io.reads==1&&get(record.data(),0x8050)==10&&get(record.data(),0x804c)==32768);
    check(jump(0,0)==0&&io.seeks==0&&take(10)==10&&io.reads==1&&output[9]==9);
    check(jump(32768,0)==0&&io.seeks==1&&get(record.data(),0x804c)==0);
    check(take(32770)==32770&&io.reads==3&&get(record.data(),0x8048)==2&&get(record.data(),0x804c)==13);
    check(take(100)==11&&io.reads==4&&fs15_buffered_file_status(record.data(),record.size())==0);
    check(jump(0,1)==0&&io.position==io.data.size());
    check(jump(7,2)==0&&take(20)==7&&output[0]==io.data[io.data.size()-7]);
    const auto before=record;check(jump(0,7)==UINT32_MAX&&record==before);
    check(jump(0x80000000u,0)==0&&get(record.data(),0x8050)==0x80000000u);
    check(jump(0,0)==0);io.error=true;check(take(1)==0&&record[0x8054]==1&&fs15_buffered_file_status(record.data(),record.size())==UINT32_MAX);
    io.error=false;check(jump(0,0)==0&&take(1)==1&&record[0x8054]==1);
    const auto stable=record;
    check(fs15_buffered_file_read(record.data(),record.size(),output.data(),0,1,&service)==0&&record==stable);
    put(record.data(),0x804c,32769);const auto malformed=record;
    check(take(1)==0&&record==malformed);
    check(fs15_buffered_file_read(nullptr,0,nullptr,0,1,&service)==0);
    check(fs15_buffered_file_seek(nullptr,0,0,0,&service)==0);
    check(fs15_buffered_file_status(nullptr,0)==UINT32_MAX);
    std::cout<<"Native buffered read/seek/status checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
