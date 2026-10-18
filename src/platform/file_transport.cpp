#include "file_transport.h"
#include <cstring>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace fs15platform {
namespace {
uint64_t token(const std::array<unsigned char,0x38>& record){uint64_t result;std::memcpy(&result,record.data()+8,8);return result;}
uint32_t read_os(void*,uint64_t file,void* output,uint32_t count,uint32_t* consumed){
#ifdef _WIN32
    DWORD actual=0;
    const auto ok=ReadFile(reinterpret_cast<HANDLE>(static_cast<uintptr_t>(file)),output,count,&actual,nullptr);
    *consumed=actual;
    return ok?0:GetLastError();
#else
    ssize_t actual;
    do{actual=::read(static_cast<int>(file),output,count);}while(actual<0&&errno==EINTR);
    if(actual<0)return errno==EBADF?6:31;
    *consumed=static_cast<uint32_t>(actual);return 0;
#endif
}
uint32_t seek_os(void*,uint64_t file,int32_t distance,uint32_t origin){
#ifdef _WIN32
    return SetFilePointer(reinterpret_cast<HANDLE>(static_cast<uintptr_t>(file)),distance,nullptr,origin);
#else
    const auto result=::lseek(static_cast<int>(file),distance,origin==0?SEEK_SET:SEEK_CUR);
    return result<0?UINT32_MAX:static_cast<uint32_t>(static_cast<uint64_t>(result));
#endif
}
uint32_t size_os(void*,uint64_t file){
#ifdef _WIN32
    return GetFileSize(reinterpret_cast<HANDLE>(static_cast<uintptr_t>(file)),nullptr);
#else
    struct stat info{};
    return ::fstat(static_cast<int>(file),&info)!=0?UINT32_MAX:static_cast<uint32_t>(static_cast<uint64_t>(info.st_size));
#endif
}
}
ReadOnlyFile::ReadOnlyFile(const std::filesystem::path& path){
    uint64_t file=UINT64_MAX;
#ifdef _WIN32
    const auto opened=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(opened==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot open resource: "+path.u8string());
    if(GetFileType(opened)!=FILE_TYPE_DISK){CloseHandle(opened);throw std::runtime_error("Resource is not a disk file");}
    file=reinterpret_cast<uintptr_t>(opened);
#else
    const int opened=::open(path.c_str(),O_RDONLY|O_CLOEXEC|O_NONBLOCK);
    if(opened<0)throw std::runtime_error("Cannot open resource: "+path.u8string());
    struct stat info{};
    if(::fstat(opened,&info)!=0||!S_ISREG(info.st_mode)){::close(opened);throw std::runtime_error("Resource is not a regular file");}
    file=static_cast<uint64_t>(opened);
#endif
    std::memcpy(record_.data()+8,&file,8);
    services_={nullptr,read_os,seek_os,size_os,nullptr};
}
ReadOnlyFile::~ReadOnlyFile(){
#ifdef _WIN32
    CloseHandle(reinterpret_cast<HANDLE>(static_cast<uintptr_t>(token(record_))));
#else
    ::close(static_cast<int>(token(record_)));
#endif
}
uint32_t ReadOnlyFile::read(void* output,uint32_t count){return fs15_file_read(record_.data(),record_.size(),output,count,&services_);}
bool ReadOnlyFile::seek(uint32_t position){return fs15_file_seek(record_.data(),record_.size(),position,&services_)!=0;}
uint32_t ReadOnlyFile::size() const{return fs15_file_size(record_.data(),record_.size(),&services_);}
uint32_t ReadOnlyFile::tell() const{return fs15_file_tell(record_.data(),record_.size(),&services_);}
uint64_t ReadOnlyFile::full_size() const{
#ifdef _WIN32
    LARGE_INTEGER value{};
    if(!GetFileSizeEx(reinterpret_cast<HANDLE>(static_cast<uintptr_t>(token(record_))),&value)||value.QuadPart<0)throw std::runtime_error("Cannot measure resource");
    return static_cast<uint64_t>(value.QuadPart);
#else
    struct stat value{};
    if(::fstat(static_cast<int>(token(record_)),&value)!=0||value.st_size<0)throw std::runtime_error("Cannot measure resource");
    return static_cast<uint64_t>(value.st_size);
#endif
}
std::vector<uint8_t> read_file_bounded(const std::filesystem::path& path,uint64_t maximum){
    BufferedReadOnlyFile file(path);
    const auto size=file.full_size();
    if(size>maximum||size>=UINT32_MAX)throw std::runtime_error("Resource exceeds bounded stream size: "+path.u8string());
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    if(file.read(bytes.data(),static_cast<uint32_t>(size))!=size)throw std::runtime_error("Resource changed or read failed: "+path.u8string());
    return bytes;
}
BufferedReadOnlyFile::BufferedReadOnlyFile(const std::filesystem::path& path):file_(path){
    std::memcpy(record_.data(),file_.record_.data(),file_.record_.size());
}
uint32_t BufferedReadOnlyFile::read(void* output,uint32_t count){return fs15_buffered_file_read(record_.data(),record_.size(),output,count,count,&file_.services_);}
uint32_t BufferedReadOnlyFile::seek(uint32_t offset,int origin){return fs15_buffered_file_seek(record_.data(),record_.size(),offset,origin,&file_.services_);}
uint32_t BufferedReadOnlyFile::status() const{return fs15_buffered_file_status(record_.data(),record_.size());}
}
