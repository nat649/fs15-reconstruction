#include "physics_world.h"
#include "vehicle_wheel.h"
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>
extern "C" int fs15_ge6_sphere_drop(float,float,float,float,int,float,float*,size_t);
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Vehicle:fs15::ge6::VehicleSolver {
    int tag=0,collisions=0,islands=0,setups=0,velocities=0,pushes=0,finishes=0;
    void unite_islands(btSimulationIslandManager&) override{++islands;}
    int island()const override{return tag;}
    void collide(fs15::ge6::World&) override{++collisions;}
    void setup(btAlignedObjectArray<btSolverBody>&,const btContactSolverInfo&) override{++setups;}
    void velocity(btAlignedObjectArray<btSolverBody>&,btScalar) override{++velocities;}
    void penetration(btAlignedObjectArray<btSolverBody>&,btScalar) override{++pushes;}
    void finish(btAlignedObjectArray<btSolverBody>&,btScalar) override{++finishes;}
};
int main(){try{
    constexpr int frames=240;std::vector<float> state(frames*7);
    require(fs15_ge6_sphere_drop(-9.81f,8,1,1,frames,1.f/60,state.data(),state.size())==1,"GE6 world/solver creation failed");
    require(std::abs(state[1]-7.997275f)<1e-5f&&state[31*7+4]<-5,"Gravity did not accumulate");
    bool contact=false;for(int i=0;i<frames;++i){for(int j=0;j<7;++j)require(std::isfinite(state[i*7+j]),"Nonfinite physics result");contact=contact||state[i*7+6]>0;require(state[i*7+1]>.8f,"Sphere passed through static collider");}
    require(contact&&std::abs(state[(frames-1)*7+1]-1)<.002f,"Rigid-body collision did not settle");
    fs15::ge6::PhysicsWorld p(btVector3(0,-9.81f,0));auto& w=p.world();Vehicle vehicle;w.add_vehicle_solver(vehicle);
    bool rejected=false;try{w.add_vehicle_solver(vehicle);}catch(...){rejected=true;}require(rejected,"Duplicate vehicle solver accepted");
    btSphereShape shape(1);btRigidBody::btRigidBodyConstructionInfo info(1,nullptr,&shape,btVector3(.4f,.4f,.4f));btRigidBody b(info);w.addRigidBody(&b);
    for(int i=0;i<3;++i)w.stepSimulation(1.f/60,1,1.f/60);
    require(vehicle.collisions==3&&vehicle.islands==3&&vehicle.setups>0&&vehicle.velocities>0&&vehicle.pushes>0&&vehicle.finishes>0,"Recovered GE6 vehicle solver hooks not connected");
    w.removeRigidBody(&b);w.remove_vehicle_solver(vehicle);
    // Exercise the recovered wheel callback against actual middleware ray
    // queries, including owner, trigger and non-rigid-object rejection.
    btSphereShape ground_shape(1),small(.25f);
    btRigidBody ground(0,nullptr,&ground_shape),owner(1,nullptr,&small),trigger(0,nullptr,&small);
    ground.setWorldTransform(btTransform(btQuaternion::getIdentity(),btVector3(0,-1,0)));
    owner.setWorldTransform(btTransform(btQuaternion::getIdentity(),btVector3(0,2,0)));
    trigger.setWorldTransform(btTransform(btQuaternion::getIdentity(),btVector3(0,1,0)));
    trigger.setCollisionFlags(trigger.getCollisionFlags()|btCollisionObject::CF_NO_CONTACT_RESPONSE);
    btCollisionObject plain;plain.setCollisionShape(&small);plain.setWorldTransform(btTransform(btQuaternion::getIdentity(),btVector3(0,1.5f,0)));
    w.addRigidBody(&ground);w.addRigidBody(&owner);w.addRigidBody(&trigger);w.addCollisionObject(&plain);
    fs15::ge6::WheelSuspension wheel;wheel.rest=1.75f;wheel.compression=.5f;wheel.droop=1;wheel.radius=.5f;
    auto hit=fs15::ge6::raycast_wheel(wheel,owner,w);
    require(hit.body==&ground&&std::abs(hit.point.y())<1e-5f&&hit.normal.y()>.999f&&std::abs(hit.length-1.5f)<1e-5f,"GE6 suspension ray did not reject owner/trigger/non-rigid objects");
    wheel.compression=.1f;hit=fs15::ge6::raycast_wheel(wheel,owner,w);
    require(std::abs(hit.length-1.65f)<1e-5f,"GE6 suspension compression limit lost");
    owner.setIgnoreCollisionCheck(&ground,true);hit=fs15::ge6::raycast_wheel(wheel,owner,w);
    require(!hit.body&&std::abs(hit.length-2.75f)<1e-5f&&std::abs(hit.point.y()+1.25f)<1e-5f&&hit.normal.y()==1,"GE6 ignored-pair/missing suspension ray result differs");
    owner.setIgnoreCollisionCheck(&ground,false);
    w.removeCollisionObject(&plain);w.removeRigidBody(&trigger);w.removeRigidBody(&owner);w.removeRigidBody(&ground);
    fs15::ge6::WheelFinish finish;finish.speed=1000;finish.normal_impulse=-3;finish.longitudinal_slip=.25f;finish.lateral_slip=-.5f;float motor=99;
    fs15::ge6::finish_wheels(&finish,1,ACTIVE_TAG,true,1.f/60,2,1000,motor);
    require(finish.angle>6.2831855f&&std::abs(finish.force-180)<1e-4f&&finish.slip[0]==.25f&&finish.slip[1]==-.5f,"GE6 single-wrap angle/solved wheel readback differs");
    float previous=finish.angle;fs15::ge6::finish_wheels(&finish,1,ISLAND_SLEEPING,true,1.f/60,2,1000,motor);require(finish.angle==previous,"Sleeping GE6 vehicle readback changed");
    std::cout<<"GE6 world/solver: accumulated gravity, generated contacts, resting body and native vehicle hook phases passed\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
