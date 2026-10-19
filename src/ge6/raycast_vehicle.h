#pragma once
#include "physics_world.h"
#include "vehicle_wheel.h"
#include "vehicle_velocity.h"
#include "vehicle_drive.h"
#include <array>
#include <vector>
namespace fs15::ge6 {
struct RaycastWheelConfig {
    WheelSuspension suspension;
    btVector3 axle{1,0,0},force_lever{0,0,0};
    SuspensionSpring spring;
    fs15::TireParameters tire{.5f,5,1,5,1};
    float inverse_inertia=1,damping=0,mass_load=0,steering=0,brake=0,direct_torque=0,drive_mode=0;
    unsigned body=0;
};
// Bt2RaycastVehicle's inspected phases over typed Bullet bodies. Ownership,
// storage and configuration are authored adapters; original XML/script body,
// wheel and joint creation must still feed this class. Stop the worker and
// unregister from World before destruction/configuration changes.
class RaycastVehicle final:public VehicleSolver {
    World& world_;std::vector<btRigidBody*> bodies_;std::vector<RaycastWheelConfig> config_;
    std::vector<Differential> differentials_;
    std::array<WheelVelocity,20> wheels_{};std::array<WheelContact,20> contacts_{};
    std::array<PenetrationWheel,20> penetration_{};std::array<WheelFinish,20> finished_{};
    std::array<DriveWheel,20> driven_{};btSolverBody fixed_{};
    VehicleMotor motor_;bool active_=false;float motor_output_=0;
    void sync_drive();void drive(float);
public:
    explicit RaycastVehicle(World&,std::vector<btRigidBody*>);
    unsigned add_wheel(const RaycastWheelConfig&);
    void set_differentials(std::vector<Differential>);
    RaycastWheelConfig& wheel_config(unsigned i){return config_.at(i);}
    const WheelVelocity& wheel_state(unsigned i)const{return wheels_.at(i);}
    const WheelContact& contact(unsigned i)const{return contacts_.at(i);}
    const WheelFinish& readback(unsigned i)const{return finished_.at(i);}
    VehicleMotor& motor(){return motor_;}
    float motor_output()const{return motor_output_;}
    std::size_t wheel_count()const{return config_.size();}
    void unite_islands(btSimulationIslandManager&)override;
    int island()const override;
    void collide(World&)override;
    void setup(btAlignedObjectArray<btSolverBody>&,const btContactSolverInfo&)override;
    void velocity(btAlignedObjectArray<btSolverBody>&,btScalar)override;
    void penetration(btAlignedObjectArray<btSolverBody>&,btScalar)override;
    void finish(btAlignedObjectArray<btSolverBody>&,btScalar)override;
};
btVector3 steered_wheel_axle(const btVector3& suspension_direction,const btVector3& axle,float angle);
void wheel_tangent_frame(const btVector3& normal,const btVector3& axle,btVector3& longitudinal,btVector3& lateral);
}
