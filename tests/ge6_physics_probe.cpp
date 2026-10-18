/* Authored bounded scenario around recovered GE6 hooks and upstream middleware.
 * There is no authored force integrator. No game resources are required. */
#include "physics_world.h"
#include "physics_vehicle_sort.h"
#include <vector>
#include <memory>
#include <cstring>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#ifdef _WIN32
#define PROBE_API __declspec(dllexport)
#else
#define PROBE_API __attribute__((visibility("default")))
#endif
extern "C" PROBE_API int fs15_ge6_sphere_drop(float gravity,float height,float radius,float mass,int steps,float dt,float* output,size_t count){
    if(!output||steps<1||steps>3600||count<size_t(steps)*7||radius<=0||mass<=0||!std::isfinite(gravity)||!std::isfinite(height)||!std::isfinite(radius)||!std::isfinite(mass)||dt<=0||!std::isfinite(dt))return 0;
    try{
        fs15::ge6::PhysicsWorld physics(btVector3(0,gravity,0));auto& world=physics.world();
        // This matches the native manager's post-factory configuration.
        world.getSolverInfo().m_numIterations=20;world.getSolverInfo().m_solverMode|=0x70;
        btSphereShape shape(radius);btVector3 inertia;shape.calculateLocalInertia(mass,inertia);
        btRigidBody::btRigidBodyConstructionInfo ground_info(0,nullptr,&shape);
        ground_info.m_startWorldTransform.setOrigin(btVector3(0,-radius,0));btRigidBody ground(ground_info);
        btRigidBody::btRigidBodyConstructionInfo body_info(mass,nullptr,&shape,inertia);
        body_info.m_startWorldTransform.setOrigin(btVector3(0,height,0));btRigidBody body(body_info);
        world.addRigidBody(&ground);world.addRigidBody(&body);
        for(int i=0;i<steps;++i){
            world.stepSimulation(dt,20,dt);const auto& p=body.getWorldTransform().getOrigin();const auto& v=body.getLinearVelocity();
            for(int j=0;j<3;++j){output[i*7+j]=p[j];output[i*7+3+j]=v[j];}
            int contacts=0;auto& dispatch=physics.dispatcher();for(int j=0;j<dispatch.getNumManifolds();++j)contacts+=dispatch.getManifoldByIndexInternal(j)->getNumContacts();
            output[i*7+6]=static_cast<float>(contacts);
        }
        world.removeRigidBody(&body);world.removeRigidBody(&ground);return 1;
    }catch(...){return 0;}
}
extern "C" PROBE_API size_t fs15_ge6_middleware_size(unsigned type){
    switch(type){case 0:return sizeof(btDiscreteDynamicsWorld);case 1:return sizeof(btSequentialImpulseConstraintSolver);
    case 2:return sizeof(btDbvtBroadphase);case 3:return sizeof(btCollisionDispatcher);case 4:return sizeof(btDefaultCollisionConfiguration);
    case 5:return sizeof(btRigidBody);case 6:return sizeof(btPersistentManifold);case 7:return sizeof(btManifoldPoint);case 8:return sizeof(btBroadphaseProxy);default:return 0;}
}
extern "C" PROBE_API int fs15_ge6_sort(uintptr_t* tokens,size_t count,int (*key)(void*,uintptr_t),void* user){return fs15_physics_vehicle_sort(tokens,count,key,user);}
extern "C" PROBE_API int fs15_ge6_hook_trace(const int* keys,size_t actors,int steps,uint32_t* output,size_t count){
    if(!keys||actors>32||steps<1||steps>10||!output)return -1;
    struct Actor final:fs15::ge6::VehicleSolver {
        int key=0,id=0;std::vector<uint32_t>* trace=nullptr;
        void emit(unsigned kind,unsigned extra=0){trace->insert(trace->end(),{kind,static_cast<unsigned>(id),extra});}
        void emit_time(unsigned kind,float dt){uint32_t bits;std::memcpy(&bits,&dt,4);emit(kind,bits);}
        int island()const override{return key;}
        void collide(fs15::ge6::World&)override{emit(1);}
        void unite_islands(btSimulationIslandManager&)override{emit(2);}
        void setup(btAlignedObjectArray<btSolverBody>& b,const btContactSolverInfo&)override{emit(3,static_cast<unsigned>(b.size()));}
        void velocity(btAlignedObjectArray<btSolverBody>&,btScalar dt)override{emit_time(4,dt);}
        void penetration(btAlignedObjectArray<btSolverBody>&,btScalar dt)override{emit_time(5,dt);}
        void finish(btAlignedObjectArray<btSolverBody>&,btScalar dt)override{emit_time(6,dt);}
    };
    try{
        fs15::ge6::PhysicsWorld physics(btVector3(0,-9.81f,0));auto& world=physics.world();world.getSolverInfo().m_numIterations=20;world.getSolverInfo().m_solverMode|=0x70;
        std::vector<uint32_t> trace;std::vector<std::unique_ptr<Actor>> vehicles;
        for(size_t i=0;i<actors;++i){auto v=std::make_unique<Actor>();v->key=keys[i];v->id=static_cast<int>(i);v->trace=&trace;world.add_vehicle_solver(*v);vehicles.push_back(std::move(v));}
        btSphereShape shape(1);btRigidBody::btRigidBodyConstructionInfo info(1,nullptr,&shape,btVector3(.4f,.4f,.4f));info.m_startWorldTransform.setOrigin(btVector3(0,8,0));btRigidBody body(info);world.addRigidBody(&body);
        for(int i=0;i<steps;++i)world.stepSimulation(1.f/60,20,1.f/60);
        world.removeRigidBody(&body);for(auto& v:vehicles)world.remove_vehicle_solver(*v);
        if(trace.size()>count)return -1;std::copy(trace.begin(),trace.end(),output);return static_cast<int>(trace.size()/3);
    }catch(...){return -1;}
}
