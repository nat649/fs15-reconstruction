#pragma once
// Authored POSIX transport for original GE6's synchronous startup file calls.
// No scene/script/archive parser or replacement game filesystem is implemented.
#include <algorithm>
#include <array>
#include <codecvt>
#include <cstdint>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <glob.h>
#include <locale>
#include <map>
#include <memory>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
namespace ge6_fs {
#define GE6_FS_MS __attribute__((ms_abi))
inline void(*trace)(const char*)=nullptr;
inline void(*set_error)(uint32_t)=nullptr;
inline uint32_t win_error(int value){switch(value){case ENOENT:return 2;case ENOTDIR:return 3;case EACCES:case EPERM:case EISDIR:return 5;case EBADF:return 6;case EEXIST:return 183;case ENAMETOOLONG:return 206;case ENOSPC:return 112;case EINVAL:return 87;default:return 31;}}
inline void error(){set_error(win_error(errno));}
inline std::u16string utf16(const std::string& value){std::wstring_convert<std::codecvt_utf8_utf16<char16_t>,char16_t> converter;return converter.from_bytes(value);}
inline std::string utf8(const uint16_t* value,size_t count){std::u16string staged;staged.reserve(count);for(size_t i=0;i<count;++i)staged.push_back(char16_t(value[i]));std::wstring_convert<std::codecvt_utf8_utf16<char16_t>,char16_t> converter;return converter.to_bytes(staged);}
inline std::string path(const uint16_t* value){size_t size=0;while(size<32768&&value[size])++size;if(size==32768)throw std::runtime_error("Unterminated native filename");auto result=utf8(value,size);std::replace(result.begin(),result.end(),'\\','/');if(result.size()>1&&result[1]==':')throw std::runtime_error("Windows drive namespace is not mapped");return result;}
struct File {int fd;};
struct Search {std::vector<std::string> names;size_t index=0;};
inline std::map<uintptr_t,std::unique_ptr<File>> files;
inline std::map<uintptr_t,std::unique_ptr<Search>> searches;
inline int fd(uintptr_t h){if(h>=0x100&&h<=0x102)return int(h-0x100);auto found=files.find(h);return found==files.end()?-1:found->second->fd;}
inline GE6_FS_MS int directory(const uint16_t* name,void* security){trace("CreateDirectoryW");if(security)throw std::runtime_error("Directory security descriptors are unsupported");if(mkdir(path(name).c_str(),0755)==0)return 1;error();return 0;}
inline uint64_t file_time(const timespec& value){return (uint64_t(value.tv_sec)+11644473600ull)*10000000ull+uint64_t(value.tv_nsec)/100;}
inline bool find_data(const std::string& name,void* raw){
    struct stat value{};if(stat(name.c_str(),&value)){error();return false;}auto* out=static_cast<uint8_t*>(raw);std::memset(out,0,592);
    uint32_t attributes=S_ISDIR(value.st_mode)?16:32;if(!(value.st_mode&S_IWUSR))attributes|=1;std::memcpy(out,&attributes,4);
    const uint64_t times[]{0,file_time(value.st_atim),file_time(value.st_mtim)};std::memcpy(out+4,times,sizeof(times));
    auto size=uint64_t(value.st_size);uint32_t high=uint32_t(size>>32),low=uint32_t(size);std::memcpy(out+28,&high,4);std::memcpy(out+32,&low,4);
    auto last=name.find_last_of('/');auto leaf=last==std::string::npos?name:name.substr(last+1);auto wide=utf16(leaf);if(wide.size()>259){set_error(206);return false;}std::memcpy(out+44,wide.data(),wide.size()*2);return true;
}
inline GE6_FS_MS uintptr_t find_first(const uint16_t* name,void* out){
    trace("FindFirstFileW");glob_t matches{};int result=glob(path(name).c_str(),GLOB_NOSORT,nullptr,&matches);if(result){globfree(&matches);set_error(result==GLOB_NOMATCH?2:31);return ~uintptr_t(0);}
    auto record=std::make_unique<Search>();for(size_t i=0;i<matches.gl_pathc;++i)record->names.emplace_back(matches.gl_pathv[i]);globfree(&matches);
    if(record->names.empty()||!find_data(record->names[0],out))return ~uintptr_t(0);
    auto handle=reinterpret_cast<uintptr_t>(record.get());searches.emplace(handle,std::move(record));return handle;
}
inline GE6_FS_MS int find_next(uintptr_t handle,void* out){trace("FindNextFileW");auto found=searches.find(handle);if(found==searches.end()){set_error(6);return 0;}auto& record=*found->second;if(++record.index>=record.names.size()){set_error(18);return 0;}return find_data(record.names[record.index],out)?1:0;}
inline GE6_FS_MS int find_close(uintptr_t handle){trace("FindClose");if(searches.erase(handle))return 1;set_error(6);return 0;}
inline GE6_FS_MS uintptr_t create(const uint16_t* name,uint32_t access,uint32_t share,void* security,uint32_t disposition,uint32_t flags,uintptr_t template_file){
    trace("CreateFileW");if(security||template_file||share>7||(flags&0x40000000u)||(access&~0xc0000004u))throw std::runtime_error("Unsupported synchronous file-open contract");
    int mode=(access&0x40000000u)?((access&0x80000000u)?O_RDWR:O_WRONLY):O_RDONLY;if(access&4)mode=O_WRONLY|O_APPEND;
    switch(disposition){case 1:mode|=O_CREAT|O_EXCL;break;case 2:mode|=O_CREAT|O_TRUNC;break;case 3:break;case 4:mode|=O_CREAT;break;case 5:mode|=O_TRUNC;break;default:throw std::runtime_error("Unsupported file creation disposition");}
    int opened=open(path(name).c_str(),mode|O_CLOEXEC,0666);if(opened<0){error();return ~uintptr_t(0);}auto record=std::make_unique<File>();record->fd=opened;auto handle=reinterpret_cast<uintptr_t>(record.get());files.emplace(handle,std::move(record));return handle;
}
inline GE6_FS_MS int read(uintptr_t handle,void* buffer,uint32_t count,uint32_t* done,void* overlapped){trace("ReadFile");if(overlapped)throw std::runtime_error("Overlapped read unsupported");if(done)*done=0;ssize_t size;do{size=::read(fd(handle),buffer,count);}while(size<0&&errno==EINTR);if(size<0){error();return 0;}if(done)*done=uint32_t(size);return 1;}
inline GE6_FS_MS int write(uintptr_t handle,const void* buffer,uint32_t count,uint32_t* done,void* overlapped){trace("WriteFile");if(overlapped)throw std::runtime_error("Overlapped write unsupported");if(done)*done=0;ssize_t size;do{size=::write(fd(handle),buffer,count);}while(size<0&&errno==EINTR);if(size<0){error();return 0;}if(done)*done=uint32_t(size);return 1;}
inline GE6_FS_MS int close(uintptr_t handle){trace("CloseHandle");auto found=files.find(handle);if(found==files.end()){set_error(6);return 0;}int result=::close(found->second->fd);files.erase(found);if(result){error();return 0;}return 1;}
inline GE6_FS_MS uint32_t type(uintptr_t handle){trace("GetFileType");struct stat value{};if(fstat(fd(handle),&value)){error();return 0;}if(S_ISCHR(value.st_mode))return 2;if(S_ISFIFO(value.st_mode)||S_ISSOCK(value.st_mode))return 3;return 1;}
inline GE6_FS_MS int size_ex(uintptr_t handle,int64_t* out){trace("GetFileSizeEx");struct stat value{};if(fstat(fd(handle),&value)){error();return 0;}*out=value.st_size;return 1;}
inline GE6_FS_MS int times(uintptr_t handle,void* creation,void* access,void* modified){trace("GetFileTime");struct stat value{};int descriptor=fd(handle);if(fstat(descriptor,&value)){error();return 0;}uint64_t born=0;
    struct statx extra{};if(::statx(descriptor,"",AT_EMPTY_PATH,STATX_BTIME,&extra)==0&&(extra.stx_mask&STATX_BTIME)){timespec stamp{extra.stx_btime.tv_sec,long(extra.stx_btime.tv_nsec)};born=file_time(stamp);}
    const auto at=file_time(value.st_atim),mt=file_time(value.st_mtim);if(creation)std::memcpy(creation,&born,8);if(access)std::memcpy(access,&at,8);if(modified)std::memcpy(modified,&mt,8);return 1;}
inline GE6_FS_MS uint32_t size(uintptr_t handle,uint32_t* high){int64_t total=0;if(!size_ex(handle,&total))return 0xffffffffu;if(high)*high=uint32_t(uint64_t(total)>>32);return uint32_t(total);}
inline GE6_FS_MS int seek_ex(uintptr_t handle,int64_t offset,int64_t* result,uint32_t mode){trace("SetFilePointerEx");if(mode>2){set_error(87);return 0;}auto position=lseek(fd(handle),offset,int(mode));if(position<0){error();return 0;}if(result)*result=position;return 1;}
inline GE6_FS_MS uint32_t seek(uintptr_t handle,int32_t low,int32_t* high,uint32_t mode){int64_t offset=high?int64_t((uint64_t(uint32_t(*high))<<32)|uint32_t(low)):int64_t(low),position=0;if(!seek_ex(handle,offset,&position,mode))return 0xffffffffu;if(high)*high=int32_t(uint64_t(position)>>32);set_error(0);return uint32_t(position);}
inline GE6_FS_MS int copy(const uint16_t* source,const uint16_t* destination,int fail_exists){
    trace("CopyFileW");int input=open(path(source).c_str(),O_RDONLY|O_CLOEXEC);if(input<0){error();return 0;}int output=open(path(destination).c_str(),O_WRONLY|O_CREAT|O_CLOEXEC|(fail_exists?O_EXCL:O_TRUNC),0666);if(output<0){error();::close(input);return 0;}
    bool ok=true;std::array<uint8_t,65536> buffer{};for(;;){ssize_t n=::read(input,buffer.data(),buffer.size());if(n==0)break;if(n<0){if(errno==EINTR)continue;error();ok=false;break;}size_t sent=0;while(sent<size_t(n)){auto written=::write(output,buffer.data()+sent,size_t(n)-sent);if(written<0&&errno==EINTR)continue;if(written<=0){error();ok=false;break;}sent+=size_t(written);}if(!ok)break;}
    if(ok){struct stat info{};if(fstat(input,&info)||fchmod(output,info.st_mode&0777)){error();ok=false;}else{const timespec stamps[]{info.st_atim,info.st_mtim};if(futimens(output,stamps)){error();ok=false;}}}
    ::close(input);if(::close(output)){error();ok=false;}return ok?1:0;
}
inline uintptr_t resolve(const std::string& name){
#define FS_BIND(n,f) if(name==n)return reinterpret_cast<uintptr_t>(&f)
    FS_BIND("CreateDirectoryW",directory);FS_BIND("FindFirstFileW",find_first);FS_BIND("FindNextFileW",find_next);FS_BIND("FindClose",find_close);
    FS_BIND("CreateFileW",create);FS_BIND("ReadFile",read);FS_BIND("WriteFile",write);FS_BIND("GetFileSizeEx",size_ex);FS_BIND("GetFileSize",size);FS_BIND("SetFilePointerEx",seek_ex);FS_BIND("SetFilePointer",seek);FS_BIND("CopyFileW",copy);
    FS_BIND("GetFileTime",times);
#undef FS_BIND
    return 0;
}
#undef GE6_FS_MS
}
