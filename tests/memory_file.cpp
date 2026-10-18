#include "memory_file.h"
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
void check(bool ok){if(!ok)throw std::runtime_error("Memory file assertion failed");}
void put(void* p,size_t at,uint32_t v){std::memcpy(static_cast<unsigned char*>(p)+at,&v,4);}
int main(){try{
    std::array<unsigned char,0x20> record;record.fill(0xa5);
    const std::array<unsigned char,6> input{0,1,0,255,42,128};unsigned char out[10]{};
    uint64_t pointer=reinterpret_cast<uintptr_t>(input.data());std::memcpy(record.data()+8,&pointer,8);
    put(record.data(),0x14,6);put(record.data(),0x18,0);const auto initial=record;
    check(fs15_memory_file_valid(record.data(),record.size())&&fs15_memory_file_size(record.data(),record.size())==6);
    check(fs15_memory_file_read(record.data(),record.size(),input.size(),out,sizeof(out),3)==3&&out[2]==0);
    check(fs15_memory_file_tell(record.data(),record.size())==3&&!fs15_memory_file_eof(record.data(),record.size()));
    check(fs15_memory_file_read(record.data(),record.size(),input.size(),out,sizeof(out),10)==3&&out[0]==255&&out[2]==128);
    check(fs15_memory_file_eof(record.data(),record.size()));
    check(fs15_memory_file_read(record.data(),record.size(),input.size(),nullptr,0,0)==0);
    check(fs15_memory_file_seek(record.data(),record.size(),UINT32_MAX)&&fs15_memory_file_tell(record.data(),record.size())==6);
    check(fs15_memory_file_seek(record.data(),record.size(),1)&&!fs15_memory_file_eof(record.data(),record.size()));
    auto before=record;
    check(fs15_memory_file_read(record.data(),record.size(),5,out,sizeof(out),1)==UINT32_MAX&&record==before);
    check(fs15_memory_file_read(record.data(),record.size(),input.size(),out,1,2)==UINT32_MAX&&record==before);
    check(fs15_memory_file_read(record.data(),record.size(),input.size(),out,sizeof(out),UINT32_MAX)==UINT32_MAX&&record==before);
    put(record.data(),0x18,7);before=record;
    check(fs15_memory_file_read(record.data(),record.size(),input.size(),out,sizeof(out),0)==UINT32_MAX&&record==before);
    check(fs15_memory_file_eof(record.data(),record.size()));
    pointer=0;std::memcpy(record.data()+8,&pointer,8);
    check(!fs15_memory_file_valid(record.data(),record.size())&&fs15_memory_file_tell(record.data(),record.size())==UINT32_MAX);
    check(fs15_memory_file_size(record.data(),record.size())==6&&fs15_memory_file_seek(record.data(),record.size(),2));
    check(fs15_memory_file_read(record.data(),record.size(),input.size(),out,sizeof(out),1)==UINT32_MAX);
    for(size_t i=0;i<record.size();++i)if(!(i>=8&&i<16)&&!(i>=0x18&&i<0x1c))check(record[i]==initial[i]);
    check(!fs15_memory_file_seek(nullptr,0,0)&&!fs15_memory_file_valid(nullptr,0));
    check(fs15_memory_file_read(nullptr,0,0,nullptr,0,0)==UINT32_MAX);
    std::cout<<"Recovered memory-file validity/read/seek/size/position/EOF checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
