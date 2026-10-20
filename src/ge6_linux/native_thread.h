#pragma once
// Authored NT_TIB stack/self prefix for original instructions on each pthread.
#include <array>
#include <asm/prctl.h>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <pthread.h>
#include <stdexcept>
#include <sys/syscall.h>
#include <unistd.h>
namespace ge6_native {
struct Context {
    alignas(16) std::array<uint8_t,0x100> tib{};uintptr_t previous_gs=0;bool installed=false;
    Context(){
        pthread_attr_t attr;if(pthread_getattr_np(pthread_self(),&attr))throw std::runtime_error("Cannot inspect Linux thread stack");
        void* stack=nullptr;size_t bytes=0;int result=pthread_attr_getstack(&attr,&stack,&bytes);pthread_attr_destroy(&attr);if(result||!bytes)throw std::runtime_error("Cannot inspect Linux stack bounds");
        uintptr_t limit=reinterpret_cast<uintptr_t>(stack),upper=limit+bytes,self=reinterpret_cast<uintptr_t>(tib.data()),none=~uintptr_t(0);
        std::memcpy(tib.data(),&none,8);std::memcpy(tib.data()+8,&upper,8);std::memcpy(tib.data()+0x10,&limit,8);std::memcpy(tib.data()+0x30,&self,8);
        if(syscall(SYS_arch_prctl,ARCH_GET_GS,&previous_gs)||syscall(SYS_arch_prctl,ARCH_SET_GS,self)){throw std::runtime_error("Cannot install native NT_TIB prefix");}installed=true;
    }
    ~Context(){if(installed){auto result=syscall(SYS_arch_prctl,ARCH_SET_GS,previous_gs);(void)result;}}
    Context(const Context&)=delete;
};
}
