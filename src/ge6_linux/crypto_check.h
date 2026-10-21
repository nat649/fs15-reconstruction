#pragma once
// Authored provider, lifetime and system-counter contracts; no game code.
#include "crypto_services.h"
#include "system_services.h"
#include <array>
#include <stdexcept>
#include <time.h>
namespace ge6_entropy_test {
inline void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
inline void check(uint32_t& error){
    using namespace ge6_crypto;
    uintptr_t handle=123;
    require(!acquire(nullptr,nullptr,nullptr,24,0xf0000000u)&&error==87,"Null crypto output accepted");
    require(!acquire(&handle,"unsupported-container",nullptr,24,0xf0000000u)&&!handle&&error==50,"Unsupported key container reported available");
    require(acquire(&handle,nullptr,nullptr,24,0xf0000000u)&&handle,"Actual OpenSSL provider unavailable");
    std::array<uint8_t,66> first{},second{};first.front()=0xa5;first.back()=0x5a;
    require(random(handle,64,first.data()+1)&&random(handle,64,second.data()+1),"Actual OpenSSL random request failed");
    require(first.front()==0xa5&&first.back()==0x5a&&std::memcmp(first.data()+1,second.data()+1,64)!=0,"Random buffer bounds or independent requests differ");
    require(random(handle,0,nullptr)&&!random(handle,1,nullptr)&&error==87,"Random empty/null contracts differ");
    require(!release(handle,1)&&error==87&&random(handle,1,second.data())&&release(handle,0)&&contexts.empty(),"Provider flags or lifetime differ");
    require(!release(handle,0)&&error==0x80090001u&&!random(handle,1,second.data())&&error==0x80090001u,"Released provider remained usable");
    using namespace ge6_system;
    uint32_t count=0;require(!user_name_a(nullptr,&count)&&count>1&&error==122,"ANSI account length query differs");
    std::vector<char> name(count+1,char(0x5a));auto capacity=count;require(user_name_a(name.data(),&capacity)&&capacity==count&&name[count]==0x5a,"ANSI account bounds differ");
    char hostname[256]{};require(gethostname(hostname,sizeof(hostname)-1)==0,"Actual hostname unavailable");count=0;
    if(std::strlen(hostname)<=15){require(!computer_name(nullptr,&count)&&error==111&&count==std::strlen(hostname)+1,"Host-name required length differs");std::vector<char> output(count+1,char(0x5a));capacity=count;require(computer_name(output.data(),&capacity)&&std::string(output.data())==hostname&&capacity+1==count&&output[count]==0x5a,"Host-name bytes/count/bounds differ");}
    else require(!computer_name(nullptr,&count)&&error==50,"Unrepresentable NetBIOS name synthesized");
    uint64_t created=0,exited=999,kernel=0,user=0,now=0;
    require(process_times(~uintptr_t(0),&created,&exited,&kernel,&user)&&exited==0,"Actual process times unavailable");timespec clock{};clock_gettime(CLOCK_REALTIME,&clock);now=(uint64_t(clock.tv_sec)+11644473600ull)*10000000+uint64_t(clock.tv_nsec)/100;
    require(created>116444736000000000ull&&created<=now,"Process creation epoch differs");
    auto previous_kernel=kernel,previous_user=user;
    require(process_times(~uintptr_t(0),&created,&exited,&kernel,&user)&&kernel>=previous_kernel&&user>=previous_user,"Process CPU counters moved backwards");
    require(!process_times(0,&created,&exited,&kernel,&user)&&error==6&&!process_times(~uintptr_t(0),nullptr,&exited,&kernel,&user)&&error==87,"Invalid process time request accepted");
    uint64_t idle=0;require(system_times(&idle,&kernel,&user)&&idle<=kernel&&kernel>0,"Actual system CPU times differ");
    require(!system_times(nullptr,&kernel,&user)&&error==87,"Null system CPU output accepted");
    uint32_t unavailable=0x12345678;require(!hardware_profile_unavailable(&unavailable)&&error==50&&unavailable==0x12345678,"Unavailable Windows hardware profile synthesized");
    require(!registry_quota_unavailable(&unavailable,&unavailable)&&error==50&&unavailable==0x12345678&&adapter_info_unavailable(nullptr,&unavailable)==50&&unavailable==0x12345678,"Unavailable Windows inventory synthesized");
}
}
