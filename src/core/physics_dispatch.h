#pragma once
#include "export.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Inspected manager fields, not a complete manager constructor or solver. */
typedef struct FS15PhysicsDispatchState {
    uint8_t unknown00[0x38];uintptr_t scene;
    uint8_t unknown40[0x68];uint8_t active,unknowna9[7];
    float fixed_step;uint32_t iterations;
    uint8_t unknownb8[8];float clamped_ms;uint8_t unknownc4[4];float elapsed_ms;
    uint8_t unknowncc[0x37c];uint8_t busy,unknown449[0x2f];
    uintptr_t request_event,completion_event;float pending_seconds;
    uint8_t asynchronous,unknown48d[3];
} FS15PhysicsDispatchState;
/* Authored services for native scene slots +0x60, scene backend +0x60/slot
 * +0x48, observation collection 0x140144ce0, and Win32 event APIs. Scene/world
 * tokens are identities and never executed. Callbacks must not throw/reenter.
 * Wait gets native INFINITE/alertable=0; zero means success. Native ignores
 * OS errors; authored guards stop on failed transport, retaining prior writes. */
typedef struct FS15PhysicsDispatchServices {
    void* user;
    int (*simulate)(void*,uintptr_t,float,uint32_t,float);
    uintptr_t (*world)(void*,uintptr_t);
    int (*collect)(void*,void*,uintptr_t);
    uint32_t (*wait)(void*,uintptr_t,uint32_t,uint8_t);
    int (*signal)(void*,uintptr_t);
} FS15PhysicsDispatchServices;
/* 0x1400c3c50: synchronous scene step then world/observation collection. */
FS15_CORE_API int fs15_physics_step(FS15PhysicsDispatchState*,size_t,float,const FS15PhysicsDispatchServices*);
/* 0x1400c7a60: native active/busy/positive-delta gate, two millisecond clocks,
 * capped step, seconds conversion and either worker signal or inline step.
 * Native post-step 0x1400c7b20, which clears busy, remains unrecovered. */
FS15_CORE_API int fs15_physics_dispatch(FS15PhysicsDispatchState*,size_t,float,const FS15PhysicsDispatchServices*);
/* 0x1400c3b90: wait -> stop check -> step/collect -> clear pending seconds ->
 * completion signal -> wait. stop points to caller-owned byte, synchronized
 * through the request event. One outstanding request; stop changes only after
 * completion or before initial wake. No unsynchronized concurrent field writes. */
FS15_CORE_API int fs15_physics_worker(FS15PhysicsDispatchState*,size_t,const uint8_t* stop,const FS15PhysicsDispatchServices*);
#ifdef __cplusplus
}
static_assert(sizeof(FS15PhysicsDispatchState)==0x490);
static_assert(offsetof(FS15PhysicsDispatchState,active)==0xa8);
static_assert(offsetof(FS15PhysicsDispatchState,fixed_step)==0xb0);
static_assert(offsetof(FS15PhysicsDispatchState,clamped_ms)==0xc0);
static_assert(offsetof(FS15PhysicsDispatchState,busy)==0x448);
static_assert(offsetof(FS15PhysicsDispatchState,request_event)==0x478);
static_assert(offsetof(FS15PhysicsDispatchState,pending_seconds)==0x488);
#endif
