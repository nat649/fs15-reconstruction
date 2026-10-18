#include "vehicle_wheel.h"

namespace fs15::ge6 {
void wheel_ray_frame(const WheelSuspension& w,const btTransform& t,btVector3& p,btVector3& d){
    const auto& m=t.getBasis();const auto& o=t.getOrigin();
    d.setValue((w.direction.y()*m[0][1]+w.direction.x()*m[0][0])+w.direction.z()*m[0][2],
               (w.direction.y()*m[1][1]+w.direction.x()*m[1][0])+w.direction.z()*m[1][2],
               (w.direction.x()*m[2][0]+w.direction.y()*m[2][1])+w.direction.z()*m[2][2]);
    p.setValue(((w.attachment.y()*m[0][1]+w.attachment.x()*m[0][0])+w.attachment.z()*m[0][2])+o.x(),
               ((w.attachment.y()*m[1][1]+w.attachment.x()*m[1][0])+w.attachment.z()*m[1][2])+o.y(),
               ((w.attachment.x()*m[2][0]+w.attachment.y()*m[2][1])+w.attachment.z()*m[2][2])+o.z());
}
WheelRayCallback::WheelRayCallback(const btVector3& from,const btVector3& to,const btCollisionObject& owner,short group,short mask)
    :ClosestRayResultCallback(from,to),owner_(owner){m_collisionFilterGroup=group;m_collisionFilterMask=mask;}
bool WheelRayCallback::needsCollision(btBroadphaseProxy* proxy)const{
    // Literal recovered OR of same-field filter bits. Do not substitute the
    // stock cross-field AND; native differential fixtures exercise both.
    if(!(proxy->m_collisionFilterMask&m_collisionFilterMask)&&!(proxy->m_collisionFilterGroup&m_collisionFilterGroup))return false;
    const auto* object=static_cast<const btCollisionObject*>(proxy->m_clientObject);
    return object!=&owner_&&object->hasContactResponse()&&(object->getInternalType()&btCollisionObject::CO_RIGID_BODY)!=0&&owner_.checkCollideWith(object);
}
btScalar WheelRayCallback::addSingleResult(btCollisionWorld::LocalRayResult& r,bool in_world){
    m_closestHitFraction=r.m_hitFraction;m_collisionObject=r.m_collisionObject;
    if(in_world)m_hitNormalWorld=r.m_hitNormalLocal;
    else{
        const auto& m=r.m_collisionObject->getWorldTransform().getBasis();const auto& n=r.m_hitNormalLocal;
        // The callback uses y,x,z association on all three rows.
        m_hitNormalWorld.setValue((n.y()*m[0][1]+n.x()*m[0][0])+n.z()*m[0][2],
                                  (n.y()*m[1][1]+n.x()*m[1][0])+n.z()*m[1][2],
                                  (n.y()*m[2][1]+n.x()*m[2][0])+n.z()*m[2][2]);
    }
    const float inverse=1.f-r.m_hitFraction;
    m_hitPointWorld.setValue(inverse*m_rayFromWorld.x()+r.m_hitFraction*m_rayToWorld.x(),
                             inverse*m_rayFromWorld.y()+r.m_hitFraction*m_rayToWorld.y(),
                             inverse*m_rayFromWorld.z()+r.m_hitFraction*m_rayToWorld.z());
    return r.m_hitFraction;
}
WheelContact raycast_wheel(const WheelSuspension& w,const btCollisionObject& owner,const btCollisionWorld& world){
    btVector3 from,d;wheel_ray_frame(w,owner.getWorldTransform(),from,d);
    const float distance=(w.rest+w.droop)+w.radius;
    const btVector3 to(from.x()+d.x()*distance,from.y()+d.y()*distance,from.z()+d.z()*distance);
    WheelRayCallback ray(from,to,owner,w.group,w.mask);world.rayTest(from,to,ray);
    WheelContact hit;hit.body=ray.m_collisionObject;
    if(!hit.body){hit.point=to;hit.normal.setValue(-d.x(),-d.y(),-d.z());hit.length=w.rest+w.droop;return hit;}
    hit.point=ray.m_hitPointWorld;hit.normal=ray.m_hitNormalWorld;
    hit.plane=-((hit.normal.y()*hit.point.y()+hit.point.x()*hit.normal.x())+hit.normal.z()*hit.point.z());
    const float length=(((hit.point.y()-from.y())*d.y()+(hit.point.x()-from.x())*d.x())+(hit.point.z()-from.z())*d.z())-w.radius;
    // Preserve the native comparison order (including unordered values).
    float limit=w.rest-w.compression;
    if(limit<=length){limit=w.rest+w.droop;if(length<=limit)limit=length;}
    hit.length=limit;return hit;
}
void finish_wheels(WheelFinish* wheels,std::size_t count,int activation,bool active,float dt,float motor_impulse,float motor_scale,float& motor_output){
    if(activation==ISLAND_SLEEPING||!active)return;
    constexpr float turn=6.283185482025146484375f;
    for(std::size_t i=0;i<count;++i){auto& w=wheels[i];
        float angle=dt*w.speed+w.angle;
        if(angle>turn)angle-=turn;else if(angle<0)angle+=turn;
        w.angle=angle;w.force=-(w.normal_impulse/dt);w.slip[0]=w.longitudinal_slip;w.slip[1]=w.lateral_slip;
    }
    motor_output=motor_impulse/(dt*motor_scale);
}
}
