#include "physics_world.h"
#include "vehicle_wheel.h"
#include "vehicle_velocity.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
namespace {
void require(bool v,const char* message){if(!v)throw std::runtime_error(message);}
// Authored one-wheel phase-integration scenario, not a production GE6 vehicle.
// Geometry/orchestration and fixed slip inputs are fixture data. The wheel is
// initially spinning; no motor/drivetrain implementation is substituted here.
class WheelFixture final:public fs15::ge6::VehicleSolver {
    fs15::ge6::World& world_;btRigidBody& body_;btRigidBody& ground_;
    btSolverBody fixed_{};fs15::ge6::WheelContact contact_;
    fs15::ge6::WheelVelocity wheel_;fs15::ge6::PenetrationWheel push_;
    fs15::ge6::WheelFinish readback_;
public:
    int iterations=0;float brake=0;bool missed=false;
    WheelFixture(fs15::ge6::World& world,btRigidBody& body,btRigidBody& ground):world_(world),body_(body),ground_(ground){
        const btVector3 zero(0,0,0);fixed_.m_worldTransform=ground.getWorldTransform();
        fixed_.m_deltaLinearVelocity=zero;fixed_.m_deltaAngularVelocity=zero;fixed_.m_angularFactor=zero;fixed_.m_linearFactor=zero;fixed_.m_invMass=zero;
        fixed_.m_pushVelocity=zero;fixed_.m_turnVelocity=zero;fixed_.m_linearVelocity=zero;fixed_.m_angularVelocity=zero;fixed_.m_externalForceImpulse=zero;fixed_.m_externalTorqueImpulse=zero;fixed_.m_originalBody=nullptr;
        wheel_.tire={.5f,5,1,5,1};wheel_.speed=20;wheel_.inverse_inertia=1;
        wheel_.friction_floor=.5f;wheel_.slip_longitudinal=.3f;wheel_.slip_lateral=.1f;
    }
    int island()const override{return body_.getIslandTag();}
    void unite_islands(btSimulationIslandManager&)override{}
    void collide(fs15::ge6::World& world)override{
        fs15::ge6::WheelSuspension suspension;suspension.rest=1;suspension.compression=.2f;suspension.droop=.5f;suspension.radius=.5f;
        contact_=fs15::ge6::raycast_wheel(suspension,body_,world);missed=missed||contact_.body!=&ground_;
    }
    void setup(btAlignedObjectArray<btSolverBody>& pool,const btContactSolverInfo& info)override{
        const int index=body_.getCompanionId();require(index>=0&&index<pool.size(),"Missing wheel solver body");auto& a=pool[index];wheel_.a=&a;wheel_.b=&fixed_;wheel_.contact=contact_.body!=nullptr;wheel_.normal=contact_.normal;
        const auto lever=contact_.point-a.m_worldTransform.getOrigin();
        const float length=a.m_worldTransform.getOrigin().y()-contact_.point.y()-wheel_.tire.radius;
        fs15::ge6::SuspensionSpring spring;spring.rest=1;spring.compression=.2f;spring.stiffness=1000;spring.positive_velocity_damping=100;spring.nonpositive_velocity_damping=100;
        wheel_.suspension={};wheel_.correction_impulse=0;wheel_.brake=brake;wheel_.brake_impulse=0;
        fs15::ge6::setup_suspension_constraint(wheel_.suspension,btVector3(0,-1,0),contact_.point,contact_.normal,lever,length,spring,body_,ground_,a,fixed_,info);
        auto lateral=fs15::ge6::setup_wheel_jacobian(wheel_.lateral.jacobian,btVector3(1,0,0),contact_.point,btVector3(1,0,0),lever,body_,ground_,a,fixed_);
        wheel_.lateral.diagonal=1.f/(lateral.denominator_a+lateral.denominator_b);wheel_.lateral.rhs=(lateral.velocity_b-lateral.velocity_a)*wheel_.lateral.diagonal;wheel_.lateral.impulse=0;
        auto longitudinal=fs15::ge6::setup_wheel_jacobian(wheel_.longitudinal.jacobian,btVector3(0,0,1),contact_.point,btVector3(0,0,1),lever,body_,ground_,a,fixed_);
        wheel_.longitudinal.diagonal=1.f/((wheel_.tire.radius*wheel_.inverse_inertia)*wheel_.tire.radius+longitudinal.denominator_a+longitudinal.denominator_b);
        wheel_.longitudinal.rhs=(longitudinal.velocity_b-longitudinal.velocity_a)*wheel_.longitudinal.diagonal;wheel_.longitudinal.impulse=0;
        push_.a=&a;push_.b=&fixed_;push_.constraint=&wheel_.suspension;push_.normal=contact_.normal;push_.contact=wheel_.contact;
    }
    void penetration(btAlignedObjectArray<btSolverBody>&,btScalar)override{fs15::ge6::solve_wheel_penetration(&push_,1,body_.getActivationState(),true);}
    void velocity(btAlignedObjectArray<btSolverBody>&,btScalar dt)override{
        fs15::ge6::WheelVelocityServices services;services.context=&world_;services.gravity=[](void* p){return static_cast<fs15::ge6::World*>(p)->getGravity();};
        fs15::ge6::solve_wheel_velocity(&wheel_,1,body_.getActivationState(),true,dt,services);++iterations;
    }
    void finish(btAlignedObjectArray<btSolverBody>&,btScalar dt)override{
        readback_.speed=wheel_.speed;readback_.normal_impulse=wheel_.suspension.impulse;readback_.longitudinal_slip=wheel_.slip_longitudinal;readback_.lateral_slip=wheel_.slip_lateral;
        float motor=0;fs15::ge6::finish_wheels(&readback_,1,body_.getActivationState(),true,dt,0,1,motor);
    }
    float spin()const{return wheel_.speed;}
    float normal_force()const{return readback_.force;}
};
}
int main(){try{
    fs15::ge6::PhysicsWorld physics(btVector3(0,-9.81f,0));auto& world=physics.world();world.getSolverInfo().m_numIterations=20;world.getSolverInfo().m_splitImpulse=true;
    btSphereShape chassis_shape(.1f);btBoxShape ground_shape(btVector3(100,.1f,100));btRigidBody body(1,nullptr,&chassis_shape,btVector3(.004f,.004f,.004f)),ground(0,nullptr,&ground_shape);
    body.setAngularFactor(0);body.setActivationState(DISABLE_DEACTIVATION);body.setWorldTransform(btTransform(btQuaternion::getIdentity(),btVector3(0,1.5f,0)));ground.setWorldTransform(btTransform(btQuaternion::getIdentity(),btVector3(0,-.1f,0)));
    world.addRigidBody(&ground);world.addRigidBody(&body);WheelFixture fixture(world,body,ground);world.add_vehicle_solver(fixture);
    for(int i=0;i<120;++i)world.stepSimulation(1.f/60,1,1.f/60);
    const float rolling=body.getLinearVelocity().z(),travel=body.getWorldTransform().getOrigin().z(),height=body.getWorldTransform().getOrigin().y();
    std::cout<<"Rolling: speed="<<rolling<<", travel="<<travel<<", height="<<height<<", spin="<<fixture.spin()<<", normal force="<<fixture.normal_force()<<'\n';
    require(rolling>1&&travel>.5f,"Recovered tire impulses did not transfer wheel spin to chassis motion");
    require(height>1.47f&&height<1.51f,"Recovered suspension velocity did not support chassis under gravity");
    fixture.brake=80;for(int i=0;i<240;++i)world.stepSimulation(1.f/60,1,1.f/60);
    const float stopped=body.getLinearVelocity().z();std::cout<<"Braked: speed="<<stopped<<", spin="<<fixture.spin()<<", iterations="<<fixture.iterations<<'\n';
    require(std::abs(stopped)<.1f&&std::abs(fixture.spin())<.3f,"Recovered wheel brake/friction did not stop the chassis");
    require(!fixture.missed&&fixture.iterations>=360*20,"Recovered wheel phase did not remain connected");
    for(int i=0;i<physics.dispatcher().getNumManifolds();++i)require(physics.dispatcher().getManifoldByIndexInternal(i)->getNumContacts()==0,"Wheel fixture depended on chassis collision contacts");
    world.remove_vehicle_solver(fixture);world.removeRigidBody(&body);world.removeRigidBody(&ground);return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
