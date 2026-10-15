#pragma once
#include <algorithm>
#include <cmath>

namespace fs15 {
// Names/units are inferred from native 0x1400c24c0 and its wheel-record fields.
// This routine alone does not provide contacts, wheel slip or a rigid-body solver.
struct TireParameters {
    float radius, lateral_stiffness, reference_load, longitudinal_stiffness, friction;
};
struct TireForces { float torque=0, longitudinal=0, lateral=0, aligning=0; };
inline bool tire_forces(const TireParameters& p,float sx,float sy,float load,TireForces& out){
    out={};
    for(float v:{p.radius,p.lateral_stiffness,p.reference_load,p.longitudinal_stiffness,p.friction,sx,sy,load})
        if(!std::isfinite(v))return false;
    if(p.radius<=0||p.lateral_stiffness<=0||p.reference_load<=0||p.longitudinal_stiffness<=0||p.friction<=0||load<0)return false;
    if(std::abs(sx)<=1e-5f&&std::abs(sy)<=1e-5f)return true;
    load=load>0.02f?(load<1.0f?(load-0.02f)/0.98f+0.02f:load):0.02f;
    const float ratio=load/p.reference_load;
    const float sensitivity=std::clamp((ratio*3.0f-ratio*ratio*3.0f)+ratio*ratio*ratio,0.2f,1.0f)*p.lateral_stiffness;
    const float friction_load=p.friction*load;
    const float stiffness=p.longitudinal_stiffness;
    const float combined=std::sqrt(stiffness*sx*stiffness*sx+sensitivity*sy*sensitivity*sy)/friction_load;
    float weight=1.0f;
    if(combined<=6.2831854820251465f)
        weight=((sensitivity/stiffness+1.0f)-std::cos(combined*0.5f)*(1.0f-sensitivity/stiffness))*0.5f;
    const float weighted_sy=weight*sy;
    const float magnitude=std::sqrt(weighted_sy*weight*sy+sx*sx);
    const float quadratic=combined*(1.0f/3.0f)*combined;
    const float cubic=combined*(1.0f/27.0f)*combined*combined;
    const float gain=std::min(1.0f,(combined-quadratic)+cubic)*(friction_load/magnitude);
    out.longitudinal=gain*sx;
    out.lateral=-(weighted_sy*gain);
    out.torque=-(out.longitudinal*p.radius);
    out.aligning=(((combined-combined*combined)+quadratic*combined)-cubic*combined)*weighted_sy*(friction_load/magnitude);
    const float minimum=friction_load*0.03f;
    if(std::abs(out.longitudinal)<minimum)out.longitudinal=out.longitudinal<0?-minimum:minimum;
    if(std::abs(out.lateral)<minimum)out.lateral=out.lateral<0?-minimum:minimum;
    for(float v:{out.torque,out.longitudinal,out.lateral,out.aligning})
        if(!std::isfinite(v)){out={};return false;}
    return true;
}
}
