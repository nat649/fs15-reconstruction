#pragma once
#include "../core/memory_file.h"
#include <array>
#include <stdexcept>
#include <cstring>
namespace fs15platform {
// Authored borrowed-storage initialization, not the original owning factory.
// The source storage must outlive this view; it is never released or modified.
class ReadOnlyMemoryFile {
public:
    ReadOnlyMemoryFile(const void* source,size_t bytes):capacity_(bytes){
        if(bytes>UINT32_MAX||(!source&&bytes))throw std::runtime_error("Invalid memory-file extent");
        const uint64_t pointer=reinterpret_cast<uintptr_t>(source?source:&empty_);
        const auto length=static_cast<uint32_t>(bytes);
        std::memcpy(record_.data()+8,&pointer,8);std::memcpy(record_.data()+0x14,&length,4);
    }
    uint32_t read(void* destination,size_t capacity,uint32_t count){return fs15_memory_file_read(record_.data(),record_.size(),capacity_,destination,capacity,count);}
    bool seek(uint32_t offset){return fs15_memory_file_seek(record_.data(),record_.size(),offset)!=0;}
    uint32_t size()const{return fs15_memory_file_size(record_.data(),record_.size());}
    uint32_t tell()const{return fs15_memory_file_tell(record_.data(),record_.size());}
    bool eof()const{return fs15_memory_file_eof(record_.data(),record_.size())!=0;}
    bool valid()const{return fs15_memory_file_valid(record_.data(),record_.size())!=0;}
    ReadOnlyMemoryFile(const ReadOnlyMemoryFile&)=delete;
    ReadOnlyMemoryFile& operator=(const ReadOnlyMemoryFile&)=delete;
private:
    std::array<unsigned char,0x20> record_{};
    unsigned char empty_=0;
    size_t capacity_;
};
}
