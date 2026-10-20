// Linux x64 transport for the original, privately supplied GE6 machine code.
// No recovered C++ renderer/physics/game implementation is linked here.
// Unimplemented imports terminate with their name and original return address.
#include <openssl/sha.h>
#include "ansi_reference.h"
#include "local_platform.h"
#include "file_services.h"
#include "network_services.h"
#include "startup_services.h"
#include "system_services.h"
#include "event_services.h"
#include "virtual_memory.h"
#include "thread_services.h"
#include "audio_services.h"
#include "audio_check.h"
#include "console_check.h"
#include "window_resource_check.h"
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <asm/prctl.h>
#include <pthread.h>
#include <malloc.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <ucontext.h>
#include <array>
#include <atomic>
#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#define MS __attribute__((ms_abi))
namespace {
constexpr uintptr_t base=0x140000000;
constexpr char expected[]="6d1beaf9d7e3c57dd51cec03385a53bda82b4ac946e622d8d2fa592a56d71f9f";
constexpr char local_expected[]="e8a9f3b4397fb9aca31fdcb7471728f4f6ef7940a85911f1f727dbe09377a955";
std::string image_kind="fixture";
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
template<class T>T read(const std::vector<uint8_t>& bytes,size_t offset){require(offset<=bytes.size()&&sizeof(T)<=bytes.size()-offset,"Truncated PE field");T out;std::memcpy(&out,bytes.data()+offset,sizeof(T));return out;}
std::vector<std::string> imports;
std::atomic<uint64_t> calls{0};
std::mutex trace_mutex;
std::atomic<uintptr_t> registered_filter{0};
bool main_reached=false;
struct Singleton {int fd;};
std::map<uintptr_t,std::unique_ptr<Singleton>> singleton_handles;
thread_local uint32_t last_error=0;
std::mutex locks_mutex;
std::recursive_mutex runtime_registry_mutex;
std::map<void*,std::shared_ptr<std::recursive_mutex>> locks;
int heap_token=0;
constexpr uintptr_t kernel_token=0x47453601;
constexpr uintptr_t nt_token=0x47453602;
constexpr uintptr_t winmm_token=0x47453603;
std::map<uintptr_t,uint32_t> explicit_module_refs;
std::string command_line="FarmingSimulator2015Game.exe";
std::string image_path;
void trace(const char* name){std::lock_guard<std::mutex> guard(trace_mutex);++calls;std::cout<<"GE6_OS "<<name<<'\n';if(ge6_threads::active_entry&&!ge6_threads::traced_import){ge6_threads::traced_import=true;std::cout<<"GE6_NATIVE_THREAD_FIRST_OS name="<<name<<" entry=0x"<<std::hex<<ge6_threads::active_entry<<std::dec<<" tid="<<syscall(SYS_gettid)<<'\n';}}
[[noreturn]] MS void missing(uint32_t id){
    std::lock_guard<std::mutex> guard(trace_mutex);
    std::cerr<<"GE6_BLOCKED_IMPORT "<<(id<imports.size()?imports[id]:"invalid-index")
             <<" caller=0x"<<std::hex<<reinterpret_cast<uintptr_t>(__builtin_return_address(0))
             <<std::dec<<" os_calls="<<calls<<'\n';std::_Exit(78);
}
[[noreturn]] void window_unsupported(const char* name,uintptr_t caller){std::lock_guard<std::mutex> guard(trace_mutex);std::cerr<<"GE6_BLOCKED_IMPORT USER32.dll!"<<name<<" caller=0x"<<std::hex<<caller<<std::dec<<" os_calls="<<calls<<'\n';std::_Exit(78);}
void fault(int signal,siginfo_t* info,void* raw){
    auto* context=static_cast<ucontext_t*>(raw);char message[192];
    int count=std::snprintf(message,sizeof(message),"GE6_NATIVE_FAULT signal=%d rip=0x%llx address=0x%llx\n",signal,
        static_cast<unsigned long long>(context->uc_mcontext.gregs[REG_RIP]),static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(info->si_addr)));
    if(count>0){auto written=write(STDERR_FILENO,message,std::min(size_t(count),sizeof(message)-1));(void)written;}
    _exit(128+signal);
}
MS void file_time(uint64_t* out){trace("GetSystemTimeAsFileTime");timespec t{};clock_gettime(CLOCK_REALTIME,&t);*out=(uint64_t(t.tv_sec)+11644473600ull)*10000000ull+uint64_t(t.tv_nsec)/100;}
MS uint32_t process_id(){trace("GetCurrentProcessId");return uint32_t(getpid());}
MS uint32_t thread_id(){trace("GetCurrentThreadId");return uint32_t(syscall(SYS_gettid));}
MS uint32_t ticks(){trace("GetTickCount");timespec t{};clock_gettime(CLOCK_MONOTONIC,&t);return uint32_t(uint64_t(t.tv_sec)*1000+uint64_t(t.tv_nsec)/1000000);}
MS int counter(int64_t* out){trace("QueryPerformanceCounter");auto caller=reinterpret_cast<uintptr_t>(__builtin_return_address(0));if(!main_reached&&caller>=base+0xc9a0&&caller<base+0xc9a0+4364){main_reached=true;std::cout<<"GE6_ORIGINAL_MAIN_REACHED caller=0x"<<std::hex<<caller<<std::dec<<'\n';}timespec t{};clock_gettime(CLOCK_MONOTONIC,&t);*out=int64_t(t.tv_sec)*1000000000+t.tv_nsec;return 1;}
MS int frequency(int64_t* out){trace("QueryPerformanceFrequency");*out=1000000000;return 1;}
MS uint32_t get_error(){return last_error;}
MS void set_error(uint32_t value){last_error=value;}
MS uintptr_t encode(uintptr_t p){return p^0x6e61746976654745ull;}
MS void* heap_create(uint32_t flags,size_t initial,size_t maximum){trace("HeapCreate");require(flags==0&&maximum==0&&initial<=0x100000,"Unsupported CRT heap flags/limits");return &heap_token;}
MS void* process_heap(){return &heap_token;}
MS void* heap_alloc(void* heap,uint32_t flags,size_t size){require(heap==&heap_token&&(flags&~8u)==0,"Unsupported heap allocation");return (flags&8)?std::calloc(1,size):std::malloc(size);}
MS int heap_free(void* heap,uint32_t flags,void* p){require(heap==&heap_token&&flags==0,"Unsupported heap free");std::free(p);return 1;}
MS size_t heap_size(void* heap,uint32_t flags,void* p){trace("HeapSize");require(heap==&heap_token&&flags==0,"Unsupported heap size query");if(!p){last_error=87;return ~size_t(0);}return malloc_usable_size(p);}
MS void* heap_realloc(void* heap,uint32_t flags,void* p,size_t size){trace("HeapReAlloc");require(heap==&heap_token&&(flags&~0x18u)==0,"Unsupported heap realloc");if(!p){last_error=87;return nullptr;}auto previous=malloc_usable_size(p);if(flags&0x10){if(size<=previous)return p;last_error=8;return nullptr;}auto* out=std::realloc(p,size);if(out&&(flags&8)&&size>previous)std::memset(static_cast<uint8_t*>(out)+previous,0,size-previous);return out;}
MS uint32_t version(){trace("GetVersion");return 0x00000006;}// Compatibility API level, not the Linux OS version.
MS int version_ex_a(void* raw){trace("GetVersionExA");if(!raw){last_error=87;return 0;}uint32_t size=0;std::memcpy(&size,raw,4);if(size!=148&&size!=156){last_error=87;return 0;}std::memset(raw,0,size);const uint32_t fields[]{size,6,0,0,2};std::memcpy(raw,fields,sizeof(fields));if(size==156)static_cast<uint8_t*>(raw)[154]=1;return 1;}// Same API compatibility level as GetVersion; no claim to be Windows.
MS int heap_info(void* heap,uint32_t kind,void*,size_t size){trace("HeapSetInformation");require(heap==&heap_token&&kind==0&&size==4,"Unsupported heap information");return 1;}
MS int init_lock(void* p,uint32_t){trace("InitializeCriticalSectionAndSpinCount");std::lock_guard<std::mutex> guard(locks_mutex);locks[p]=std::make_shared<std::recursive_mutex>();return 1;}
MS void initialize_lock(void* p){init_lock(p,0);}
std::shared_ptr<std::recursive_mutex> lock_record(void* p){std::lock_guard<std::mutex> guard(locks_mutex);auto found=locks.find(p);require(found!=locks.end(),"Uninitialized critical section");return found->second;}
MS void enter_lock(void* p){lock_record(p)->lock();}
MS void leave_lock(void* p){lock_record(p)->unlock();}
MS void delete_lock(void* p){std::lock_guard<std::mutex> guard(locks_mutex);require(locks.erase(p)==1,"Uninitialized critical section deletion");}
uintptr_t module(const std::string& name){auto lower=ge6_system::lower(name);if(lower=="kernel32.dll")return kernel_token;if(lower=="ntdll.dll")return nt_token;if(lower=="winmm.dll")return winmm_token;last_error=126;return 0;}
MS uintptr_t get_module_w(const uint16_t* name){trace("GetModuleHandleW");if(!name)return base;return module(ge6_fs::path(name));}
MS uintptr_t get_module_a(const char* name){trace("GetModuleHandleA");return name?module(name):base;}
MS uintptr_t load_library_a(const char* name){trace("LoadLibraryA");if(!name){last_error=87;return 0;}auto handle=module(name);if(handle){std::lock_guard<std::recursive_mutex> guard(runtime_registry_mutex);++explicit_module_refs[handle];}else std::cerr<<"GE6_MODULE_UNAVAILABLE "<<name<<'\n';return handle;}// Known authored API modules only; no Windows DLL code is loaded.
MS int free_library(uintptr_t handle){trace("FreeLibrary");std::lock_guard<std::recursive_mutex> guard(runtime_registry_mutex);auto found=explicit_module_refs.find(handle);if(found==explicit_module_refs.end()){last_error=6;return 0;}if(--found->second==0)explicit_module_refs.erase(found);return 1;}// Releases explicit references; imported ABI transport remains resident.
uintptr_t resolve(const std::string& library,const std::string& name);
MS uintptr_t get_proc(uintptr_t module,const char* name){trace("GetProcAddress");if(reinterpret_cast<uintptr_t>(name)<=65535){last_error=127;return 0;}uintptr_t result=0;if(module==kernel_token)result=resolve("KERNEL32.dll",name);else if(module==winmm_token)result=resolve("WINMM.dll",name);else if(module==nt_token&&std::string(name)=="NtAllocateVirtualMemory")result=reinterpret_cast<uintptr_t>(&ge6_vm::allocate);if(!result){last_error=module==kernel_token||module==nt_token||module==winmm_token||module==base?127:126;std::cerr<<"GE6_OPTIONAL_PROC_UNAVAILABLE "<<name<<'\n';}return result;}
MS char* get_command(){trace("GetCommandLineA");return command_line.data();}
MS uintptr_t standard_handle(int32_t id){require(id>=-12&&id<=-10,"Unknown standard handle");return uintptr_t(0x100+(-10-id));}
MS uint32_t file_type(uintptr_t handle){return ge6_fs::type(handle);}
MS uint32_t handle_count(uint32_t count){trace("SetHandleCount");return count;}// Documented NT compatibility no-op.
MS uint32_t ansi_page(){trace("GetACP");return 1252;}// Matches the local Windows reference's ANSI page.
MS int valid_page(uint32_t page){trace("IsValidCodePage");require(page==1252||page==65001,"Unimplemented code page validation");return 1;}
MS int page_info(uint32_t page,void* out){trace("GetCPInfo");require(page==0||page==1252||page==65001,"Unimplemented code page information");std::memset(out,0,20);uint32_t size=page==65001?4:1;std::memcpy(out,&size,4);static_cast<uint8_t*>(out)[4]='?';return 1;}
MS uint16_t* environment_w(){
    trace("GetEnvironmentStringsW");std::vector<uint16_t> out;
    for(char** entry=::environ;entry&&*entry;++entry){for(const unsigned char* p=reinterpret_cast<const unsigned char*>(*entry);*p;++p){if(*p>127){last_error=1113;return nullptr;}out.push_back(*p);}out.push_back(0);}
    if(out.empty())out.push_back(0);
    out.push_back(0);auto* result=static_cast<uint16_t*>(std::malloc(out.size()*2));if(result)std::memcpy(result,out.data(),out.size()*2);return result;
}
MS int free_environment_w(void* p){trace("FreeEnvironmentStringsW");std::free(p);return 1;}
MS uint32_t environment_a(const char* name,char* out,uint32_t capacity){trace("GetEnvironmentVariableA");if(!name||!*name||std::strchr(name,'=')||(!out&&capacity)){last_error=87;return 0;}const char* value=std::getenv(name);if(!value){last_error=203;return 0;}size_t size=std::strlen(value);for(size_t i=0;i<size;++i)if(static_cast<unsigned char>(value[i])>127){last_error=1113;return 0;}if(size>=capacity)return uint32_t(size+1);std::memcpy(out,value,size+1);return uint32_t(size);}// ASCII-bounded host environment, matching current CRT environment staging.
int byte_index(uint16_t unit){auto found=std::find(ge6_ansi::unicode.begin(),ge6_ansi::unicode.end(),unit);return found==ge6_ansi::unicode.end()?-1:int(found-ge6_ansi::unicode.begin());}
int wide_count(const uint16_t* source,int count){if(count==-1){count=0;while(count<1048576&&source[count])++count;++count;}require(count>0&&count<=1048576,"Invalid text count");return count;}
MS int widen(uint32_t page,uint32_t flags,const unsigned char* source,int count,uint16_t* out,int capacity){
    trace("MultiByteToWideChar");require(((page==0||page==1252)&&(flags==0||flags==1||flags==9))||(page==65001&&(flags==0||flags==8)),"Unimplemented multibyte page/flags");
    if(count==-1){count=0;while(count<1048576&&source[count])++count;++count;}require(count>0&&count<=1048576&&capacity>=0,"Invalid multibyte conversion bounds");
    std::u16string converted;try{if(page==65001)converted=ge6_fs::utf16(std::string(reinterpret_cast<const char*>(source),size_t(count)));else for(int i=0;i<count;++i)converted.push_back(char16_t(ge6_ansi::unicode[source[i]]));}catch(const std::range_error&){last_error=1113;return 0;}
    auto required=int(converted.size());if(capacity==0)return required;
    if(!out||capacity<required){last_error=122;return 0;}
    std::memcpy(out,converted.data(),converted.size()*2);return required;
}
MS int string_type(uint32_t kind,const uint16_t* source,int count,uint16_t* out){
    trace("GetStringTypeW");require(kind==1,"Unimplemented string classification kind");count=wide_count(source,count);
    for(int i=0;i<count;++i){int index=byte_index(source[i]);require(index>=0,"Unimplemented character outside CP1252");out[i]=ge6_ansi::ctype1[size_t(index)];}return 1;
}
MS int map_string(uint32_t locale,uint32_t flags,const uint16_t* source,int count,uint16_t* out,int capacity){
    trace("LCMapStringW");require((locale==0||locale==ge6_ansi::reference_lcid)&&(flags==0x100||flags==0x200),"Unimplemented case-map locale/flags");count=wide_count(source,count);require(capacity>=0,"Invalid case-map capacity");
    if(capacity==0)return count;
    if(!out||capacity<count){last_error=122;return 0;}
    const auto& table=flags==0x100?ge6_ansi::lower:ge6_ansi::upper;
    for(int i=0;i<count;++i){int index=byte_index(source[i]);require(index>=0,"Unimplemented case-map character outside CP1252");out[i]=table[size_t(index)];}return count;
}
MS int narrow(uint32_t page,uint32_t flags,const uint16_t* source,int count,char* out,int capacity,const char* fallback,int* used){
    trace("WideCharToMultiByte");require((page==0||page==1252||page==65001)&&flags==0&&!fallback,"Unimplemented codepage/conversion flags");
    count=wide_count(source,count);require(capacity>=0,"Invalid wide conversion capacity");std::vector<char> bytes;bytes.reserve(size_t(count));bool defaulted=false;
    if(page==65001){try{auto converted=ge6_fs::utf8(source,size_t(count));bytes.assign(converted.begin(),converted.end());}catch(const std::range_error&){last_error=1113;return 0;}}
    else
    for(int i=0;i<count;++i){int value=page==65001?(source[i]<128?int(source[i]):-1):byte_index(source[i]);
        if(value<0&&page!=65001){auto found=std::find(ge6_ansi::bestfit_unicode.begin(),ge6_ansi::bestfit_unicode.end(),source[i]);if(found!=ge6_ansi::bestfit_unicode.end()){auto index=size_t(found-ge6_ansi::bestfit_unicode.begin());value=ge6_ansi::bestfit_byte[index];defaulted=defaulted||ge6_ansi::bestfit_default[index];}}
        if(value<0){last_error=1113;return 0;}bytes.push_back(char(value));}// Only observed CP1252 case substitutions are supported.
    if(used)*used=defaulted?1:0;
    auto required=int(bytes.size());if(capacity==0)return required;
    if(!out||capacity<required){last_error=122;return 0;}
    std::memcpy(out,bytes.data(),bytes.size());
    return required;
}
MS uint32_t module_filename(uintptr_t module,char* out,uint32_t capacity){
    trace("GetModuleFileNameA");require(module==0||module==base,"Unimplemented module filename");
    if(!capacity){last_error=122;return 0;}
    auto size=std::min(image_path.size(),size_t(capacity-1));std::memcpy(out,image_path.data(),size);out[size]=0;
    if(image_path.size()>=capacity){last_error=122;return capacity;}return uint32_t(size);
}
MS uint32_t module_filename_w(uintptr_t module,uint16_t* out,uint32_t capacity){trace("GetModuleFileNameW");require(module==0||module==base,"Unimplemented module filename");if(!capacity){last_error=122;return 0;}auto name=ge6_fs::utf16(image_path);auto count=std::min(name.size(),size_t(capacity-1));std::memcpy(out,name.data(),count*2);out[count]=0;if(name.size()>=capacity){last_error=122;return capacity;}return uint32_t(count);}
MS void startup_info(void* raw){trace("GetStartupInfoW/A");auto* out=static_cast<uint8_t*>(raw);std::memset(out,0,104);uint32_t size=104;std::memcpy(out,&size,4);}
MS void sleep_ms(uint32_t ms){trace("Sleep");usleep(uint64_t(ms)*1000);}
MS void terminate(uint32_t code){std::lock_guard<std::mutex> guard(trace_mutex);std::cerr<<"GE6_NATIVE_EXIT code="<<code<<'\n';std::_Exit(int(code));}
MS uintptr_t set_exception_filter(uintptr_t callback){trace("SetUnhandledExceptionFilter");return registered_filter.exchange(callback);}// Registration only; original Windows exception dispatch remains unimplemented.
MS uintptr_t create_singleton(void* attributes,int owner,const char* name){
    trace("CreateMutexA");std::lock_guard<std::recursive_mutex> guard(runtime_registry_mutex);require(!attributes&&!owner&&name,"Unimplemented general mutex creation");std::string label=name;
    require(label=="FarmingSimulator2015"||label=="FS15_GE6_BOOT_FIXTURE","Unimplemented mutex name");
    auto path="/tmp/fs15-ge6-"+std::to_string(geteuid())+(label=="FarmingSimulator2015"?"-runtime.lock":"-fixture.lock");
    int fd=open(path.c_str(),O_CREAT|O_RDWR|O_CLOEXEC|O_NOFOLLOW,0600);if(fd<0){last_error=5;return 0;}
    struct stat info{};if(fstat(fd,&info)||!S_ISREG(info.st_mode)||info.st_uid!=geteuid()){close(fd);last_error=5;return 0;}
    bool exists=false;if(flock(fd,LOCK_EX|LOCK_NB)){if(errno==EWOULDBLOCK)exists=true;else{close(fd);last_error=5;return 0;}}
    auto record=std::make_unique<Singleton>();record->fd=fd;auto handle=reinterpret_cast<uintptr_t>(record.get());singleton_handles.emplace(handle,std::move(record));last_error=exists?183:0;return handle;
}// Bounded GE6 singleton existence/lifetime check, not Wait/ReleaseMutex semantics.
MS int close_handle(uintptr_t handle){if(ge6_threads::owns(handle))return ge6_threads::close(handle);std::lock_guard<std::recursive_mutex> guard(runtime_registry_mutex);if(ge6_events::owns(handle))return ge6_events::close(handle);auto found=singleton_handles.find(handle);if(found==singleton_handles.end())return ge6_fs::close(handle);trace("CloseHandle");int result=close(found->second->fd);singleton_handles.erase(found);return result==0;}
MS int debugger_present(){trace("IsDebuggerPresent");std::ifstream file("/proc/self/status");require(bool(file),"Cannot inspect Linux debugger state");std::string key;while(file>>key){if(key=="TracerPid:"){uint32_t pid=0;file>>pid;return pid!=0;}std::string rest;std::getline(file,rest);}throw std::runtime_error("Linux debugger field unavailable");}
MS int32_t com_initialize(void* reserved,uint32_t){trace("CoInitializeEx");std::cout<<"GE6_COM_UNAVAILABLE\n";return int32_t(reserved?0x80070057u:0x80004001u);}// HRESULT failure: no COM apartment or service exists in this transport.
MS void* direct3d9_unavailable(uint32_t){trace("Direct3DCreate9 unavailable on Linux");return nullptr;}// Documented factory failure; no Direct3D object/driver is fabricated. Original optional adapter probe may continue.

MS uint32_t wait_handle(uintptr_t handle,uint32_t milliseconds){return ge6_threads::owns(handle)?ge6_threads::wait(handle,milliseconds):ge6_events::wait(handle,milliseconds);}
MS uint32_t wait_handle_ex(uintptr_t handle,uint32_t milliseconds,int alertable){if(!alertable)return wait_handle(handle,milliseconds);std::lock_guard<std::mutex> guard(trace_mutex);std::cerr<<"GE6_BLOCKED_IMPORT KERNEL32.dll!WaitForSingleObjectEx caller=0x"<<std::hex<<reinterpret_cast<uintptr_t>(__builtin_return_address(0))<<std::dec<<" os_calls="<<calls<<" alertable=1\n";std::_Exit(78);}// APC/alertable waits remain unsupported.
struct ThreadNameInfo {uint32_t type,padding;const char* name;uint32_t tid,flags;};
static_assert(sizeof(ThreadNameInfo)==24);
MS void raise_exception(uint32_t code,uint32_t flags,uint32_t count,const uintptr_t* arguments){
    trace("RaiseException");if(code==0x406d1388u&&!flags&&(count==3||count==6)&&arguments){ThreadNameInfo info{};std::memcpy(&info,arguments,sizeof(info));if(info.type==0x1000&&info.name&&!info.flags){ge6_threads::name(info.tid,info.name);return;}}
    std::lock_guard<std::mutex> guard(trace_mutex);std::cerr<<"GE6_BLOCKED_IMPORT KERNEL32.dll!RaiseException caller=0x"<<std::hex<<reinterpret_cast<uintptr_t>(__builtin_return_address(0))<<std::dec<<" os_calls="<<calls<<" exception=0x"<<std::hex<<code<<std::dec<<'\n';std::_Exit(78);
}// Only the documented debugger thread-name notification is transported; no general SEH is swallowed.

// Authored multithreaded fixtures: no original game code in these callbacks.
struct ThreadFixture {uint32_t tls,fls;uintptr_t gate;int critical=0;uint32_t increments=0;std::atomic<uint32_t> entered{0},cleaned{0};};
void check_native_stack(){uintptr_t self=0;asm("movq %%gs:0x30,%0":"=r"(self));uintptr_t repeated=0,upper=0,lower=0;std::memcpy(&repeated,reinterpret_cast<void*>(self+0x30),8);std::memcpy(&upper,reinterpret_cast<void*>(self+8),8);std::memcpy(&lower,reinterpret_cast<void*>(self+0x10),8);auto stack=reinterpret_cast<uintptr_t>(&self);require(self==repeated&&lower<stack&&stack<upper,"Per-thread NT_TIB stack/self differs");}
MS void fixture_fls_cleanup(void* raw){check_native_stack();++static_cast<ThreadFixture*>(raw)->cleaned;}
MS uint32_t fixture_thread(void* raw){auto& fixture=*static_cast<ThreadFixture*>(raw);check_native_stack();require(ge6_slots::tls_get(fixture.tls)==nullptr&&ge6_slots::fls_get(fixture.fls)==nullptr,"Child inherited parent TLS/FLS values");require(ge6_slots::tls_set(fixture.tls,raw)&&ge6_slots::fls_set(fixture.fls,raw),"Cannot set native worker slots");last_error=12345;++fixture.entered;require(ge6_events::wait(fixture.gate,5000)==0,"Fixture worker gate timed out");for(int i=0;i<500;++i){int local=0;init_lock(&local,0);enter_lock(&local);leave_lock(&local);delete_lock(&local);enter_lock(&fixture.critical);++fixture.increments;leave_lock(&fixture.critical);}require(ge6_slots::tls_get(fixture.tls)==raw&&ge6_slots::fls_get(fixture.fls)==raw,"Concurrent TLS/FLS values changed");require(last_error==0,"TLS successful get did not clear last error");return 0x42;}

uintptr_t resolve(const std::string& library,const std::string& name){
    if(library=="ADVAPI32.dll"){
        if(name=="RegOpenKeyExA")return reinterpret_cast<uintptr_t>(&ge6_system::open_key);
        if(name=="RegQueryValueExA")return reinterpret_cast<uintptr_t>(&ge6_system::query_key);
        if(name=="RegCloseKey")return reinterpret_cast<uintptr_t>(&ge6_system::close_key);
        return 0;
    }
    if(library=="WINMM.dll"){
        if(name=="timeBeginPeriod")return reinterpret_cast<uintptr_t>(&ge6_startup::begin);
        if(name=="timeEndPeriod")return reinterpret_cast<uintptr_t>(&ge6_startup::end);
        if(name=="timeGetTime")return reinterpret_cast<uintptr_t>(&ge6_startup::ticks);
        return 0;
    }
    if(library=="WS2_32.dll")return ge6_net::resolve(name);
    if(library=="OpenAL32.dll")return ge6_audio::resolve(name);
    if(library=="USER32.dll"){
        if(name=="LoadImageW")return reinterpret_cast<uintptr_t>(&ge6_images::load);
        if(name=="DestroyIcon")return reinterpret_cast<uintptr_t>(&ge6_images::destroy);
        if(name=="RegisterClassW")return reinterpret_cast<uintptr_t>(&ge6_classes::add);
        if(name=="UnregisterClassW")return reinterpret_cast<uintptr_t>(&ge6_classes::remove);
        return 0;
    }
    if(library=="fs15_local.dll"&&(image_kind=="local-platform"||image_kind=="fixture"))return ge6_local::resolve(name.c_str());
    if(library=="ole32.dll"&&name=="CoInitializeEx")return reinterpret_cast<uintptr_t>(&com_initialize);
    if(library=="d3d9.dll"&&name=="Direct3DCreate9")return reinterpret_cast<uintptr_t>(&direct3d9_unavailable);
    if(library!="KERNEL32.dll")return 0;
    if(auto file=ge6_fs::resolve(name))return file;
#define BIND(n,f) if(name==n)return reinterpret_cast<uintptr_t>(&f)
    BIND("GetSystemTimeAsFileTime",file_time);BIND("GetCurrentProcessId",process_id);BIND("GetCurrentThreadId",thread_id);
    BIND("GetTickCount",ticks);BIND("QueryPerformanceCounter",counter);BIND("QueryPerformanceFrequency",frequency);
    BIND("SetConsoleCtrlHandler",ge6_console::set_handler);
    BIND("GetLastError",get_error);BIND("SetLastError",set_error);BIND("EncodePointer",encode);BIND("DecodePointer",encode);
    BIND("HeapCreate",heap_create);BIND("GetProcessHeap",process_heap);BIND("HeapAlloc",heap_alloc);BIND("HeapFree",heap_free);BIND("GetVersion",version);BIND("HeapSetInformation",heap_info);
    BIND("HeapSize",heap_size);BIND("HeapReAlloc",heap_realloc);
    BIND("InitializeCriticalSectionAndSpinCount",init_lock);BIND("InitializeCriticalSection",initialize_lock);BIND("EnterCriticalSection",enter_lock);BIND("LeaveCriticalSection",leave_lock);BIND("DeleteCriticalSection",delete_lock);
    BIND("FlsAlloc",ge6_slots::fls_alloc);BIND("TlsAlloc",ge6_slots::tls_alloc);BIND("FlsSetValue",ge6_slots::fls_set);BIND("TlsSetValue",ge6_slots::tls_set);BIND("FlsGetValue",ge6_slots::fls_get);BIND("TlsGetValue",ge6_slots::tls_get);BIND("FlsFree",ge6_slots::fls_free);BIND("TlsFree",ge6_slots::tls_free);
    BIND("GetModuleHandleW",get_module_w);BIND("GetProcAddress",get_proc);BIND("GetCommandLineA",get_command);BIND("GetStdHandle",standard_handle);BIND("GetFileType",file_type);BIND("GetStartupInfoA",startup_info);BIND("GetStartupInfoW",startup_info);BIND("Sleep",sleep_ms);BIND("ExitProcess",terminate);
    BIND("SetHandleCount",handle_count);BIND("GetEnvironmentStringsW",environment_w);BIND("FreeEnvironmentStringsW",free_environment_w);BIND("WideCharToMultiByte",narrow);BIND("GetModuleFileNameA",module_filename);
    BIND("GetACP",ansi_page);BIND("IsValidCodePage",valid_page);BIND("GetCPInfo",page_info);
    BIND("MultiByteToWideChar",widen);BIND("GetStringTypeW",string_type);BIND("LCMapStringW",map_string);
    BIND("SetUnhandledExceptionFilter",set_exception_filter);
    BIND("CreateMutexA",create_singleton);BIND("CloseHandle",close_handle);BIND("IsDebuggerPresent",debugger_present);
    BIND("GetModuleFileNameW",module_filename_w);
    BIND("GetVersionExA",version_ex_a);
    BIND("GetModuleHandleA",get_module_a);BIND("GlobalMemoryStatusEx",ge6_system::memory);BIND("GetSystemInfo",ge6_system::information);BIND("GetNativeSystemInfo",ge6_system::information);
    BIND("GetCurrentThread",ge6_startup::current_thread);BIND("SetThreadAffinityMask",ge6_startup::affinity);
    BIND("CreateEventA",ge6_events::create);BIND("SetEvent",ge6_events::set);BIND("ResetEvent",ge6_events::reset);BIND("WaitForSingleObject",wait_handle);
    BIND("CreateThread",ge6_threads::create);BIND("ResumeThread",ge6_threads::resume);BIND("GetExitCodeThread",ge6_threads::exit_code);
    BIND("RaiseException",raise_exception);
    BIND("GetEnvironmentVariableA",environment_a);
    BIND("WaitForSingleObjectEx",wait_handle_ex);
    BIND("VirtualFree",ge6_vm::free);BIND("VirtualQuery",ge6_vm::query);
    BIND("LoadLibraryA",load_library_a);BIND("FreeLibrary",free_library);
#undef BIND
    return 0;
}

struct Image {
    uint8_t* mapped=nullptr;size_t size=0;uint8_t* trampolines=nullptr;size_t trampoline_bytes=65536;uint32_t entry=0;bool authored_fixture=false;
    ~Image(){if(mapped)munmap(mapped,size);if(trampolines)munmap(trampolines,trampoline_bytes);}
    template<class T>T get(uint32_t rva)const{require(rva<=size&&sizeof(T)<=size-rva,"PE virtual field outside image");T value;std::memcpy(&value,mapped+rva,sizeof(value));return value;}
    std::string name(uint32_t rva)const{require(rva<size,"PE name outside image");auto* begin=reinterpret_cast<const char*>(mapped+rva);auto* end=static_cast<const char*>(std::memchr(begin,0,std::min(size-rva,size_t(1024))));require(end!=nullptr,"Unterminated PE name");return std::string(begin,end);}
    void load(const std::vector<uint8_t>& raw,bool fixture){
        authored_fixture=fixture;
        require(read<uint16_t>(raw,0)==0x5a4d,"Not a PE image");auto pe=read<uint32_t>(raw,0x3c);require(read<uint32_t>(raw,pe)==0x4550&&read<uint16_t>(raw,pe+4)==0x8664,"Expected PE x86_64");
        auto count=read<uint16_t>(raw,pe+6),optional_size=read<uint16_t>(raw,pe+20);size_t opt=size_t(pe)+24;
        require(read<uint16_t>(raw,opt)==0x20b&&read<uint64_t>(raw,opt+24)==base,"Unsupported PE optional header/base");
        require(count&&count<=32&&optional_size>=240,"Unsupported PE section/directory count");
        auto stack_reserve=read<uint64_t>(raw,opt+72);if(stack_reserve)ge6_threads::default_stack=std::min(uint64_t(64*1024*1024),stack_reserve);
        size=read<uint32_t>(raw,opt+56);auto headers=read<uint32_t>(raw,opt+60);require(size>=4096&&size<=0x10000000&&size%4096==0&&headers<=size&&headers<=raw.size(),"Invalid PE image size");
        require(read<uint32_t>(raw,opt+112+9*8)==0,"PE TLS callbacks are not implemented");
        auto allocation=mmap(reinterpret_cast<void*>(base),size,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
        require(allocation!=MAP_FAILED&&reinterpret_cast<uintptr_t>(allocation)==base,"GE6 preferred address range unavailable");mapped=static_cast<uint8_t*>(allocation);std::memcpy(mapped,raw.data(),headers);
        struct Section {uint32_t rva,length,flags;bool skip;};std::vector<Section> sections;
        for(unsigned i=0;i<count;++i){size_t offset=opt+optional_size+i*40;auto vs=read<uint32_t>(raw,offset+8),rva=read<uint32_t>(raw,offset+12),bytes=read<uint32_t>(raw,offset+16),file=read<uint32_t>(raw,offset+20),flags=read<uint32_t>(raw,offset+36);
            require(offset+8<=raw.size(),"Invalid PE section name");std::string section(reinterpret_cast<const char*>(raw.data()+offset),8);bool skip=section.rfind(".bind",0)==0;
            auto length=std::max(vs,bytes);require(rva>=4096&&rva%4096==0&&rva<=size&&length<=size-rva&&file<=raw.size()&&bytes<=raw.size()-file,"Invalid PE section bounds");
            for(const auto& old:sections)require(uint64_t(rva)+length<=old.rva||uint64_t(old.rva)+old.length<=rva,"Overlapping PE sections");
            if(!skip&&bytes)std::memcpy(mapped+rva,raw.data()+file,bytes);
            sections.push_back({rva,length,flags,skip});
        }
        auto resource=read<uint32_t>(raw,opt+128),resource_size=read<uint32_t>(raw,opt+132);
        if(resource||resource_size){require(resource&&resource_size&&uint64_t(resource)+resource_size<=size,"Invalid PE resource bounds");auto readable=std::find_if(sections.begin(),sections.end(),[&](const Section& section){return !section.skip&&(section.flags&0x40000000)&&resource>=section.rva&&uint64_t(resource)+resource_size<=uint64_t(section.rva)+section.length;});require(readable!=sections.end(),"PE resource directory outside readable section");}
        ge6_images::install(base,resource,resource_size?std::vector<uint8_t>(mapped+resource,mapped+resource+resource_size):std::vector<uint8_t>{});
        auto table=read<uint32_t>(raw,opt+120),table_size=read<uint32_t>(raw,opt+124);require(table&&table_size>=20&&table<size&&table_size<=size-table,"Invalid PE import directory");
        auto allocation2=mmap(nullptr,trampoline_bytes,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);require(allocation2!=MAP_FAILED,"Cannot allocate import-stop trampolines");trampolines=static_cast<uint8_t*>(allocation2);
        uint32_t supported=0;bool terminated=false;
        for(uint32_t off=0;off+20<=table_size;off+=20){auto lookup=get<uint32_t>(table+off),dll=get<uint32_t>(table+off+12),iat=get<uint32_t>(table+off+16);if(!lookup&&!dll&&!iat){terminated=true;break;}require(dll&&iat,"Invalid PE import descriptor");auto library=name(dll);if(!lookup)lookup=iat;
            for(uint32_t j=0;;++j){require(j<4096,"Unterminated PE import table");auto value=get<uint64_t>(lookup+j*8);if(!value)break;std::string symbol=(value>>63)?("ordinal:"+std::to_string(value&65535)):name(uint32_t(value)+2);auto target=resolve(library,symbol);
                if(target)++supported;else{require(imports.size()*32+32<=trampoline_bytes,"Import-stop space exhausted");uint32_t id=uint32_t(imports.size());imports.push_back(library+"!"+symbol);auto* code=trampolines+id*32;code[0]=0xb9;std::memcpy(code+1,&id,4);code[5]=0x48;code[6]=0xb8;uintptr_t stop=reinterpret_cast<uintptr_t>(&missing);std::memcpy(code+7,&stop,8);code[15]=0xff;code[16]=0xe0;target=reinterpret_cast<uintptr_t>(code);}
                require(uint64_t(iat)+uint64_t(j)*8+8<=size,"IAT outside PE image");std::memcpy(mapped+iat+j*8,&target,8);
            }
        }require(terminated,"Unterminated PE import directory");require(mprotect(trampolines,trampoline_bytes,PROT_READ|PROT_EXEC)==0,"Cannot protect import-stop trampolines");
        // W^X: original executable pages become RX; mutable data stays RW.
        require(mprotect(mapped,size,PROT_NONE)==0,"Cannot protect PE image");require(mprotect(mapped,(headers+4095)&~size_t(4095),PROT_READ)==0,"Cannot protect PE headers");
        for(const auto& section:sections){if(!section.length||section.skip)continue;int protection=PROT_READ;if(section.flags&0x20000000)protection|=PROT_EXEC;if(section.flags&0x80000000)protection|=PROT_WRITE;require((protection&(PROT_WRITE|PROT_EXEC))!=(PROT_WRITE|PROT_EXEC),"Writable executable PE section rejected");require(mprotect(mapped+section.rva,(section.length+4095)&~size_t(4095),protection)==0,"Cannot protect PE section");}
        entry=fixture?read<uint32_t>(raw,opt+16):0x3fc96c;
        auto found=std::find_if(sections.begin(),sections.end(),[&](const Section& s){return !s.skip&&(s.flags&0x20000000)&&entry>=s.rva&&uint64_t(entry)<uint64_t(s.rva)+s.length;});require(found!=sections.end(),"CRT entry is not in executable code");
        std::cout<<"GE6_IMAGE_MAPPED base=0x"<<std::hex<<base<<" entry=0x"<<base+entry<<std::dec<<" supported_imports="<<supported<<" blocked_imports="<<imports.size()<<'\n';
    }
    void run(){ge6_native::Context thread;std::cout<<"GE6_NATIVE_NT_TIB_READY\n";using Entry=void(MS*)();auto entrypoint=reinterpret_cast<Entry>(mapped+entry);auto prefix=authored_fixture?"GE6_AUTHORED_FIXTURE":"GE6_ORIGINAL_CRT";std::cout<<prefix<<"_ENTER\n";entrypoint();ge6_slots::cleanup();std::cout<<prefix<<"_RETURN os_calls="<<calls<<'\n';}
};
}
int main(int argc,char** argv){
    std::cout.setf(std::ios::unitbuf);std::cerr.setf(std::ios::unitbuf);
    try{
        ge6_slots::trace=trace;ge6_slots::set_error=[](uint32_t value){last_error=value;};
        ge6_threads::trace=trace;ge6_threads::set_error=[](uint32_t value){last_error=value;};
        ge6_threads::record=[](const char* event,uintptr_t entry,uint32_t tid,uint32_t code){std::lock_guard<std::mutex> guard(trace_mutex);std::cout<<"GE6_NATIVE_THREAD_"<<event<<" entry=0x"<<std::hex<<entry<<std::dec<<" tid="<<tid<<" code="<<code<<'\n';};
        ge6_fs::trace=trace;ge6_fs::set_error=[](uint32_t value){last_error=value;};
        ge6_net::trace=trace;
        ge6_audio::trace=trace;
        ge6_images::trace=trace;ge6_images::set_error=[](uint32_t value){last_error=value;};ge6_images::stop=window_unsupported;
        ge6_images::record=[](const char* event,uint32_t first,uint32_t second){std::lock_guard<std::mutex> guard(trace_mutex);std::cout<<"GE6_IMAGE_RESOURCE_"<<event<<" first="<<first<<" second="<<second<<'\n';};
        ge6_classes::trace=trace;ge6_classes::set_error=[](uint32_t value){last_error=value;};ge6_classes::stop=window_unsupported;
        ge6_classes::record=[](const char* event,uint16_t atom,uintptr_t procedure){std::lock_guard<std::mutex> guard(trace_mutex);std::cout<<"GE6_WINDOW_CLASS_"<<event<<" atom="<<atom<<" procedure=0x"<<std::hex<<procedure<<std::dec<<'\n';};
        ge6_console::trace=trace;ge6_console::set_error=[](uint32_t value){last_error=value;};
        ge6_console::record=[](const char* action,uintptr_t entry,uint32_t event){std::lock_guard<std::mutex> guard(trace_mutex);std::cout<<"GE6_CONSOLE_"<<action<<" entry=0x"<<std::hex<<entry<<std::dec<<" event="<<event<<" tid="<<syscall(SYS_gettid)<<'\n';};
        ge6_startup::trace=trace;ge6_startup::set_error=[](uint32_t value){last_error=value;};
        ge6_system::trace=trace;ge6_system::set_error=[](uint32_t value){last_error=value;};
        ge6_events::trace=trace;ge6_events::set_error=[](uint32_t value){last_error=value;};
        ge6_vm::trace=trace;ge6_vm::set_error=[](uint32_t value){last_error=value;};
        if(argc==2&&std::string(argv[1])=="--check-unknown-exception"){raise_exception(0xdeadbeefu,0,0,nullptr);}
        if(argc==2&&std::string(argv[1])=="--check-alertable-wait"){wait_handle_ex(0,0,1);}
        if(argc==2&&std::string(argv[1])=="--check-window-resources"){ge6_window_test::check();std::cout<<"GE6 authored PE icon name/ID lookup, actual PNG/DIB pixels and class registration/lifetime fixtures passed; no window created\n";return 0;}
        if(argc==2&&std::string(argv[1])=="--check-console"){ge6_console_test::check();std::cout<<"GE6 actual Linux console signals, reverse handler order, ignore/remove, fresh MS-ABI threads/GS/TLS/FLS passed\n";return 0;}
        if(argc==2&&std::string(argv[1])=="--check-console-default"){ge6_console::set_handler(ge6_console_test::decline,1);kill(getpid(),SIGINT);for(;;)pause();}
        if(argc==2&&std::string(argv[1])=="--check-audio"){ge6_audio::check();std::cout<<"GE6 Linux OpenAL Soft actual device/context, PCM loopback energy, source queue/play/pause/stop and errors passed\n";return 0;}
        if(argc==2&&std::string(argv[1])=="--check-environment"){
            const char* name="FS15_GE6_AUTHORED_ENV_FIXTURE";unsetenv(name);char out[16]{};require(environment_a(name,out,16)==0&&last_error==203,"Missing environment variable differs");require(setenv(name,"GE6-Linux",1)==0&&environment_a(name,nullptr,0)==10&&environment_a(name,out,4)==10&&environment_a(name,out,16)==9&&std::string(out)=="GE6-Linux","Environment count/copy differs");require(environment_a(nullptr,out,16)==0&&last_error==87&&environment_a(name,nullptr,16)==0&&last_error==87,"Invalid environment buffer accepted");unsetenv(name);std::cout<<"GE6 actual ASCII Linux environment query/capacity/error fixtures passed\n";return 0;
        }
        if(argc==2&&std::string(argv[1])=="--check-threads"){
            ge6_native::Context context;ThreadFixture fixture{};fixture.tls=ge6_slots::tls_alloc();fixture.fls=ge6_slots::fls_alloc(fixture_fls_cleanup);require(fixture.tls==fixture.fls,"TLS and FLS pools are not independent");require(ge6_slots::tls_set(fixture.tls,&fixture)&&ge6_slots::fls_set(fixture.fls,&fixture),"Cannot set parent slots");fixture.gate=ge6_events::create(nullptr,1,0,nullptr);init_lock(&fixture.critical,0);uint32_t tid=0,code=0;auto first=ge6_threads::create(nullptr,0,fixture_thread,&fixture,4,&tid);require(first&&tid&&tid!=uint32_t(syscall(SYS_gettid))&&fixture.entered==0,"Suspended thread ran or ID differs");require(ge6_threads::exit_code(first,&code)&&code==259&&wait_handle_ex(first,0,0)==258,"Suspended thread status differs");ThreadNameInfo naming{0x1000,0,"GE6 authored worker name",tid,0};raise_exception(0x406d1388,0,6,reinterpret_cast<const uintptr_t*>(&naming));auto state=ge6_threads::get(first);char label[16]{};require(pthread_getname_np(state->native,label,16)==0&&std::string(label)==std::string(naming.name).substr(0,15),"Legacy thread name did not reach Linux");require(ge6_threads::resume(first)==1&&ge6_threads::resume(first)==0,"Resume count differs");auto second=ge6_threads::create(nullptr,0,fixture_thread,&fixture,0,nullptr);require(second&&wait_handle(second,0)==258,"Worker did not wait for gate");last_error=54321;require(ge6_events::set(fixture.gate)&&wait_handle(first,5000)==0&&wait_handle(second,5000)==0,"Native worker wait timed out");require(last_error==54321&&fixture.entered==2&&fixture.cleaned==2&&fixture.increments==1000,"Thread errors, FLS cleanup or critical-section updates differ");require(ge6_threads::exit_code(first,&code)&&code==0x42&&ge6_threads::running==0,"Native worker exit code/lifetime differs");require(ge6_slots::tls_get(fixture.tls)==&fixture&&ge6_slots::fls_get(fixture.fls)==&fixture,"Workers overwrote parent slot values");require(close_handle(first)&&close_handle(second)&&close_handle(fixture.gate)&&!close_handle(first)&&last_error==6,"Thread handle cleanup differs");delete_lock(&fixture.critical);require(ge6_slots::fls_free(fixture.fls)&&fixture.cleaned==3&&ge6_slots::tls_free(fixture.tls),"Parent FLS/TLS cleanup differs");auto reused=ge6_slots::tls_alloc();require(reused==fixture.tls&&ge6_slots::tls_get(reused)==nullptr&&ge6_slots::tls_free(reused),"Reused TLS slot retained data");ge6_slots::cleanup();std::cout<<"GE6 actual pthread Microsoft-ABI callbacks, distinct TIDs/stacks/TLS/FLS, suspended resume, critical-section contention, waits and FLS cleanup passed; fibers/SEH/arbitrary suspension remain unsupported\n";return 0;
        }
        if(argc==2&&std::string(argv[1])=="--check-virtual-memory"){
            auto denied=[](void* memory){pid_t child=fork();require(child>=0,"Cannot fork page-protection fixture");if(!child){signal(SIGSEGV,+[](int){_exit(33);});*static_cast<volatile uint8_t*>(memory)=1;_exit(1);}int status=0;require(waitpid(child,&status,0)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==33,"Linux page protection did not reject write");};
            void* memory=nullptr;size_t bytes=3*ge6_vm::page();require(ge6_vm::allocate(~uintptr_t(0),&memory,1,&bytes,0x2000,4)==0&&reinterpret_cast<uintptr_t>(memory)<0x80000000,"Cannot reserve low Lua memory");denied(memory);ge6_vm::Information info{};require(ge6_vm::query(memory,&info,48)==48&&info.state==0x2000&&info.size==bytes,"Reserved region query differs");
            auto committed=memory;size_t count=ge6_vm::page();require(ge6_vm::allocate(~uintptr_t(0),&committed,1,&count,0x1000,4)==0,"Cannot commit Lua memory");auto* data=static_cast<uint8_t*>(memory);require(data[0]==0,"New committed page not zero");data[0]=0x5a;require(ge6_vm::allocate(~uintptr_t(0),&committed,1,&count,0x1000,4)==0&&data[0]==0x5a,"Repeated commit discarded data");require(ge6_vm::query(memory,&info,48)==48&&info.state==0x1000&&info.size==count,"Committed region query differs");
            require(ge6_vm::free(memory,count,0x4000)==1,"Cannot decommit Lua memory");denied(memory);require(ge6_vm::allocate(~uintptr_t(0),&committed,1,&count,0x1000,2)==0&&data[0]==0,"Recommitted page retained data");denied(memory);require(!ge6_vm::free(data+ge6_vm::page(),0,0x8000)&&ge6_vm::free(memory,0,0x8000)&&ge6_vm::regions.empty(),"Reservation release/lifetime differs");require(!ge6_vm::query(memory,&info,48)&&last_error==487,"Released mapping retained state");
            memory=nullptr;count=1;require(ge6_vm::allocate(~uintptr_t(0),&memory,1,&count,0x3000,4)==0&&count==ge6_vm::page()&&ge6_vm::free(memory,0,0x8000),"Combined reserve/commit rounding differs");require(ge6_vm::allocate(0,&memory,0,&count,0x2000,4)==int32_t(0xc0000008u),"Foreign process handle accepted");require(get_proc(get_module_a("ntdll.dll"),"NtAllocateVirtualMemory")!=0&&get_proc(0,"invalid-module")==0&&last_error==126,"NT module/procedure transport differs");std::cout<<"GE6 original Lua allocator OS contracts: reserve/commit/decommit/release, actual page faults and bounded query fixtures passed; general NT/JIT parity is not claimed\n";return 0;
        }
        if(argc==2&&std::string(argv[1])=="--check-events"){
            auto automatic=ge6_events::create(nullptr,0,0,nullptr);require(automatic&&ge6_events::wait(automatic,0)==258,"Unsignaled event did not time out");std::atomic<uint32_t> result{999};std::thread waiter([&](){result=ge6_events::wait(automatic,1000);});require(ge6_events::set(automatic)==1,"Cannot signal waiting event");waiter.join();require(result==0&&ge6_events::wait(automatic,0)==258,"Auto-reset event signal was not consumed");require(ge6_events::set(automatic)&&ge6_events::set(automatic)&&ge6_events::wait(automatic,0)==0&&ge6_events::wait(automatic,0)==258,"Auto-reset signals incorrectly accumulated");
            auto manual=ge6_events::create(nullptr,1,1,nullptr);require(manual&&ge6_events::wait(manual,0)==0&&ge6_events::wait(manual,0)==0,"Manual event did not remain signaled");require(ge6_events::reset(manual)&&ge6_events::wait(manual,3)==258,"Manual reset/timeout differs");require(close_handle(automatic)&&close_handle(manual)&&!close_handle(manual)&&last_error==6,"Event handle lifetime differs");require(ge6_events::wait(manual,0)==0xffffffffu&&last_error==6&&ge6_events::create(nullptr,0,0,"unsupported-name")==0&&last_error==50,"Unknown/named event accepted");std::cout<<"GE6 unnamed auto/manual event waits, actual worker wakeup and lifetime fixtures passed; named events and full Windows thread parity remain unsupported\n";return 0;
        }
        if(argc==2&&std::string(argv[1])=="--check-system"){
            auto mm=load_library_a("winmm.dll");require(mm==winmm_token&&load_library_a("WINMM.DLL")==mm&&get_proc(mm,"timeBeginPeriod")!=0&&get_proc(mm,"timeEndPeriod")!=0,"Known timer module bindings missing");require(free_library(mm)&&free_library(mm)&&!free_library(mm)&&last_error==6,"Explicit module reference lifetime differs");require(!load_library_a("unavailable-fixture.dll")&&last_error==126&&!get_proc(mm,"unsupported-function")&&last_error==127,"Unavailable module/procedure reported present");
            using namespace ge6_system;uintptr_t key=0;require(open_key(0x80000002u,"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",0,0x20019,&key)==0&&key,"Linux CPU view missing");uint32_t size=0,type=0;require(query_key(key,"ProcessorNameString",nullptr,&type,nullptr,&size)==0&&type==1&&size>1,"CPU value size/type differs");std::vector<char> model(size+1,char(0x5a));uint32_t small=1;require(query_key(key,"ProcessorNameString",nullptr,&type,model.data(),&small)==234&&small==size&&model[0]==0x5a,"Small registry buffer contract differs");require(query_key(key,"ProcessorNameString",nullptr,nullptr,model.data(),&size)==0&&std::string(model.data())==cpu_name()&&model[size]==0x5a,"CPU value/source or bounds differ");require(close_key(key)==0&&close_key(key)==6&&query_key(key,"ProcessorNameString",nullptr,nullptr,nullptr,&size)==6,"Registry handle lifetime differs");require(open_key(0x80000002u,"SOFTWARE\\MissingFixture",0,0x20019,&key)==2&&key==0,"Unavailable registry key synthesized");
            MemoryStatus mem{};mem.size=64;require(memory(&mem)&&mem.physical>0&&mem.available<=mem.physical&&mem.load<=100&&mem.available_pagefile<=mem.pagefile&&mem.virtual_available<=mem.virtual_total,"Linux memory values differ");mem.size=0;require(!memory(&mem)&&last_error==87,"Invalid memory status size accepted");SystemInfo sys{};information(&sys);require(sys.architecture==9&&sys.page_size==uint32_t(sysconf(_SC_PAGESIZE))&&sys.processors>0&&sys.minimum<sys.maximum,"System info fields differ");require(get_module_a("KERNEL32.DLL")==kernel_token&&get_proc(kernel_token,"GetNativeSystemInfo")!=0&&get_module_a("absent-fixture.dll")==0&&last_error==126,"Optional module resolution differs");std::cout<<"GE6 live Linux CPU/memory/system-query and read-only registry-view fixtures passed; complete registry/Windows memory parity is not claimed\n";return 0;
        }
        if(argc==2&&std::string(argv[1])=="--check-startup"){
            using namespace ge6_startup;auto slack=prctl(PR_GET_TIMERSLACK);require(slack>=0&&begin(0)==97&&end(1)==97,"Invalid timer period accepted");require(begin(1)==0&&begin(1)==0&&begin(2)==0&&periods[1]==2,"Nested timer lease differs");require(end(1)==0&&periods[1]==1&&end(2)==0&&end(1)==0&&periods.empty()&&prctl(PR_GET_TIMERSLACK)==slack,"Timer lease did not restore slack");auto start=ge6_startup::ticks();usleep(3000);require(uint32_t(ge6_startup::ticks()-start)>=2,"Multimedia clock did not advance");
            require(capture_affinity(),"Cannot query fixture CPU mask");uintptr_t original=0;bool wide=false;for(int i=0;i<CPU_SETSIZE;++i)if(CPU_ISSET(i,&allowed)){if(i>=64)wide=true;else original|=uintptr_t(1)<<i;}
            require(affinity(0,1)==0&&last_error==6&&affinity(current_thread(),0)==0&&last_error==87,"Invalid thread/mask accepted");
            if(!wide){auto one=original&(~original+1);require(one&&affinity(current_thread(),one)==original,"Previous affinity mask differs");cpu_set_t actual{};require(pthread_getaffinity_np(pthread_self(),sizeof(actual),&actual)==0&&CPU_COUNT(&actual)==1,"OS affinity did not change");require(affinity(current_thread(),original)==one,"Affinity restore differs");}else require(affinity(current_thread(),1)==0&&last_error==50,"Unsupported wide CPU group accepted");
            std::cout<<"GE6 main-thread timer slack/lifetime and actual CPU affinity fixtures passed; full WinMM/thread parity is not claimed\n";return 0;
        }
        if(argc==2&&std::string(argv[1])=="--check-network"){
            using namespace ge6_net;StartupData info{};require(startup(0,&info)==10092&&references==0&&startup(0x202,nullptr)==10014,"Invalid startup accepted");require(create(2,2,0)==~uintptr_t(0)&&ge6_net::get_error()==10093,"Socket used before startup");
            require(startup(0x202,&info)==0&&info.version==0x202&&info.highest==0x202&&info.vendor==0,"Socket version/ABI differs");require(startup(0x101,&info)==0&&references==2&&info.version==0x101,"Nested startup/version negotiation differs");
            auto receiver=create(2,2,17),sender=create(2,2,0);require(receiver!=~uintptr_t(0)&&sender!=~uintptr_t(0),"Cannot create UDP fixture sockets");
            sockaddr_in target{};target.sin_family=AF_INET;target.sin_addr.s_addr=htonl(INADDR_LOOPBACK);require(ge6_net::bind(receiver,&target,16)==0,"Cannot bind loopback fixture");int address_size=16;require(name(receiver,&target,&address_size)==0&&target.sin_port!=0,"Ephemeral loopback address missing");
            const char payload[]{'G','E','6','-',char(0xc3),char(0xa9)};require(send_to(sender,payload,6,0,&target,16)==6,"Loopback UDP send differs");char readback[16]{};sockaddr_in peer{};address_size=16;require(receive_from(receiver,readback,16,0,&peer,&address_size)==6&&!std::memcmp(payload,readback,6)&&peer.sin_addr.s_addr==htonl(INADDR_LOOPBACK),"Loopback UDP receive differs");
            require(send_to(sender,payload,6,0,&target,16)==6&&receive_from(receiver,readback,2,0,nullptr,nullptr)==-1&&ge6_net::get_error()==10040,"Truncated UDP datagram not reported");
            require(ge6_net::close(sender)==0&&ge6_net::close(sender)==-1&&ge6_net::get_error()==10038,"Socket lifetime differs");require(cleanup()==0&&references==1&&name(receiver,&peer,&address_size)==0,"Nested cleanup closed live sockets");require(cleanup()==0&&sockets.empty()&&cleanup()==-1&&ge6_net::get_error()==10093,"Final cleanup differs");
            require(startup(0x202,&info)==0&&ge6_net::close(receiver)==-1&&ge6_net::get_error()==10038&&cleanup()==0,"Stale socket survived cleanup");std::cout<<"GE6 real IPv4 UDP loopback, Winsock startup/cleanup/error and handle lifetime fixtures passed; full network parity is not claimed\n";return 0;
        }
        if(argc==2&&std::string(argv[1])=="--check-version"){
            require(com_initialize(nullptr,0)==int32_t(0x80004001u)&&com_initialize(&heap_token,0)==int32_t(0x80070057u),"Unavailable COM reported success");
            require(direct3d9_unavailable(32)==nullptr,"Unavailable Direct3D9 factory reported success");
            for(uint32_t size:{148u,156u}){std::array<uint8_t,164> info;info.fill(0xa5);std::memcpy(info.data(),&size,4);require(version_ex_a(info.data())==1,"Compatibility version query failed");uint32_t major=0,platform=0;std::memcpy(&major,info.data()+4,4);std::memcpy(&platform,info.data()+16,4);require(major==(version()&255)&&platform==2&&info[20]==0&&info[size]==0xa5,"Version fields or output bounds differ");if(size==156)require(info[154]==1,"Compatibility product type differs");}
            uint32_t invalid=147;require(!version_ex_a(&invalid)&&last_error==87&&invalid==147&&!version_ex_a(nullptr),"Invalid version structure accepted");std::cout<<"GE6 API compatibility version fixtures passed; Linux host version is reported separately\n";return 0;
        }
        if(argc==3&&std::string(argv[1])=="--check-files"){
            auto root=ge6_fs::utf16(std::string(argv[2])+"/unicode-\xc3\xa9-\xe2\x82\xac");auto wide=[](const std::u16string& s){return reinterpret_cast<const uint16_t*>(s.c_str());};
            require(ge6_fs::directory(wide(root),nullptr)==1,"Cannot create filesystem fixture");require(ge6_fs::directory(wide(root),nullptr)==0&&last_error==183,"Directory existence error differs");
            auto source=root+u"/source.bin",copy=root+u"/copy.bin";auto handle=ge6_fs::create(wide(source),0xc0000000u,1,nullptr,2,0x80,0);require(handle!=~uintptr_t(0),"Cannot open filesystem fixture");
            const char payload[]{'G','E','6','-',char(0xc3),char(0xa9)};uint32_t done=0;require(ge6_fs::write(handle,payload,sizeof(payload),&done,nullptr)&&done==sizeof(payload),"File write differs");
            int64_t total=0;uint64_t modified=0;require(ge6_fs::size_ex(handle,&total)&&total==6&&file_type(handle)==1,"File size/type differs");require(ge6_fs::times(handle,nullptr,nullptr,&modified)&&modified>116444736000000000ull,"Modification timestamp differs");require(ge6_fs::seek_ex(handle,0,nullptr,0),"File seek differs");
            char output[8]{};require(ge6_fs::read(handle,output,6,&done,nullptr)&&done==6&&!std::memcmp(output,payload,6),"File read differs");require(ge6_fs::read(handle,output,1,&done,nullptr)&&done==0,"EOF read differs");
            int32_t high=0;require(ge6_fs::seek(handle,2,&high,0)==2&&high==0,"Legacy file seek differs");require(ge6_fs::read(handle,output,1,&done,nullptr)&&output[0]=='6',"Legacy seek position differs");require(close_handle(handle)&&!close_handle(handle)&&last_error==6,"File handle lifetime differs");
            std::array<uint8_t,592> found{};auto search=ge6_fs::find_first(wide(source),found.data());require(search!=~uintptr_t(0),"Cannot find exact fixture file");uint32_t low=0;std::memcpy(&low,found.data()+32,4);require(low==6,"WIN32_FIND_DATAW file size differs");require(!ge6_fs::find_next(search,found.data())&&last_error==18&&ge6_fs::find_close(search),"Find lifetime/EOF differs");
            require(ge6_fs::copy(wide(source),wide(copy),1)&&!ge6_fs::copy(wide(source),wide(copy),1)&&last_error==183&&ge6_fs::copy(wide(source),wide(copy),0),"CopyFileW disposition differs");
            handle=ge6_fs::create(wide(copy),0x80000000u,1,nullptr,3,0x80,0);uint64_t copied_time=0;require(handle!=~uintptr_t(0)&&ge6_fs::times(handle,nullptr,nullptr,&copied_time)&&copied_time==modified,"CopyFileW modification time differs");require(close_handle(handle),"Copied handle cleanup differs");
            auto absent=root+u"/absent";require(ge6_fs::find_first(wide(absent),found.data())==~uintptr_t(0)&&last_error==2,"Missing-file result differs");
            auto drive=u"Z:"+source;require(ge6_fs::path(wide(drive))==ge6_fs::path(wide(source)),"Linux root drive mapping differs");
            handle=ge6_fs::create(wide(drive),0x80000000u,1,nullptr,3,0x80,0);require(handle!=~uintptr_t(0)&&close_handle(handle),"Mapped drive cannot open original file");
            for(const auto* invalid:{u"C:/unmapped",u"Z:relative"}){bool rejected=false;try{ge6_fs::path(reinterpret_cast<const uint16_t*>(invalid));}catch(const std::runtime_error&){rejected=true;}require(rejected,"Unsupported drive path accepted");}
            const unsigned char tractor[]{0xf0,0x9f,0x9a,0x9c,0};uint16_t utf[8]{};char roundtrip[16]{};require(widen(65001,0,tractor,-1,utf,8)==3&&utf[0]==0xd83d&&utf[1]==0xde9c,"UTF8 surrogate conversion differs");require(narrow(65001,0,utf,-1,roundtrip,16,nullptr,nullptr)==5&&!std::memcmp(roundtrip,tractor,5),"UTF8 byte count/roundtrip differs");
            std::cout<<"GE6 synchronous file and UTF8 fixtures passed; full Windows sharing/namespace parity is not claimed\n";return 0;
        }
        if(argc==2&&std::string(argv[1])=="--check-singleton"){
            auto first=create_singleton(nullptr,0,"FS15_GE6_BOOT_FIXTURE");require(first&&last_error==0,"Cannot create singleton fixture");pid_t child=fork();require(child>=0,"Cannot fork singleton fixture");
            if(child==0){auto second=create_singleton(nullptr,0,"FS15_GE6_BOOT_FIXTURE");bool ok=second&&last_error==183;if(second)close_handle(second);_exit(ok?0:1);}
            int status=0;require(waitpid(child,&status,0)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==0,"Cross-process singleton detection differs");close_handle(first);
            auto third=create_singleton(nullptr,0,"FS15_GE6_BOOT_FIXTURE");require(third&&last_error==0,"Singleton lifetime retained stale ownership");close_handle(third);std::cout<<"GE6 singleton creation, cross-process presence and released lifetime passed (authored fixture)\n";return 0;
        }
        require(argc>=3,"Usage: FS15GE6LinuxBoot --image private-runtime.bin [--command-line string] | --fixture authored-test.exe");bool fixture=std::string(argv[1])=="--fixture";require(fixture||std::string(argv[1])=="--image","Expected --image or --fixture");
        char* absolute=realpath(argv[2],nullptr);require(absolute!=nullptr,"Cannot resolve private image path");image_path=absolute;std::free(absolute);std::ifstream file(argv[2],std::ios::binary|std::ios::ate);require(bool(file),"Cannot open private GE6 image");auto size=file.tellg();require(size>0&&size<=0x4000000,"Invalid image file size");std::vector<uint8_t> raw(static_cast<size_t>(size));file.seekg(0);file.read(reinterpret_cast<char*>(raw.data()),size);require(bool(file),"Truncated image file");
        if(!fixture){std::array<unsigned char,SHA256_DIGEST_LENGTH> hash{};SHA256(raw.data(),raw.size(),hash.data());std::ostringstream text;for(auto b:hash)text<<std::hex<<std::setfill('0')<<std::setw(2)<<unsigned(b);require(text.str()==expected||text.str()==local_expected,"GE6 image hash differs from pinned private reference/local platform");image_kind=text.str()==expected?"reference":"local-platform";}
        std::cout<<"GE6_IMAGE_KIND="<<image_kind<<'\n';
        if(!fixture)image_path="Z:"+image_path;
        if(argc>3){require(argc==5&&std::string(argv[3])=="--command-line","Unknown argument");command_line=argv[4];}
        struct sigaction action{};action.sa_sigaction=fault;action.sa_flags=SA_SIGINFO;sigemptyset(&action.sa_mask);for(int signal:{SIGSEGV,SIGILL,SIGBUS,SIGABRT})sigaction(signal,&action,nullptr);
        Image image;ge6_console::Lifetime console_lifetime;image.load(raw,fixture);image.run();if(ge6_threads::running!=0){std::cerr<<"GE6_BOOT_TRANSPORT_ERROR native entry returned with active workers\n";std::_Exit(78);}return 0;
    }catch(const std::exception& error){std::cerr<<"GE6_BOOT_TRANSPORT_ERROR "<<error.what()<<'\n';return 1;}
}
