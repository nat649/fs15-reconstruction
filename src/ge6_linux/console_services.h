#pragma once
// Authored console-control transport. Only SIGINT/CTRL_C and SIGQUIT/CTRL_BREAK.
// The POSIX signal handler only writes a byte; PE callbacks run on fresh threads.
#include "native_thread.h"
#include "thread_slots.h"
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <fcntl.h>
#include <iterator>
#include <mutex>
#include <signal.h>
#include <thread>
#include <vector>
namespace ge6_console {
#define GE6_CONSOLE_MS __attribute__((ms_abi))
using Handler=int(GE6_CONSOLE_MS*)(uint32_t);
inline void(*trace)(const char*)=nullptr;
inline void(*set_error)(uint32_t)=nullptr;
inline void(*record)(const char*,uintptr_t,uint32_t)=nullptr;
inline std::atomic<int> signal_fd{-1};
inline std::atomic<bool> ignore_ctrl_c{false};
static_assert(decltype(signal_fd)::is_always_lock_free&&decltype(ignore_ctrl_c)::is_always_lock_free);
inline void receive(int signal){
    int saved=errno;
    if(signal!=SIGINT||!ignore_ctrl_c.load(std::memory_order_relaxed)){
        int fd=signal_fd.load(std::memory_order_relaxed);unsigned char code=signal==SIGINT?1:2;
        if(fd>=0){ssize_t written;do{written=write(fd,&code,1);}while(written<0&&errno==EINTR);}
    }
    errno=saved;
}
struct Service {
    std::mutex mutex;std::vector<Handler> handlers;int pipe[2]{-1,-1};
    struct sigaction previous[2]{};bool started=false,stopping=false;std::thread dispatcher;
    bool start(){ // registration mutex held
        if(started)return !stopping;
        if(pipe2(pipe,O_CLOEXEC))return false;
        if(fcntl(pipe[1],F_SETFL,O_NONBLOCK)<0){close(pipe[0]);close(pipe[1]);pipe[0]=pipe[1]=-1;return false;}
        struct sigaction action{};action.sa_handler=receive;action.sa_flags=SA_RESTART;sigemptyset(&action.sa_mask);
        signal_fd.store(pipe[1]);
        if(sigaction(SIGINT,&action,&previous[0])){signal_fd.store(-1);close(pipe[0]);close(pipe[1]);pipe[0]=pipe[1]=-1;return false;}
        if(sigaction(SIGQUIT,&action,&previous[1])){signal_fd.store(-1);sigaction(SIGINT,&previous[0],nullptr);close(pipe[0]);close(pipe[1]);pipe[0]=pipe[1]=-1;return false;}
        try{dispatcher=std::thread([this](){run();});}
        catch(...){signal_fd.store(-1);sigaction(SIGINT,&previous[0],nullptr);sigaction(SIGQUIT,&previous[1],nullptr);close(pipe[0]);close(pipe[1]);pipe[0]=pipe[1]=-1;return false;}
        started=true;return true;
    }
    void run(){
        unsigned char code=0;
        for(;;){
            auto count=read(pipe[0],&code,1);if(count<0&&errno==EINTR)continue;
            std::vector<Handler> snapshot;
            {std::lock_guard<std::mutex> lock(mutex);if(stopping||count!=1||!code)return;snapshot=handlers;}
            // Windows dispatches control handlers on another thread. Serialize
            // queued notifications, but give each one a fresh GS/TLS/FLS context.
            try{std::thread worker([snapshot,code](){
                try{ge6_native::Context context;bool handled=false;
                    for(auto it=snapshot.rbegin();it!=snapshot.rend();++it){record("CALL",reinterpret_cast<uintptr_t>(*it),code-1);if((*it)(code-1)){handled=true;break;}}
                    ge6_slots::cleanup();
                    if(!handled)std::_Exit(128+(code==1?SIGINT:SIGQUIT)); // Linux exit status, not Windows status parity.
                }catch(...){std::_Exit(125);}
            });worker.join();}catch(...){std::_Exit(125);}
        }
    }
    void shutdown(){
        {std::lock_guard<std::mutex> lock(mutex);if(!started||stopping)return;stopping=true;
            signal_fd.store(-1);sigaction(SIGINT,&previous[0],nullptr);sigaction(SIGQUIT,&previous[1],nullptr);
            unsigned char quit=0;auto written=write(pipe[1],&quit,1);(void)written;
        }
        dispatcher.join();
        // Keep the pipe descriptors owned until process exit: an already-entered
        // POSIX signal handler may still hold the old fd. Never recycle it beneath
        // that handler. Shutdown is terminal, not a reusable console attach API.
    }
    ~Service(){shutdown();} // OS reclaims the two retained descriptors at exit.
};
inline Service& service(){static Service state;return state;}
inline GE6_CONSOLE_MS int set_handler(Handler handler,int add){
    trace("SetConsoleCtrlHandler");auto& state=service();std::lock_guard<std::mutex> lock(state.mutex);
    if(state.stopping){set_error(50);return 0;}
    if(!handler){if(!state.start()){set_error(8);return 0;}ignore_ctrl_c.store(add!=0);return 1;}
    if(!add){auto found=std::find(state.handlers.rbegin(),state.handlers.rend(),handler);if(found==state.handlers.rend()){set_error(87);return 0;}state.handlers.erase(std::next(found).base());record("REMOVE",reinterpret_cast<uintptr_t>(handler),0);return 1;}
    if(!state.start()){set_error(8);return 0;}
    state.handlers.push_back(handler);record("REGISTER",reinterpret_cast<uintptr_t>(handler),0);return 1;
}
struct Lifetime {~Lifetime(){service().shutdown();}}; // Destroy before the PE mapping.
#undef GE6_CONSOLE_MS
}
