#pragma once
#include "physics_dispatch.h"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
namespace fs15::platform {
/* Authored event/thread transport. Native dispatch/worker control flow stays
 * in the recovered core. Backends outlive this object; no solver is supplied.
 * Calls are serialized. tick waits for completion; callback reentry forbidden.
 * Acknowledging busy here replaces the still-unrecovered post-step finalizer. */
class PhysicsWorker {
public:
    PhysicsWorker(uintptr_t scene,float fixed_step,uint32_t iterations,const FS15PhysicsDispatchServices& backend);
    ~PhysicsWorker();
    PhysicsWorker(const PhysicsWorker&)=delete;
    PhysicsWorker& operator=(const PhysicsWorker&)=delete;
    bool tick(float milliseconds);
    bool stop();
    unsigned completed() const noexcept{return completed_.load();}
private:
    struct Event {
        std::mutex mutex;std::condition_variable condition;bool signaled=false;
        void signal();bool wait(bool bounded=true);
    } request_,completion_;
    FS15PhysicsDispatchState state_{};FS15PhysicsDispatchServices backend_,services_;
    uint8_t stop_=0;std::thread thread_;std::mutex control_;std::atomic<int> result_{-1};std::atomic<unsigned> completed_{0};
    static int simulate(void*,uintptr_t,float,uint32_t,float);
    static uintptr_t world(void*,uintptr_t);
    static int collect(void*,void*,uintptr_t);
    static uint32_t wait(void*,uintptr_t,uint32_t,uint8_t);
    static int signal(void*,uintptr_t);
};
}
