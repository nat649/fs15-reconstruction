#pragma once
#include "vehicle_constraint.h"
#include "tire_forces.h"
namespace fs15::ge6 {
struct WheelFrictionConstraint {
    WheelJacobian jacobian;
    float diagonal=0,rhs=0,impulse=0;
};
struct WheelVelocity {
    btSolverBody* a=nullptr;btSolverBody* b=nullptr;
    SuspensionConstraint suspension;
    float correction_impulse=0;
    WheelFrictionConstraint lateral,longitudinal;
    fs15::TireParameters tire{};
    btVector3 normal{0,0,0};
    float mass_load=0,friction_floor=0,slip_longitudinal=0,slip_lateral=0;
    float speed=0,inverse_inertia=0,brake=0,brake_impulse=0;
    bool contact=false;
};
// Explicit typed services for the original pre-wheel drivetrain call and
// world gravity query. A null drive means no differential network configured;
// it must not be presented as a recovered working drivetrain. Gravity is
// required when an active wheel has contact. Wheels/solver bodies are borrowed.
struct WheelVelocityServices {
    void* context=nullptr;
    void (*drive)(void*,float)=nullptr;
    btVector3 (*gravity)(void*)=nullptr;
};
// Bounded recovery of 0x1401540b0 over the existing recovered tire-force leaf.
// Finite valid tire parameters and positive dt are inherited caller contracts.
// Differential/motor implementation, complete vehicle setup and registration
// remain separate from this wheel velocity/friction phase.
void solve_wheel_velocity(WheelVelocity*,std::size_t count,int activation,bool active,float dt,const WheelVelocityServices&);
}
