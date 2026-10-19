#pragma once
#include <cstddef>
#include <cstdint>
namespace fs15::ge6 {
// Typed transport for GE6 wheel/differential records. Native child IDs below
// 20 name wheels; IDs 20+ name an earlier differential. Native scratch storage
// has 20 slots. Configuration/ownership validation belongs to the caller.
struct DriveWheel {float radius=0,speed=0,inverse_inertia=0,weight=0;};
struct Differential {float ratio=.5f,bias=1;std::uint16_t a=0,b=0;};
struct VehicleMotor {
    float inverse_inertia=1000,speed=0,target_speed=300,damping=.002f,torque=0,ratio=1;
    float clutch_torque=10,driven_speed=0,torque_impulse=0,clutch_impulse=0;
};
// 0x140155940: differential distribution weights, including coast/reverse.
void prepare_differentials(DriveWheel*,const Differential*,std::size_t,const VehicleMotor&);
// 0x140155d50: complete motor/clutch/differential velocity update.
void solve_drivetrain(DriveWheel*,const Differential*,std::size_t,VehicleMotor&,float dt);
}
