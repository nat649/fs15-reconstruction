#pragma once
#include <btBulletDynamicsCommon.h>
#include <BulletDynamics/ConstraintSolver/btSolverBody.h>
#include <cstddef>

namespace fs15::ge6 {
// Authored typed storage for inspected GE6 constraint fields, never a cast of
// a Windows Bullet/vehicle record. Original body and solver fields stay typed.
struct WheelJacobian {
    btVector3 angular_a{0,0,0},angular_b{0,0,0},axis{0,0,0};
    btVector3 component_a{0,0,0},component_b{0,0,0};
    float projection=0;
};
struct WheelJacobianStats {float denominator_a=0,denominator_b=0,velocity_a=0,velocity_b=0;};
// 0x1401511a0. Distinct query and application axes are intentional in GE6.
WheelJacobianStats setup_wheel_jacobian(WheelJacobian&,const btVector3& query_axis,
    const btVector3& contact_point,const btVector3& application_axis,const btVector3& lever,
    const btRigidBody&,const btRigidBody&,const btSolverBody&,const btSolverBody&);
struct SuspensionSpring {
    float rest=0,compression=0,stiffness=0,positive_velocity_damping=0,nonpositive_velocity_damping=0;
};
struct SuspensionConstraint {
    WheelJacobian jacobian;
    float diagonal=0,rhs=0,cfm=0,impulse=0,lower_limit=0;
    float push_diagonal=0,correction_rhs=0,push_rhs=0,push_impulse=0;
};
// 0x140150f50. The caller owns accumulated impulses and their setup reset.
void setup_suspension_constraint(SuspensionConstraint&,const btVector3& query_axis,
    const btVector3& point,const btVector3& contact_normal,const btVector3& lever,float length,const SuspensionSpring&,
    const btRigidBody&,const btRigidBody&,const btSolverBody&,const btSolverBody&,const btContactSolverInfo&);
struct PenetrationWheel {
    SuspensionConstraint* constraint=nullptr;
    btSolverBody* a=nullptr;btSolverBody* b=nullptr;
    btVector3 normal{0,0,0};bool contact=false;
};
// 0x140154d70. A complete split-penetration vehicle phase; setup, normal
// velocity/friction, drivetrain and complete vehicle registration are separate.
void solve_wheel_penetration(PenetrationWheel*,std::size_t count,int activation,bool active);
}
