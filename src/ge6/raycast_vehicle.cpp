#include "raycast_vehicle.h"
#include <cmath>
#include <stdexcept>
namespace fs15::ge6 {
namespace {
btVector3 rotate(const btMatrix3x3& m,const btVector3& v){return btVector3((v.x()*m[0][0]+v.y()*m[0][1])+v.z()*m[0][2],(v.x()*m[1][0]+v.y()*m[1][1])+v.z()*m[1][2],(v.x()*m[2][0]+v.y()*m[2][1])+v.z()*m[2][2]);}
void zero_fixed(btSolverBody& s){const btVector3 z(0,0,0);s.m_worldTransform.setIdentity();s.m_deltaLinearVelocity=z;s.m_deltaAngularVelocity=z;s.m_angularFactor=z;s.m_linearFactor=z;s.m_invMass=z;s.m_pushVelocity=z;s.m_turnVelocity=z;s.m_linearVelocity=z;s.m_angularVelocity=z;s.m_externalForceImpulse=z;s.m_externalTorqueImpulse=z;s.m_originalBody=nullptr;}
}
btVector3 steered_wheel_axle(const btVector3& direction,const btVector3& axle,float angle){
    const float half=angle*.5f,x=-direction.x(),y=-direction.y(),z=-direction.z();
    const float scale=std::sin(half)/std::sqrt((x*x+y*y)+z*z),qx=scale*x,qy=scale*y,qz=scale*z,qw=std::cos(half);
    const float tx=(axle.z()*qy+axle.x()*qw)-axle.y()*qz,ty=(axle.x()*qz+axle.y()*qw)-axle.z()*qx,tz=(axle.y()*qx+axle.z()*qw)-axle.x()*qy;
    const float tw=((-axle.x()*qx)-axle.y()*qy)-axle.z()*qz;
    return btVector3(((-qx*tw+tx*qw)+(-qz)*ty)-tz*(-qy),
                     (((-qy)*tw+ty*qw)+tz*(-qx))-(-qz)*tx,
                     (((-qz)*tw+tz*qw)+(-qy)*tx)-ty*(-qx));
}
void wheel_tangent_frame(const btVector3& n,const btVector3& axle,btVector3& longitudinal,btVector3& lateral){
    longitudinal.setValue(axle.y()*n.z()-axle.z()*n.y(),n.x()*axle.z()-axle.x()*n.z(),axle.x()*n.y()-n.x()*axle.y());
    float inverse=1.f/std::sqrt((longitudinal.x()*longitudinal.x()+longitudinal.y()*longitudinal.y())+longitudinal.z()*longitudinal.z());
    longitudinal.setValue(inverse*longitudinal.x(),inverse*longitudinal.y(),inverse*longitudinal.z());
    lateral.setValue(n.y()*longitudinal.z()-n.z()*longitudinal.y(),n.z()*longitudinal.x()-longitudinal.z()*n.x(),longitudinal.y()*n.x()-n.y()*longitudinal.x());
    inverse=1.f/std::sqrt((lateral.x()*lateral.x()+lateral.y()*lateral.y())+lateral.z()*lateral.z());lateral.setValue(lateral.x()*inverse,lateral.y()*inverse,lateral.z()*inverse);
}
RaycastVehicle::RaycastVehicle(World& world,std::vector<btRigidBody*> bodies):world_(world),bodies_(std::move(bodies)){
    if(bodies_.empty()||bodies_.size()>6)throw std::invalid_argument("GE6 vehicle needs 1..6 dynamic bodies");
    for(auto* b:bodies_){if(!b||b->isStaticOrKinematicObject())throw std::invalid_argument("GE6 vehicle body is not dynamic");
        if(world.getCollisionObjectArray().findLinearSearch(b)==world.getCollisionObjectArray().size())throw std::invalid_argument("Add GE6 vehicle bodies to this world before registration");}
    zero_fixed(fixed_);
}
unsigned RaycastVehicle::add_wheel(const RaycastWheelConfig& c){
    if(config_.size()>=20||c.body>=bodies_.size()||c.suspension.direction.length2()==0||c.suspension.direction.cross(c.axle).length2()==0||c.suspension.radius<=0||c.inverse_inertia<=0)throw std::invalid_argument("Invalid GE6 wheel configuration");
    // The native record has one shared radius/rest/compression field. Normalize
    // their typed adapter views once rather than permit inconsistent copies.
    auto normalized=c;normalized.tire.radius=c.suspension.radius;normalized.spring.rest=c.suspension.rest;normalized.spring.compression=c.suspension.compression;
    const auto index=static_cast<unsigned>(config_.size());config_.push_back(normalized);return index;
}
void RaycastVehicle::set_differentials(std::vector<Differential> nodes){
    if(nodes.size()>20)throw std::invalid_argument("GE6 differential scratch limit is 20");
    for(std::size_t i=0;i<nodes.size();++i)for(auto child:{nodes[i].a,nodes[i].b})if(child<20?child>=config_.size():child-20>=i)throw std::invalid_argument("GE6 differential child must be an existing wheel or earlier node");
    differentials_=std::move(nodes);
}
int RaycastVehicle::island()const{return bodies_[0]->getIslandTag();}
void RaycastVehicle::unite_islands(btSimulationIslandManager& manager){
    auto& uf=manager.getUnionFind();for(std::size_t i=1;i<bodies_.size();++i)uf.unite(bodies_[0]->getIslandTag(),bodies_[i]->getIslandTag());
    if(active_)for(std::size_t i=0;i<config_.size();++i)if(contacts_[i].body&&!contacts_[i].body->isStaticOrKinematicObject())uf.unite(bodies_[config_[i].body]->getIslandTag(),contacts_[i].body->getIslandTag());
}
void RaycastVehicle::collide(World& world){
    bool wake=motor_.torque!=0;for(const auto& c:config_)if(c.drive_mode<0&&c.direct_torque!=0)wake=true;
    if(wake)for(auto* b:bodies_)b->activate();
    else{bool sleeping=true;for(auto* b:bodies_)if(b->getActivationState()!=ISLAND_SLEEPING)sleeping=false;if(sleeping){active_=false;return;}}
    active_=true;for(std::size_t i=0;i<config_.size();++i)contacts_[i]=raycast_wheel(config_[i].suspension,*bodies_[config_[i].body],world);
}
void RaycastVehicle::sync_drive(){for(std::size_t i=0;i<config_.size();++i){driven_[i].radius=config_[i].tire.radius;driven_[i].inverse_inertia=config_[i].inverse_inertia;driven_[i].speed=wheels_[i].speed;}}
void RaycastVehicle::drive(float dt){sync_drive();solve_drivetrain(driven_.data(),differentials_.data(),differentials_.size(),motor_,dt);for(std::size_t i=0;i<config_.size();++i)wheels_[i].speed=driven_[i].speed;}
void RaycastVehicle::setup(btAlignedObjectArray<btSolverBody>& pool,const btContactSolverInfo& info){
    if(bodies_[0]->getActivationState()==ISLAND_SLEEPING)return;
    const bool refresh=!active_;if(refresh){for(std::size_t i=0;i<config_.size();++i)contacts_[i]=raycast_wheel(config_[i].suspension,*bodies_[config_[i].body],world_);active_=true;}
    for(std::size_t i=0;i<config_.size();++i){const auto& c=config_[i];auto& w=wheels_[i];auto& hit=contacts_[i];
        w.slip_longitudinal=0;w.slip_lateral=0;w.friction_floor=.25f;w.suspension.impulse=0;w.correction_impulse=0;w.suspension.push_impulse=0;w.lateral.impulse=0;w.longitudinal.impulse=0;w.brake_impulse=0;
        const float factor=info.m_timeStep*c.inverse_inertia;if(c.drive_mode<0)w.speed=factor*c.direct_torque+w.speed;w.speed/=factor*c.damping+1.f;
        w.tire=c.tire;w.inverse_inertia=c.inverse_inertia;w.brake=c.brake;w.mass_load=c.mass_load;w.contact=hit.body!=nullptr;hit.length=c.suspension.rest+c.suspension.droop;
        if(!hit.body)continue;
        auto& body=*bodies_[c.body];const auto* other=btRigidBody::upcast(hit.body);auto& a=pool[body.getCompanionId()];auto& b=other->getCompanionId()<0||refresh?fixed_:pool[other->getCompanionId()];w.a=&a;w.b=&b;w.normal=hit.normal;
        btVector3 from,direction;wheel_ray_frame(c.suspension,a.m_worldTransform,from,direction);
        const auto axle=rotate(a.m_worldTransform.getBasis(),steered_wheel_axle(c.suspension.direction,c.axle,c.steering));const auto lever=rotate(a.m_worldTransform.getBasis(),c.force_lever);
        const float length=(((hit.point.x()-from.x())*direction.x()+direction.y()*(hit.point.y()-from.y()))+direction.z()*(hit.point.z()-from.z()))-c.tire.radius;
        float limit=c.suspension.rest-c.suspension.compression;if(limit<=length){limit=c.suspension.rest+c.suspension.droop;if(length<=limit)limit=length;}hit.length=limit;
        setup_suspension_constraint(w.suspension,direction,hit.point,hit.normal,lever,length,c.spring,body,*other,a,b,info);
        btVector3 longitudinal,lateral;wheel_tangent_frame(hit.normal,axle,longitudinal,lateral);
        const auto lat=setup_wheel_jacobian(w.lateral.jacobian,lateral,hit.point,lateral,lever,body,*other,a,b);
        w.lateral.diagonal=1.f/(lat.denominator_a+lat.denominator_b);w.lateral.rhs=(lat.velocity_b-lat.velocity_a)*w.lateral.diagonal;
        const auto lon=setup_wheel_jacobian(w.longitudinal.jacobian,longitudinal,hit.point,longitudinal,lever,body,*other,a,b);
        w.longitudinal.diagonal=1.f/(((c.tire.radius*c.inverse_inertia)*c.tire.radius+lon.denominator_a)+lon.denominator_b);w.longitudinal.rhs=(lon.velocity_b-lon.velocity_a)*w.longitudinal.diagonal;
        const float lateral_speed=lat.velocity_a-lat.velocity_b,abs_lateral=std::abs(lateral_speed),signed_speed=lon.velocity_a-lon.velocity_b;
        float speed=std::abs(signed_speed),rim=c.tire.radius*w.speed;if(signed_speed<0)rim=-rim;
        w.slip_lateral=abs_lateral<speed*570.f?lateral_speed/speed:(lateral_speed<0?-570.f:570.f);
        if(speed<rim)w.slip_longitudinal=rim-speed<rim?(rim-speed)/rim:1.f;else w.slip_longitudinal=speed-rim<speed?(speed-rim)/speed:1.f;
        if(.2f<=speed){if(abs_lateral<.05f){const float bias=0<=w.slip_lateral?.36f:-.36f;w.slip_lateral=(w.slip_lateral-bias)*(abs_lateral/.05f)+bias;}}
        else{const float bias=0<=w.slip_longitudinal?.2f:-.2f,latbias=0<=w.slip_lateral?.36f:-.36f;w.slip_lateral=(w.slip_lateral-latbias)*(speed/.2f)+latbias;w.slip_longitudinal=(w.slip_longitudinal-bias)*(speed/.2f)+bias;}
        if(speed<abs_lateral)speed=abs_lateral;if(speed<.05f)w.friction_floor=.9f-(speed/.05f)*.65f;
        penetration_[i].constraint=&w.suspension;penetration_[i].a=&a;penetration_[i].b=&b;penetration_[i].normal=hit.normal;
    }
    motor_.torque_impulse=0;motor_.clutch_impulse=0;const float damping=motor_.torque==0||motor_.target_speed<=motor_.speed?motor_.damping:0.f;
    motor_.speed/=((damping*motor_.inverse_inertia)*info.m_timeStep)+1.f;sync_drive();prepare_differentials(driven_.data(),differentials_.data(),differentials_.size(),motor_);
}
void RaycastVehicle::velocity(btAlignedObjectArray<btSolverBody>&,btScalar dt){
    WheelVelocityServices services;services.context=this;services.drive=[](void* p,float t){static_cast<RaycastVehicle*>(p)->drive(t);};services.gravity=[](void* p){return static_cast<RaycastVehicle*>(p)->world_.getGravity();};solve_wheel_velocity(wheels_.data(),config_.size(),bodies_[0]->getActivationState(),active_,dt,services);
}
void RaycastVehicle::penetration(btAlignedObjectArray<btSolverBody>&,btScalar){
    for(std::size_t i=0;i<config_.size();++i)penetration_[i].contact=contacts_[i].body!=nullptr;solve_wheel_penetration(penetration_.data(),config_.size(),bodies_[0]->getActivationState(),active_);
}
void RaycastVehicle::finish(btAlignedObjectArray<btSolverBody>&,btScalar dt){
    // The earlier finish transport's inferred names precede recovery of the
    // velocity phase: native public slip[0] reads record +0x10 (lateral),
    // while slip[1] reads +0x14 (longitudinal). Preserve those native lanes.
    for(std::size_t i=0;i<config_.size();++i){finished_[i].speed=wheels_[i].speed;finished_[i].normal_impulse=wheels_[i].suspension.impulse;finished_[i].longitudinal_slip=wheels_[i].slip_lateral;finished_[i].lateral_slip=wheels_[i].slip_longitudinal;}
    finish_wheels(finished_.data(),config_.size(),bodies_[0]->getActivationState(),active_,dt,motor_.torque_impulse,motor_.inverse_inertia,motor_output_);
}
}
