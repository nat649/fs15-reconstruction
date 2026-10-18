#include "physics_world.h"
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
    std::cout<<"GE6 world/solver: accumulated gravity, generated contacts, resting body and native vehicle hook phases passed\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
