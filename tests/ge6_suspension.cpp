#include "physics_world.h"
#include "vehicle_wheel.h"
#include "vehicle_constraint.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
// Authored single-wheel integration fixture, not a production vehicle. It
// deliberately isolates split penetration: no motor or velocity force phase.
class SuspensionFixture final:public fs15::ge6::VehicleSolver {
    btRigidBody& body_;btRigidBody& ground_;fs15::ge6::WheelContact contact_;
    fs15::ge6::SuspensionConstraint constraint_;fs15::ge6::PenetrationWheel wheel_;
    btSolverBody fixed_{};
public:
    int solves=0;bool wrong_body=false;float rhs=0,projection=0,ray_length=0;
    SuspensionFixture(btRigidBody& b,btRigidBody& g):body_(b),ground_(g){
        // Bullet vector default constructors do not initialize their storage.
        // Match the native zeroed fixed solver body explicitly.
        const btVector3 zero(0,0,0);fixed_.m_worldTransform=ground_.getWorldTransform();
        fixed_.m_deltaLinearVelocity=zero;fixed_.m_deltaAngularVelocity=zero;
        fixed_.m_angularFactor=zero;fixed_.m_linearFactor=zero;fixed_.m_invMass=zero;
        fixed_.m_pushVelocity=zero;fixed_.m_turnVelocity=zero;fixed_.m_linearVelocity=zero;fixed_.m_angularVelocity=zero;
        fixed_.m_externalForceImpulse=zero;fixed_.m_externalTorqueImpulse=zero;fixed_.m_originalBody=nullptr;
    }
    int island()const override{return body_.getIslandTag();}
    void unite_islands(btSimulationIslandManager&)override{}
    void collide(fs15::ge6::World& world)override{
        fs15::ge6::WheelSuspension suspension;suspension.rest=1;suspension.compression=.2f;suspension.droop=.5f;
        contact_=fs15::ge6::raycast_wheel(suspension,body_,world);
        wrong_body=wrong_body||contact_.body!=&ground_;
    }
    void setup(btAlignedObjectArray<btSolverBody>& pool,const btContactSolverInfo& info)override{
        const int index=body_.getCompanionId();require(index>=0&&index<pool.size(),"GE6 did not assign a wheel solver body");
        auto& a=pool[index];fs15::ge6::SuspensionSpring spring;spring.rest=1;spring.compression=.2f;spring.stiffness=1000;spring.positive_velocity_damping=100;spring.nonpositive_velocity_damping=100;
        constraint_.impulse=0;constraint_.push_impulse=0;
        // Native setup passes the geometric distance before its separate
        // wheel readback clamp. Passing contact_.length would hide bottom-out.
        const float geometric_length=a.m_worldTransform.getOrigin().y()-contact_.point.y();
        fs15::ge6::setup_suspension_constraint(constraint_,btVector3(0,-1,0),contact_.point,contact_.normal,btVector3(0,0,0),geometric_length,spring,body_,ground_,a,fixed_,info);
        rhs=constraint_.push_rhs;projection=constraint_.jacobian.projection;ray_length=contact_.length;
        wheel_.a=&a;wheel_.b=&fixed_;wheel_.constraint=&constraint_;wheel_.normal=contact_.normal;wheel_.contact=contact_.body!=nullptr;
    }
    void penetration(btAlignedObjectArray<btSolverBody>&,btScalar)override{
        fs15::ge6::solve_wheel_penetration(&wheel_,1,body_.getActivationState(),true);++solves;
    }
    void velocity(btAlignedObjectArray<btSolverBody>&,btScalar)override{}
    void finish(btAlignedObjectArray<btSolverBody>&,btScalar)override{}
};
}
int main(){try{
    fs15::ge6::PhysicsWorld physics(btVector3(0,0,0));auto& world=physics.world();world.getSolverInfo().m_numIterations=20;world.getSolverInfo().m_splitImpulse=true;world.getSolverInfo().m_erp2=.2f;
    // Isolate split correction all the way to zero penetration. The default
    // negative threshold hands small errors to the not-yet-ported velocity phase.
    world.getSolverInfo().m_splitImpulsePenetrationThreshold=0;
    btSphereShape chassis_shape(.1f);btBoxShape ground_shape(btVector3(5,.1f,5));
    btRigidBody body(1,nullptr,&chassis_shape,btVector3(.004f,.004f,.004f)),ground(0,nullptr,&ground_shape);
    body.setWorldTransform(btTransform(btQuaternion::getIdentity(),btVector3(0,.5f,0)));
    ground.setWorldTransform(btTransform(btQuaternion::getIdentity(),btVector3(0,-.1f,0)));
    world.addRigidBody(&ground);world.addRigidBody(&body);SuspensionFixture fixture(body,ground);world.add_vehicle_solver(fixture);
    world.stepSimulation(1.f/60,1,1.f/60);
    const float first=body.getWorldTransform().getOrigin().y();
    std::cout<<"First correction: y="<<first<<", phases="<<fixture.solves<<", rhs="<<fixture.rhs<<", projection="<<fixture.projection<<", ray length="<<fixture.ray_length<<'\n';
    require(first>.55f&&first<.57f,"GE6 wheel correction did not reach the real body transform through the solver");
    for(int i=0;i<29;++i)world.stepSimulation(1.f/60,1,1.f/60);
    const float settled=body.getWorldTransform().getOrigin().y();
    std::cout<<"After 30 steps: y="<<settled<<", phases="<<fixture.solves<<", rhs="<<fixture.rhs<<'\n';
    require(!fixture.wrong_body&&fixture.solves>30,"GE6 ray/penetration phases were not connected");
    require(settled>.799f&&settled<.801f,"GE6 wheel penetration did not converge to its compression limit");
    require(body.getLinearVelocity().length2()<1e-10f,"Split wheel correction introduced kinetic velocity");
    for(int i=0;i<physics.dispatcher().getNumManifolds();++i)require(physics.dispatcher().getManifoldByIndexInternal(i)->getNumContacts()==0,"Integration fixture relied on chassis collision contacts");
    world.remove_vehicle_solver(fixture);world.removeRigidBody(&body);world.removeRigidBody(&ground);
    std::cout<<"GE6 suspension ray -> spring setup -> penetration iteration -> middleware transform writeback: "<<first<<" -> "<<settled<<"; isolated split phase, not a complete vehicle\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
