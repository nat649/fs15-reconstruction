/* Authored bounded scenario around recovered GE6 hooks and upstream middleware.
 * There is no authored force integrator. No game resources are required. */
#include "physics_world.h"
#include "physics_vehicle_sort.h"
#include "vehicle_wheel.h"
#include "vehicle_constraint.h"
#include "vehicle_velocity.h"
#include "vehicle_drive.h"
#include "raycast_vehicle.h"
#include "gl_draw.h"
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
extern "C" PROBE_API int fs15_ge6_gl_draw(FS15GLDrawState* state,int32_t count,int32_t first,uint32_t primitive,uint32_t minimum,uint32_t range_count,FS15GLDrawServices* s){return fs15_gl_draw(state,count,first,primitive,minimum,range_count,s);}
namespace {
float probe_float(const void* p,size_t offset){float f;std::memcpy(&f,static_cast<const char*>(p)+offset,4);return f;}
void probe_store(void* p,size_t offset,float f){std::memcpy(static_cast<char*>(p)+offset,&f,4);}
btVector3 probe_vector(const void* p,size_t offset){return btVector3(probe_float(p,offset),probe_float(p,offset+4),probe_float(p,offset+8));}
void probe_vector_store(void* p,size_t offset,const btVector3& v){for(int i=0;i<4;++i)probe_store(p,offset+i*4,v[i]);}
btTransform probe_transform(const float* v){return btTransform(btMatrix3x3(v[0],v[1],v[2],v[4],v[5],v[6],v[8],v[9],v[10]),btVector3(v[12],v[13],v[14]));}
fs15::ge6::WheelSuspension probe_wheel(const float* v){
    fs15::ge6::WheelSuspension w;w.attachment=btVector3(v[16],v[17],v[18]);w.direction=btVector3(v[19],v[20],v[21]);
    w.rest=v[22];w.compression=v[23];w.droop=v[24];w.radius=v[25];return w;
}
void probe_body(const void* raw,btRigidBody& body){
    const float inverse=probe_float(raw,0x1a0);body.setMassProps(inverse==0?0:1.f/inverse,btVector3(0,0,0));body.setAngularFactor(probe_vector(raw,0x290));
    // Authored test injection of arbitrary inspected inertia rows. Production
    // GE6 uses the middleware's ordinary body/inertia APIs instead.
    auto& m=const_cast<btMatrix3x3&>(body.getInvInertiaTensorWorld());for(int i=0;i<3;++i)m[i]=probe_vector(raw,0x150+i*16);
}
btSolverBody probe_solver(const void* raw,btRigidBody& body){
    btSolverBody s{};float transform[16];std::memcpy(transform,raw,64);s.m_worldTransform=probe_transform(transform);
    s.m_deltaLinearVelocity=probe_vector(raw,0x40);s.m_deltaAngularVelocity=probe_vector(raw,0x50);s.m_angularFactor=probe_vector(raw,0x60);
    s.m_invMass=probe_vector(raw,0x70);s.m_linearFactor=probe_vector(raw,0x80);s.m_pushVelocity=probe_vector(raw,0x90);s.m_turnVelocity=probe_vector(raw,0xa0);
    s.m_linearVelocity=probe_vector(raw,0xb0);s.m_angularVelocity=probe_vector(raw,0xc0);s.m_externalForceImpulse=probe_vector(raw,0xd0);s.m_externalTorqueImpulse=probe_vector(raw,0xe0);
    uintptr_t original;std::memcpy(&original,static_cast<const char*>(raw)+0xf0,8);s.m_originalBody=original?&body:nullptr;return s;
}
fs15::ge6::WheelJacobian probe_jacobian(const void* p){fs15::ge6::WheelJacobian j;j.angular_a=probe_vector(p,0);j.angular_b=probe_vector(p,0x10);j.axis=probe_vector(p,0x20);j.component_a=probe_vector(p,0x30);j.component_b=probe_vector(p,0x40);j.projection=probe_float(p,0x60);return j;}
void probe_jacobian_store(void* p,const fs15::ge6::WheelJacobian& j){probe_vector_store(p,0,j.angular_a);probe_vector_store(p,0x10,j.angular_b);probe_vector_store(p,0x20,j.axis);probe_vector_store(p,0x30,j.component_a);probe_vector_store(p,0x40,j.component_b);probe_store(p,0x60,j.projection);}
}
// These bridges translate explicit inspected Windows fixture bytes through
// memcpy. They never reinterpret foreign solver records as Linux Bullet types.
extern "C" PROBE_API void fs15_ge6_constraint_setup(int method,const void* ar,const void* br,const void* sar,const void* sbr,const float* q,void* result,float* stats){
    btRigidBody a(0,nullptr,nullptr),b(0,nullptr,nullptr);probe_body(ar,a);probe_body(br,b);const auto sa=probe_solver(sar,a),sb=probe_solver(sbr,b);
    const auto n=probe_vector(q,0),p=probe_vector(q,16),application=probe_vector(q,32),lever=probe_vector(q,48);
    if(!method){fs15::ge6::WheelJacobian j;const auto s=fs15::ge6::setup_wheel_jacobian(j,n,p,application,lever,a,b,sa,sb);probe_jacobian_store(result,j);stats[0]=s.denominator_a;stats[1]=s.denominator_b;stats[2]=s.velocity_a;stats[3]=s.velocity_b;}
    else{
        fs15::ge6::SuspensionSpring spring;spring.rest=q[17];spring.compression=q[18];spring.stiffness=q[19];spring.positive_velocity_damping=q[20];spring.nonpositive_velocity_damping=q[21];
        btContactSolverInfo info;info.m_timeStep=q[22];info.m_erp=q[23];info.m_erp2=q[24];info.m_splitImpulse=int(q[25]);info.m_splitImpulsePenetrationThreshold=q[26];
        fs15::ge6::SuspensionConstraint c;fs15::ge6::setup_suspension_constraint(c,n,p,application,lever,q[16],spring,a,b,sa,sb,info);probe_jacobian_store(result,c.jacobian);
        probe_store(result,0x50,c.diagonal);probe_store(result,0x54,c.rhs);probe_store(result,0x58,c.cfm);probe_store(result,0x70,c.lower_limit);probe_store(result,0x78,c.push_diagonal);probe_store(result,0x7c,c.correction_rhs);probe_store(result,0x80,c.push_rhs);
    }
}
extern "C" PROBE_API void fs15_ge6_constraint_push(void* sar,void* sbr,void* records,const void* wheels,size_t count,int activation,int active,int alias){
    btRigidBody a(0,nullptr,nullptr),b(0,nullptr,nullptr);auto sa=probe_solver(sar,a),sb=probe_solver(sbr,b);
    std::vector<fs15::ge6::SuspensionConstraint> constraints(count);std::vector<fs15::ge6::PenetrationWheel> states(count);
    for(size_t i=0;i<count;++i){const char* r=static_cast<const char*>(records)+i*0x1a0+0x20;const char* w=static_cast<const char*>(wheels)+i*0x160;
        auto& c=constraints[i];c.jacobian=probe_jacobian(r);c.push_diagonal=probe_float(r,0x78);c.push_rhs=probe_float(r,0x80);c.push_impulse=probe_float(r,0x84);
        auto& s=states[i];s.constraint=&c;s.a=&sa;s.b=alias?&sa:&sb;s.normal=probe_vector(w,0);uintptr_t contact;std::memcpy(&contact,w+0x28,8);s.contact=contact!=0;
    }
    fs15::ge6::solve_wheel_penetration(states.data(),count,activation,active!=0);
    for(size_t i=0;i<count;++i)probe_store(records,i*0x1a0+0xa4,constraints[i].push_impulse);
    // Preserve untouched fourth vector lanes in the serialized native record.
    for(int k=0;k<3;++k){probe_store(sar,0x90+k*4,sa.m_pushVelocity[k]);probe_store(sar,0xa0+k*4,sa.m_turnVelocity[k]);if(!alias){probe_store(sbr,0x90+k*4,sb.m_pushVelocity[k]);probe_store(sbr,0xa0+k*4,sb.m_turnVelocity[k]);}}
}
extern "C" PROBE_API void fs15_ge6_constraint_velocity(void* sar,void* sbr,void* records,void* wheels,size_t count,int activation,int active,int alias,float dt,const float* gravity,int* counters){
    btRigidBody a(0,nullptr,nullptr),b(0,nullptr,nullptr);auto sa=probe_solver(sar,a),sb=probe_solver(sbr,b);
    std::vector<fs15::ge6::WheelVelocity> states(count);
    for(size_t i=0;i<count;++i){const char* r=static_cast<const char*>(records)+i*0x1a0;const char* p=static_cast<const char*>(wheels)+i*0x160;auto& w=states[i];
        w.a=&sa;w.b=alias?&sa:&sb;w.normal=probe_vector(p,0);uintptr_t contact;std::memcpy(&contact,p+0x28,8);w.contact=contact!=0;
        w.suspension.jacobian=probe_jacobian(r+0x20);w.suspension.diagonal=probe_float(r,0x70);w.suspension.rhs=probe_float(r,0x74);w.suspension.cfm=probe_float(r,0x78);w.suspension.impulse=probe_float(r,0x7c);w.suspension.lower_limit=probe_float(r,0x90);
        w.suspension.push_diagonal=probe_float(r,0x98);w.suspension.correction_rhs=probe_float(r,0x9c);w.correction_impulse=probe_float(r,0x94);
        w.lateral.jacobian=probe_jacobian(r+0xb0);w.lateral.diagonal=probe_float(r,0x100);w.lateral.rhs=probe_float(r,0x104);w.lateral.impulse=probe_float(r,0x10c);
        w.longitudinal.jacobian=probe_jacobian(r+0x120);w.longitudinal.diagonal=probe_float(r,0x170);w.longitudinal.rhs=probe_float(r,0x174);w.longitudinal.impulse=probe_float(r,0x17c);
        w.tire={probe_float(p,0xdc),probe_float(p,0x110),probe_float(p,0x114),probe_float(p,0x118),probe_float(p,0x11c)};
        w.mass_load=probe_float(p,0x120);w.inverse_inertia=probe_float(p,0x124);w.speed=probe_float(p,0x108);w.brake=probe_float(p,0x134);w.brake_impulse=probe_float(p,0x158);
        w.friction_floor=probe_float(r,0x18);w.slip_longitudinal=probe_float(r,0x14);w.slip_lateral=probe_float(r,0x10);
    }
    struct Services {btVector3 gravity;int drive_calls=0,gravity_calls=0;};Services context{btVector3(gravity[0],gravity[1],gravity[2])};
    fs15::ge6::WheelVelocityServices services;services.context=&context;services.drive=[](void* p,float){++static_cast<Services*>(p)->drive_calls;};services.gravity=[](void* p){auto& c=*static_cast<Services*>(p);++c.gravity_calls;return c.gravity;};
    fs15::ge6::solve_wheel_velocity(states.data(),count,activation,active!=0,dt,services);
    for(size_t i=0;i<count;++i){const auto& w=states[i];probe_store(records,i*0x1a0+0x7c,w.suspension.impulse);probe_store(records,i*0x1a0+0x94,w.correction_impulse);probe_store(records,i*0x1a0+0x10c,w.lateral.impulse);probe_store(records,i*0x1a0+0x17c,w.longitudinal.impulse);probe_store(wheels,i*0x160+0x108,w.speed);probe_store(wheels,i*0x160+0x158,w.brake_impulse);}
    for(int k=0;k<3;++k){probe_store(sar,0x40+k*4,sa.m_deltaLinearVelocity[k]);probe_store(sar,0x50+k*4,sa.m_deltaAngularVelocity[k]);if(!alias){probe_store(sbr,0x40+k*4,sb.m_deltaLinearVelocity[k]);probe_store(sbr,0x50+k*4,sb.m_deltaAngularVelocity[k]);}}
    counters[0]=context.drive_calls;counters[1]=context.gravity_calls;
}
extern "C" PROBE_API void fs15_ge6_drivetrain(void* actor,void* raw_wheels,const void* raw_nodes,size_t count,float dt,int prepare){
    fs15::ge6::DriveWheel wheels[20];std::vector<fs15::ge6::Differential> nodes(count);
    for(size_t i=0;i<20;++i){auto& w=wheels[i];w.radius=probe_float(raw_wheels,i*0x160+0xdc);w.speed=probe_float(raw_wheels,i*0x160+0x108);w.inverse_inertia=probe_float(raw_wheels,i*0x160+0x124);w.weight=probe_float(raw_wheels,i*0x160+0x144);}
    for(size_t i=0;i<count;++i){auto& n=nodes[i];n.ratio=probe_float(raw_nodes,i*12);n.bias=probe_float(raw_nodes,i*12+4);std::memcpy(&n.a,static_cast<const char*>(raw_nodes)+i*12+8,2);std::memcpy(&n.b,static_cast<const char*>(raw_nodes)+i*12+10,2);}
    fs15::ge6::VehicleMotor m;m.inverse_inertia=probe_float(actor,0x98);m.speed=probe_float(actor,0x9c);m.target_speed=probe_float(actor,0xa0);m.damping=probe_float(actor,0xa4);m.torque=probe_float(actor,0xa8);m.ratio=probe_float(actor,0xac);m.clutch_torque=probe_float(actor,0xb0);m.driven_speed=probe_float(actor,0xb4);m.torque_impulse=probe_float(actor,0xb8);m.clutch_impulse=probe_float(actor,0xbc);
    if(prepare)fs15::ge6::prepare_differentials(wheels,nodes.data(),count,m);else fs15::ge6::solve_drivetrain(wheels,nodes.data(),count,m,dt);
    for(size_t i=0;i<20;++i)probe_store(raw_wheels,i*0x160+(prepare?0x144:0x108),prepare?wheels[i].weight:wheels[i].speed);
    if(!prepare){probe_store(actor,0x9c,m.speed);probe_store(actor,0xb4,m.driven_speed);probe_store(actor,0xb8,m.torque_impulse);probe_store(actor,0xbc,m.clutch_impulse);}
}
extern "C" PROBE_API int fs15_ge6_vehicle_course(int steps,const float* commands,float* output,size_t capacity){
    if(steps<1||steps>1200||!commands||!output||capacity<size_t(steps)*40)return 0;
    try{
        fs15::ge6::PhysicsWorld physics(btVector3(0,-9.81f,0));auto& world=physics.world();world.getSolverInfo().m_numIterations=20;world.getSolverInfo().m_solverMode|=0x70;
        btSphereShape ground_shape(1000),body_shape(.5f);btRigidBody ground(0,nullptr,&ground_shape),body(1000,nullptr,&body_shape,btVector3(100,100,100));
        ground.setWorldTransform(btTransform(btQuaternion::getIdentity(),btVector3(0,-1000,0)));body.setWorldTransform(btTransform(btQuaternion::getIdentity(),btVector3(0,1.8f,0)));body.setActivationState(DISABLE_DEACTIVATION);
        world.addRigidBody(&ground);world.addRigidBody(&body);fs15::ge6::RaycastVehicle vehicle(world,{&body});
        for(int i=0;i<4;++i){fs15::ge6::RaycastWheelConfig c;c.suspension.attachment=btVector3(i%2?1.f:-1.f,0,i<2?1.5f:-1.5f);c.suspension.rest=1;c.suspension.compression=.2f;c.suspension.droop=.5f;c.suspension.radius=.5f;c.spring.rest=1;c.spring.compression=.2f;c.spring.stiffness=30000;c.spring.positive_velocity_damping=5000;c.spring.nonpositive_velocity_damping=5000;c.tire={.5f,5,1,5,1};c.force_lever=btVector3(c.suspension.attachment.x(),-1,c.suspension.attachment.z());c.damping=.1f;vehicle.add_wheel(c);}
        vehicle.set_differentials({{.5f,2,0,1},{.5f,2,2,3},{.5f,2,20,21}});vehicle.motor().inverse_inertia=1;vehicle.motor().clutch_torque=2000;world.add_vehicle_solver(vehicle);
        for(int step=0;step<steps;++step){const float* command=commands+step*5;vehicle.motor().torque=command[0];vehicle.motor().target_speed=command[1];vehicle.motor().ratio=command[2];for(unsigned i=0;i<4;++i){vehicle.wheel_config(i).steering=i<2?command[3]:0;vehicle.wheel_config(i).brake=command[4];}
            world.stepSimulation(1.f/60,20,1.f/60);float* out=output+step*40;const auto& p=body.getWorldTransform().getOrigin();const auto& v=body.getLinearVelocity();const auto& a=body.getAngularVelocity();for(int i=0;i<3;++i){out[i]=p[i];out[3+i]=v[i];out[6+i]=a[i];}out[9]=vehicle.motor().speed;out[10]=vehicle.motor().driven_speed;out[11]=vehicle.motor_output();
            for(unsigned i=0;i<4;++i){const auto& w=vehicle.wheel_state(i);const auto& f=vehicle.readback(i);const auto& c=vehicle.contact(i);float* r=out+12+i*7;r[0]=w.speed;r[1]=f.angle;r[2]=f.force;r[3]=f.slip[0];r[4]=f.slip[1];r[5]=c.length;r[6]=c.body?1.f:0.f;}
        }
        world.remove_vehicle_solver(vehicle);world.removeRigidBody(&body);world.removeRigidBody(&ground);return 1;
    }catch(...){return 0;}
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
