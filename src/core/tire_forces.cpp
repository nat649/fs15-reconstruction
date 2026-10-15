#include "tire_forces.h"
extern "C" __declspec(dllexport) int fs15_compute_tire_forces(const float* parameters,float sx,float sy,float load,float* result){
    if(!parameters||!result)return 0;
    const fs15::TireParameters p{parameters[0],parameters[1],parameters[2],parameters[3],parameters[4]};
    fs15::TireForces out;
    if(!fs15::tire_forces(p,sx,sy,load,out))return 0;
    result[0]=out.torque;result[1]=out.longitudinal;result[2]=out.lateral;result[3]=out.aligning;
    return 1;
}
