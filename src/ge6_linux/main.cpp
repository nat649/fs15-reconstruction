// Linux x64 transport for the original, privately supplied GE6 machine code.
// No recovered C++ renderer/physics/game implementation is linked here.
// Unimplemented imports terminate with their name and original return address.
#include <openssl/sha.h>
#include "ansi_reference.h"
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <asm/prctl.h>
#include <pthread.h>
#include <malloc.h>
#include <unistd.h>
#include <signal.h>
#include <ucontext.h>
#include <array>
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
#include <vector>

#define MS __attribute__((ms_abi))
namespace {
constexpr uintptr_t base=0x140000000;
constexpr char expected[]="6d1beaf9d7e3c57dd51cec03385a53bda82b4ac946e622d8d2fa592a56d71f9f";
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
template<class T>T read(const std::vector<uint8_t>& bytes,size_t offset){require(offset<=bytes.size()&&sizeof(T)<=bytes.size()-offset,"Truncated PE field");T out;std::memcpy(&out,bytes.data()+offset,sizeof(T));return out;}
std::vector<std::string> imports;
uint64_t calls=0;
uintptr_t registered_filter=0;
bool main_reached=false;
thread_local uint32_t last_error=0;
std::vector<void*> slots;
std::map<void*,std::unique_ptr<std::recursive_mutex>> locks;
int heap_token=0;
constexpr uintptr_t kernel_token=0x47453601;
std::string command_line="FarmingSimulator2015Game.exe";
std::string image_path;
void trace(const char* name){++calls;std::cout<<"GE6_OS "<<name<<'\n';}
[[noreturn]] MS void missing(uint32_t id){
    std::cerr<<"GE6_BLOCKED_IMPORT "<<(id<imports.size()?imports[id]:"invalid-index")
             <<" caller=0x"<<std::hex<<reinterpret_cast<uintptr_t>(__builtin_return_address(0))
             <<std::dec<<" os_calls="<<calls<<'\n';std::exit(78);
}
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
MS int heap_info(void* heap,uint32_t kind,void*,size_t size){trace("HeapSetInformation");require(heap==&heap_token&&kind==0&&size==4,"Unsupported heap information");return 1;}
MS int init_lock(void* p,uint32_t){trace("InitializeCriticalSectionAndSpinCount");locks[p]=std::make_unique<std::recursive_mutex>();return 1;}
MS void initialize_lock(void* p){init_lock(p,0);}
MS void enter_lock(void* p){auto it=locks.find(p);require(it!=locks.end(),"Uninitialized critical section");it->second->lock();}
MS void leave_lock(void* p){auto it=locks.find(p);require(it!=locks.end(),"Uninitialized critical section");it->second->unlock();}
MS void delete_lock(void* p){require(locks.erase(p)==1,"Uninitialized critical section deletion");}
MS uint32_t alloc_slot(void*){trace("FlsAlloc/TlsAlloc");require(slots.size()<128,"TLS slots exhausted");slots.push_back(nullptr);return uint32_t(slots.size()-1);}
MS uint32_t tls_alloc(){return alloc_slot(nullptr);}
MS int set_slot(uint32_t i,void* p){require(i<slots.size(),"Invalid TLS slot");slots[i]=p;return 1;}
MS void* get_slot(uint32_t i){require(i<slots.size(),"Invalid TLS slot");last_error=0;return slots[i];}
MS int free_slot(uint32_t i){require(i<slots.size(),"Invalid TLS slot");slots[i]=nullptr;return 1;}
MS uintptr_t get_module_w(const uint16_t* name){trace("GetModuleHandleW");if(!name)return base;std::string text;while(*name&&text.size()<128)text.push_back(char(*name++));std::transform(text.begin(),text.end(),text.begin(),[](unsigned char c){return char(std::tolower(c));});require(text=="kernel32.dll","Unimplemented module lookup");return kernel_token;}
uintptr_t resolve(const std::string& library,const std::string& name);
MS uintptr_t get_proc(uintptr_t module,const char* name){trace("GetProcAddress");require(module==kernel_token&&reinterpret_cast<uintptr_t>(name)>65535,"Unimplemented module/procedure lookup");auto result=resolve("KERNEL32.dll",name);if(!result)std::cerr<<"GE6_OPTIONAL_PROC_UNAVAILABLE "<<name<<'\n';return result;}
MS char* get_command(){trace("GetCommandLineA");return command_line.data();}
MS uintptr_t standard_handle(int32_t id){require(id>=-12&&id<=-10,"Unknown standard handle");return uintptr_t(0x100+(-10-id));}
MS uint32_t file_type(uintptr_t handle){require(handle>=0x100&&handle<=0x102,"Unknown CRT standard handle");return isatty(int(handle-0x100))?2:3;}
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
int byte_index(uint16_t unit){auto found=std::find(ge6_ansi::unicode.begin(),ge6_ansi::unicode.end(),unit);return found==ge6_ansi::unicode.end()?-1:int(found-ge6_ansi::unicode.begin());}
int wide_count(const uint16_t* source,int count){if(count==-1){count=0;while(count<1048576&&source[count])++count;++count;}require(count>0&&count<=1048576,"Invalid text count");return count;}
MS int widen(uint32_t page,uint32_t flags,const unsigned char* source,int count,uint16_t* out,int capacity){
    trace("MultiByteToWideChar");require((page==0||page==1252)&&(flags==0||flags==1||flags==9),"Unimplemented multibyte page/flags");
    if(count==-1){count=0;while(count<1048576&&source[count])++count;++count;}require(count>0&&count<=1048576&&capacity>=0,"Invalid multibyte conversion bounds");
    if(capacity==0)return count;
    if(!out||capacity<count){last_error=122;return 0;}
    for(int i=0;i<count;++i)out[i]=ge6_ansi::unicode[source[i]];
    return count;
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
    for(int i=0;i<count;++i){int value=page==65001?(source[i]<128?int(source[i]):-1):byte_index(source[i]);
        if(value<0&&page!=65001){auto found=std::find(ge6_ansi::bestfit_unicode.begin(),ge6_ansi::bestfit_unicode.end(),source[i]);if(found!=ge6_ansi::bestfit_unicode.end()){auto index=size_t(found-ge6_ansi::bestfit_unicode.begin());value=ge6_ansi::bestfit_byte[index];defaulted=defaulted||ge6_ansi::bestfit_default[index];}}
        if(value<0){last_error=1113;return 0;}bytes.push_back(char(value));}// Only observed CP1252 case substitutions are supported.
    if(used)*used=defaulted?1:0;
    if(capacity==0)return count;
    if(!out||capacity<count){last_error=122;return 0;}
    std::memcpy(out,bytes.data(),bytes.size());
    return count;
}
MS uint32_t module_filename(uintptr_t module,char* out,uint32_t capacity){
    trace("GetModuleFileNameA");require(module==0||module==base,"Unimplemented module filename");
    if(!capacity){last_error=122;return 0;}
    auto size=std::min(image_path.size(),size_t(capacity-1));std::memcpy(out,image_path.data(),size);out[size]=0;
    if(image_path.size()>=capacity){last_error=122;return capacity;}return uint32_t(size);
}
MS void startup_info(void* raw){trace("GetStartupInfoW/A");auto* out=static_cast<uint8_t*>(raw);std::memset(out,0,104);uint32_t size=104;std::memcpy(out,&size,4);}
MS void sleep_ms(uint32_t ms){trace("Sleep");usleep(uint64_t(ms)*1000);}
MS void terminate(uint32_t code){std::cerr<<"GE6_NATIVE_EXIT code="<<code<<'\n';std::exit(int(code));}
MS uintptr_t set_exception_filter(uintptr_t callback){trace("SetUnhandledExceptionFilter");auto previous=registered_filter;registered_filter=callback;return previous;}// Registration only; original Windows exception dispatch remains unimplemented.

uintptr_t resolve(const std::string& library,const std::string& name){
    if(library!="KERNEL32.dll")return 0;
#define BIND(n,f) if(name==n)return reinterpret_cast<uintptr_t>(&f)
    BIND("GetSystemTimeAsFileTime",file_time);BIND("GetCurrentProcessId",process_id);BIND("GetCurrentThreadId",thread_id);
    BIND("GetTickCount",ticks);BIND("QueryPerformanceCounter",counter);BIND("QueryPerformanceFrequency",frequency);
    BIND("GetLastError",get_error);BIND("SetLastError",set_error);BIND("EncodePointer",encode);BIND("DecodePointer",encode);
    BIND("HeapCreate",heap_create);BIND("GetProcessHeap",process_heap);BIND("HeapAlloc",heap_alloc);BIND("HeapFree",heap_free);BIND("GetVersion",version);BIND("HeapSetInformation",heap_info);
    BIND("HeapSize",heap_size);BIND("HeapReAlloc",heap_realloc);
    BIND("InitializeCriticalSectionAndSpinCount",init_lock);BIND("InitializeCriticalSection",initialize_lock);BIND("EnterCriticalSection",enter_lock);BIND("LeaveCriticalSection",leave_lock);BIND("DeleteCriticalSection",delete_lock);
    BIND("FlsAlloc",alloc_slot);BIND("TlsAlloc",tls_alloc);BIND("FlsSetValue",set_slot);BIND("TlsSetValue",set_slot);BIND("FlsGetValue",get_slot);BIND("TlsGetValue",get_slot);BIND("FlsFree",free_slot);BIND("TlsFree",free_slot);
    BIND("GetModuleHandleW",get_module_w);BIND("GetProcAddress",get_proc);BIND("GetCommandLineA",get_command);BIND("GetStdHandle",standard_handle);BIND("GetFileType",file_type);BIND("GetStartupInfoA",startup_info);BIND("GetStartupInfoW",startup_info);BIND("Sleep",sleep_ms);BIND("ExitProcess",terminate);
    BIND("SetHandleCount",handle_count);BIND("GetEnvironmentStringsW",environment_w);BIND("FreeEnvironmentStringsW",free_environment_w);BIND("WideCharToMultiByte",narrow);BIND("GetModuleFileNameA",module_filename);
    BIND("GetACP",ansi_page);BIND("IsValidCodePage",valid_page);BIND("GetCPInfo",page_info);
    BIND("MultiByteToWideChar",widen);BIND("GetStringTypeW",string_type);BIND("LCMapStringW",map_string);
    BIND("SetUnhandledExceptionFilter",set_exception_filter);
#undef BIND
    return 0;
}

// Keep Linux libc's FS TLS untouched. The original MSVC __chkstk reads GS:0x10.
// Only the inspected NT_TIB stack/self prefix is supplied; no complete PEB/SEH.
struct NativeThread {
    alignas(16) std::array<uint8_t,0x100> tib{};uintptr_t previous_gs=0;bool installed=false;
    NativeThread(){
        pthread_attr_t attr;require(pthread_getattr_np(pthread_self(),&attr)==0,"Cannot inspect Linux thread stack");
        void* stack=nullptr;size_t bytes=0;int result=pthread_attr_getstack(&attr,&stack,&bytes);pthread_attr_destroy(&attr);require(result==0&&bytes,"Cannot inspect Linux stack bounds");
        uintptr_t limit=reinterpret_cast<uintptr_t>(stack),upper=limit+bytes,self=reinterpret_cast<uintptr_t>(tib.data()),none=~uintptr_t(0);
        std::memcpy(tib.data(),&none,8);std::memcpy(tib.data()+8,&upper,8);std::memcpy(tib.data()+0x10,&limit,8);std::memcpy(tib.data()+0x30,&self,8);
        require(syscall(SYS_arch_prctl,ARCH_GET_GS,&previous_gs)==0,"Cannot read Linux GS base");require(syscall(SYS_arch_prctl,ARCH_SET_GS,self)==0,"Cannot install native NT_TIB prefix");installed=true;
        std::cout<<"GE6_NATIVE_NT_TIB_READY\n";
    }
    ~NativeThread(){if(installed){auto result=syscall(SYS_arch_prctl,ARCH_SET_GS,previous_gs);(void)result;}}
};

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
    void run(){NativeThread thread;using Entry=void(MS*)();auto entrypoint=reinterpret_cast<Entry>(mapped+entry);auto prefix=authored_fixture?"GE6_AUTHORED_FIXTURE":"GE6_ORIGINAL_CRT";std::cout<<prefix<<"_ENTER\n";entrypoint();std::cout<<prefix<<"_RETURN os_calls="<<calls<<'\n';}
};
}
int main(int argc,char** argv){
    std::cout.setf(std::ios::unitbuf);std::cerr.setf(std::ios::unitbuf);
    try{require(argc>=3,"Usage: FS15GE6LinuxBoot --image private-runtime.bin [--command-line string] | --fixture authored-test.exe");bool fixture=std::string(argv[1])=="--fixture";require(fixture||std::string(argv[1])=="--image","Expected --image or --fixture");
        char* absolute=realpath(argv[2],nullptr);require(absolute!=nullptr,"Cannot resolve private image path");image_path=absolute;std::free(absolute);std::ifstream file(argv[2],std::ios::binary|std::ios::ate);require(bool(file),"Cannot open private GE6 image");auto size=file.tellg();require(size>0&&size<=0x4000000,"Invalid image file size");std::vector<uint8_t> raw(static_cast<size_t>(size));file.seekg(0);file.read(reinterpret_cast<char*>(raw.data()),size);require(bool(file),"Truncated image file");
        if(!fixture){std::array<unsigned char,SHA256_DIGEST_LENGTH> hash{};SHA256(raw.data(),raw.size(),hash.data());std::ostringstream text;for(auto b:hash)text<<std::hex<<std::setfill('0')<<std::setw(2)<<unsigned(b);require(text.str()==expected,"GE6 image hash differs from pinned private reference");}
        if(argc>3){require(argc==5&&std::string(argv[3])=="--command-line","Unknown argument");command_line=argv[4];}
        struct sigaction action{};action.sa_sigaction=fault;action.sa_flags=SA_SIGINFO;sigemptyset(&action.sa_mask);for(int signal:{SIGSEGV,SIGILL,SIGBUS,SIGABRT})sigaction(signal,&action,nullptr);
        Image image;image.load(raw,fixture);image.run();return 0;
    }catch(const std::exception& error){std::cerr<<"GE6_BOOT_TRANSPORT_ERROR "<<error.what()<<'\n';return 1;}
}
