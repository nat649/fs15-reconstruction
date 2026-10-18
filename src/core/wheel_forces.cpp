#include "wheel_forces.h"
#include "wheel_record.h"
#include "tire_forces.h"
#include <cstring>
extern "C" int fs15_script_wheel_forces(const void* manager,const void* object,void* frame,size_t bytes,void* user,float (*tangent)(void*,float)){
    if(!manager||!object||!frame||bytes<0x130)return 0;
    auto* raw=static_cast<uint8_t*>(frame);int32_t id;float sx,angle,load;
    std::memcpy(&id,raw,4);std::memcpy(&sx,raw+0x10,4);std::memcpy(&angle,raw+0x20,4);std::memcpy(&load,raw+0x30,4);
    if(!std::isfinite(sx)||!std::isfinite(angle)||!std::isfinite(load))return 0;
    const auto* r=static_cast<const fs15::WheelRecord*>(fs15_resolve_wheel_record(manager,object,id,nullptr));
    fs15::TireForces out;
    if(r&&load>1e-5f){
        const float sy=tangent?tangent(user,angle):std::tan(angle);
        const fs15::TireParameters p{r->radius,r->lateral_stiffness,r->reference_load,r->longitudinal_stiffness,r->friction};
        if(!fs15::tire_forces(p,sx,sy,load,out))return 0;
    }
    const uint16_t tag=3;
    std::memcpy(raw+0xf0,&out.longitudinal,4);std::memcpy(raw+0x100,&out.lateral,4);
    std::memcpy(raw+0xf8,&tag,2);std::memcpy(raw+0x108,&tag,2);return 1;
}
