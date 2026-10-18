#include "physics_world.h"
#include "physics_vehicle_sort.h"
#include <BulletCollision/CollisionDispatch/btCollisionObjectWrapper.h>
#include <BulletCollision/BroadphaseCollision/btCollisionAlgorithm.h>
#include <algorithm>
#include <stdexcept>
#include <vector>
namespace fs15::ge6 {
// 0x14027f620 / 0x14027f590: GE6 removes ordinary sleeping pairs. Retaining
// stock Bullet's callback leaves stale manifolds after bodies go to sleep.
void Dispatcher::dispatchAllCollisionPairs(btOverlappingPairCache* cache,const btDispatcherInfo& info,btDispatcher* dispatcher){
    struct Callback final:btOverlapCallback {
        Dispatcher& owner;const btDispatcherInfo& info;
        Callback(Dispatcher& d,const btDispatcherInfo& i):owner(d),info(i){}
        bool processOverlap(btBroadphasePair& pair) override{
            auto* a=static_cast<btCollisionObject*>(pair.m_pProxy0->m_clientObject);
            auto* b=static_cast<btCollisionObject*>(pair.m_pProxy1->m_clientObject);
            if(!a->isActive()&&!b->isActive())return a->hasContactResponse()&&b->hasContactResponse();
            near_callback(pair,owner,info);return false;
        }
    } callback(*this,info);
    cache->processAllOverlappingPairs(&callback,dispatcher);
}
void near_callback(btBroadphasePair& pair,btCollisionDispatcher& dispatcher,const btDispatcherInfo& info){
    auto* a=static_cast<btCollisionObject*>(pair.m_pProxy0->m_clientObject);
    auto* b=static_cast<btCollisionObject*>(pair.m_pProxy1->m_clientObject);
    if((!a->isActive()&&!b->isActive())||!a->checkCollideWith(b)||!b->checkCollideWith(a))return;
    btCollisionObjectWrapper wa(nullptr,a->getCollisionShape(),a,a->getWorldTransform(),-1,-1);
    btCollisionObjectWrapper wb(nullptr,b->getCollisionShape(),b,b->getWorldTransform(),-1,-1);
    if(!pair.m_algorithm)pair.m_algorithm=dispatcher.findAlgorithm(&wa,&wb);
    if(!pair.m_algorithm)return;
    btManifoldResult result(&wa,&wb);
    btDispatcherInfo current=info;
    if(!a->hasContactResponse()||!b->hasContactResponse())current.m_enableSatConvex=false;
    pair.m_algorithm->processCollision(&wa,&wb,current,&result);
}
// 0x14027e570: retain the inclusive interval selected by first/last body island.
btScalar Solver::solveGroupCacheFriendlySetup(btCollisionObject** bodies,int nb,btPersistentManifold** m,int nm,btTypedConstraint** c,int nc,const btContactSolverInfo& info,btIDebugDraw* debug){
    const btScalar result=btSequentialImpulseConstraintSolver::solveGroupCacheFriendlySetup(bodies,nb,m,nm,c,nc,info,debug);
    count_=0;
    const int total=vehicles_->size();if(nb<1||total<1)return result;
    const int first=bodies[0]->getIslandTag(),last=bodies[nb-1]->getIslandTag();start_=total;
    for(int i=0;i<total;++i){const int tag=(*vehicles_)[i]->island();if(first<=tag&&tag<=last){start_=i;break;}}
    for(int i=start_;i<total;++i){auto* v=(*vehicles_)[i];const int tag=v->island();if(tag<first||last<tag)break;++count_;v->setup(m_tmpSolverBodyPool,info);}
    return result;
}
// 0x14027e6e0 and 0x14027e7e0: vehicle constraints run before base solver work.
btScalar Solver::solveSingleIteration(int iteration,btCollisionObject** b,int nb,btPersistentManifold** m,int nm,btTypedConstraint** c,int nc,const btContactSolverInfo& info,btIDebugDraw* debug){
    for(int i=0;i<count_;++i)(*vehicles_)[start_+i]->velocity(m_tmpSolverBodyPool,info.m_timeStep);
    return btSequentialImpulseConstraintSolver::solveSingleIteration(iteration,b,nb,m,nm,c,nc,info,debug);
}
btScalar Solver::solveGroupCacheFriendlyFinish(btCollisionObject** b,int n,const btContactSolverInfo& info){
    for(int i=0;i<count_;++i)(*vehicles_)[start_+i]->finish(m_tmpSolverBodyPool,info.m_timeStep);
    return btSequentialImpulseConstraintSolver::solveGroupCacheFriendlyFinish(b,n,info);
}
// 0x14027e890. One base iteration per native outer iteration, with the vehicle
// penetration callbacks interleaved; stock Bullet's complete loop would reorder them.
void Solver::solveGroupCacheFriendlySplitImpulseIterations(btCollisionObject** b,int nb,btPersistentManifold** m,int nm,btTypedConstraint** c,int nc,const btContactSolverInfo& info,btIDebugDraw* debug){
    if(!info.m_splitImpulse)return;
    auto one=info;one.m_numIterations=1;
    for(int j=0;j<info.m_numIterations;++j){
        for(int i=0;i<count_;++i)(*vehicles_)[start_+i]->penetration(m_tmpSolverBodyPool,info.m_timeStep);
        btSequentialImpulseConstraintSolver::solveGroupCacheFriendlySplitImpulseIterations(b,nb,m,nm,c,nc,one,debug);
    }
}
World::World(btDispatcher* d,btBroadphaseInterface* b,Solver* s,btCollisionConfiguration* c):btDiscreteDynamicsWorld(d,b,s,c){}
// 0x14027ea00; profiling transport is intentionally excluded.
void World::performDiscreteCollisionDetection(){for(int i=0,n=vehicles_.size();i<n;++i)vehicles_[i]->collide(*this);btDiscreteDynamicsWorld::performDiscreteCollisionDetection();}
// 0x14027eb00. GE6 additionally requires an active endpoint for both joins.
void World::calculateSimulationIslands(){
    auto* islands=getSimulationIslandManager();islands->updateActivationState(this,getDispatcher());
    auto join=[&](const btCollisionObject* a,const btCollisionObject* b){
        if(a&&b&&!a->isStaticOrKinematicObject()&&!b->isStaticOrKinematicObject()&&(a->isActive()||b->isActive()))islands->getUnionFind().unite(a->getIslandTag(),b->getIslandTag());
    };
    for(int i=0;i<m_predictiveManifolds.size();++i){auto* m=m_predictiveManifolds[i];join(m->getBody0(),m->getBody1());}
    for(int i=0,n=m_constraints.size();i<n;++i){auto* c=m_constraints[i];if(c->isEnabled())join(&c->getRigidBodyA(),&c->getRigidBodyB());}
    for(int i=0,n=vehicles_.size();i<n;++i)vehicles_[i]->unite_islands(*islands);
    islands->storeIslandActivationState(this);
}
// 0x14027ed40, including native equal-key swap order.
void World::solveConstraints(btContactSolverInfo& info){
    if(vehicles_.size()>1){
        // Explicit token staging avoids aliasing a C++ array of derived pointers.
        std::vector<uintptr_t> tokens;tokens.reserve(static_cast<size_t>(vehicles_.size()));
        for(int i=0;i<vehicles_.size();++i)tokens.push_back(reinterpret_cast<uintptr_t>(vehicles_[i]));
        if(!fs15_physics_vehicle_sort(tokens.data(),tokens.size(),[](void*,uintptr_t token){return reinterpret_cast<VehicleSolver*>(token)->island();},nullptr))throw std::runtime_error("Invalid GE6 vehicle solver list");
        for(int i=0;i<vehicles_.size();++i)vehicles_[i]=reinterpret_cast<VehicleSolver*>(tokens[static_cast<size_t>(i)]);
    }
    btDiscreteDynamicsWorld::solveConstraints(info);
}
void World::add_vehicle_solver(VehicleSolver& v){if(vehicles_.findLinearSearch(&v)!=vehicles_.size())throw std::runtime_error("Duplicate GE6 vehicle solver");vehicles_.push_back(&v);}
void World::remove_vehicle_solver(VehicleSolver& v){vehicles_.remove(&v);}
PhysicsWorld::PhysicsWorld(const btVector3& gravity){
    btDefaultCollisionConstructionInfo ci;ci.m_customCollisionAlgorithmMaxElementSize=0x30;
    configuration_=std::make_unique<btDefaultCollisionConfiguration>(ci);
    dispatcher_=std::make_unique<Dispatcher>(configuration_.get());dispatcher_->setNearCallback(near_callback);
    broadphase_=std::make_unique<btDbvtBroadphase>();
    solver_=std::make_unique<Solver>();
    world_=std::make_unique<World>(dispatcher_.get(),broadphase_.get(),solver_.get(),configuration_.get());
    // Rebind without exposing a partially constructed world through callbacks.
    solver_->vehicles_=&world_->vehicle_solvers();
    world_->setGravity(gravity);
}
PhysicsWorld::~PhysicsWorld(){
    // Objects/constraints belong to the scene owner and must be detached first.
    // A nonempty world is a caller contract error, not ownership of its bodies.
}
}
