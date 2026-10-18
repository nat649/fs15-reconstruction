#pragma once
#include <btBulletDynamicsCommon.h>
#include <cstddef>

namespace fs15::ge6 {
// Typed transport for inspected Bt2RaycastVehicle wheel fields. This is not
// the Windows object layout and is not yet a complete vehicle solver.
struct WheelSuspension {
    btVector3 attachment{0,0,0},direction{0,-1,0};
    float rest=0,compression=0,droop=0,radius=0;
    short group=1,mask=-1;
};
struct WheelContact {
    const btCollisionObject* body=nullptr;
    btVector3 point{0,0,0},normal{0,0,0};
    float plane=0,length=0;
};
// 0x140150a10: explicit native float association, no normalization.
void wheel_ray_frame(const WheelSuspension&,const btTransform&,btVector3& origin,btVector3& direction);
// 0x140151930 / 0x1400ba820. GE6's wheel filter differs from Bullet's default.
class WheelRayCallback final:public btCollisionWorld::ClosestRayResultCallback {
    const btCollisionObject& owner_;
public:
    WheelRayCallback(const btVector3& from,const btVector3& to,const btCollisionObject& owner,short group,short mask);
    bool needsCollision(btBroadphaseProxy*)const override;
    btScalar addSingleResult(btCollisionWorld::LocalRayResult&,bool normal_in_world) override;
};
// 0x140153330. Calls the middleware ray service; original GIANTS heightfield
// support must be supplied separately before original terrain is validated.
WheelContact raycast_wheel(const WheelSuspension&,const btCollisionObject& owner,const btCollisionWorld&);
struct WheelFinish {
    float speed=0,angle=0,normal_impulse=0,longitudinal_slip=0,lateral_slip=0;
    float force=0,slip[2]{};
};
// 0x140155140. Caller supplies solved impulses; this does not solve suspension.
// Preconditions inherited from solver: positive dt, valid wheel array.
void finish_wheels(WheelFinish*,std::size_t count,int body_activation,bool active,
                   float dt,float motor_impulse,float motor_scale,float& motor_output);
}
