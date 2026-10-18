/* Authored bounded scenario around recovered GE6 hooks and upstream middleware.
 * There is no authored force integrator. No game resources are required. */
#include "physics_world.h"
#include "physics_vehicle_sort.h"
#include "vehicle_wheel.h"
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
namespace {
btTransform probe_transform(const float* v){return btTransform(btMatrix3x3(v[0],v[1],v[2],v[4],v[5],v[6],v[8],v[9],v[10]),btVector3(v[12],v[13],v[14]));}
fs15::ge6::WheelSuspension probe_wheel(const float* v){
    fs15::ge6::WheelSuspension w;w.attachment=btVector3(v[16],v[17],v[18]);w.direction=btVector3(v[19],v[20],v[21]);
    w.rest=v[22];w.compression=v[23];w.droop=v[24];w.radius=v[25];return w;
}
}
extern "C" PROBE_API void fs15_ge6_wheel_frame(const float* v,float* out){
    btVector3 p,d;fs15::ge6::wheel_ray_frame(probe_wheel(v),probe_transform(v),p,d);
    for(int i=0;i<4;++i){out[i]=p[i];out[4+i]=d[i];}
}
extern "C" PROBE_API void fs15_ge6_wheel_ray(const float* v,int mode,float* out){
    struct RayWorld final:btCollisionWorld {
        const btCollisionObject& target;const float* v;int mode;
        RayWorld(const btCollisionObject& t,const float* values,int m):btCollisionWorld(nullptr,nullptr,nullptr),target(t),v(values),mode(m){}
        void rayTest(const btVector3&,const btVector3&,RayResultCallback& cb)const override{
            if(!mode)return;LocalRayResult r(&target,nullptr,btVector3(v[27],v[28],v[29]),v[26]);cb.addSingleResult(r,mode==1);
        }
    };
    btCollisionObject owner,target;owner.setWorldTransform(probe_transform(v));target.setWorldTransform(probe_transform(v+30));
    RayWorld world(target,v,mode);const auto r=fs15::ge6::raycast_wheel(probe_wheel(v),owner,world);
    out[0]=r.body?1.f:0.f;for(int i=0;i<3;++i){out[1+i]=r.point[i];out[4+i]=r.normal[i];}out[7]=r.plane;out[8]=r.length;
}
extern "C" PROBE_API int fs15_ge6_wheel_filter(int group,int mask,int proxy_group,int proxy_mask,int flags,int type,int own,int ignored){
    btRigidBody owner(1,nullptr,nullptr),other(1,nullptr,nullptr);btCollisionObject plain;
    btCollisionObject* object=own?static_cast<btCollisionObject*>(&owner):(type==2?static_cast<btCollisionObject*>(&other):&plain);
    object->setCollisionFlags(flags);if(ignored)owner.setIgnoreCollisionCheck(object,true);
    btBroadphaseProxy proxy;proxy.m_clientObject=object;proxy.m_collisionFilterGroup=short(proxy_group);proxy.m_collisionFilterMask=short(proxy_mask);
    fs15::ge6::WheelRayCallback ray(btVector3(0,0,0),btVector3(0,-1,0),owner,short(group),short(mask));return ray.needsCollision(&proxy)?1:0;
}
extern "C" PROBE_API void fs15_ge6_wheel_finish(const float* v,size_t count,int activation,int active,float dt,float impulse,float scale,float previous,float* out){
    std::vector<fs15::ge6::WheelFinish> wheels(count);
    for(size_t i=0;i<count;++i){auto& w=wheels[i];const float* p=v+i*8;w.speed=p[0];w.angle=p[1];w.normal_impulse=p[2];w.longitudinal_slip=p[3];w.lateral_slip=p[4];w.force=p[5];w.slip[0]=p[6];w.slip[1]=p[7];}
    fs15::ge6::finish_wheels(wheels.data(),count,activation,active!=0,dt,impulse,scale,previous);
    for(size_t i=0;i<count;++i){out[i*4]=wheels[i].angle;out[i*4+1]=wheels[i].force;out[i*4+2]=wheels[i].slip[0];out[i*4+3]=wheels[i].slip[1];}out[count*4]=previous;
}
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
