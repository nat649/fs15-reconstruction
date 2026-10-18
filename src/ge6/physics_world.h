#pragma once
/* Recovered GE6 physics hooks over explicitly pinned upstream Bullet.
 * No ReFarm dependencies. This does not yet implement native heightfield
 * collision algorithms or the Bt2RaycastVehicle callback implementation. */
#include <btBulletDynamicsCommon.h>
#include <BulletDynamics/ConstraintSolver/btSequentialImpulseConstraintSolver.h>
#include <BulletCollision/CollisionDispatch/btSimulationIslandManager.h>
#include <memory>
namespace fs15::ge6 {
class World;
class Dispatcher final:public btCollisionDispatcher {
public:
    explicit Dispatcher(btCollisionConfiguration* c):btCollisionDispatcher(c){}
    void dispatchAllCollisionPairs(btOverlappingPairCache*,const btDispatcherInfo&,btDispatcher*) override;
};
// Inspected Bt2VehicleSolverBase slots. Objects are borrowed; destruction is
// never dispatched through this interface. Not a reconstructed vehicle class.
class VehicleSolver {
public:
    virtual void unite_islands(btSimulationIslandManager&)=0;
    virtual int island() const=0;
    virtual void collide(World&)=0;
    virtual void setup(btAlignedObjectArray<btSolverBody>&,const btContactSolverInfo&)=0;
    virtual void velocity(btAlignedObjectArray<btSolverBody>&,btScalar)=0;
    virtual void penetration(btAlignedObjectArray<btSolverBody>&,btScalar)=0;
    virtual void finish(btAlignedObjectArray<btSolverBody>&,btScalar)=0;
protected:
    ~VehicleSolver()=default;
};
class Solver final:public btSequentialImpulseConstraintSolver {
    btAlignedObjectArray<VehicleSolver*>* vehicles_=nullptr;
    friend class PhysicsWorld;
    int start_=0,count_=0;
protected:
    btScalar solveGroupCacheFriendlySetup(btCollisionObject**,int,btPersistentManifold**,int,btTypedConstraint**,int,const btContactSolverInfo&,btIDebugDraw*) override;
    btScalar solveSingleIteration(int,btCollisionObject**,int,btPersistentManifold**,int,btTypedConstraint**,int,const btContactSolverInfo&,btIDebugDraw*) override;
    btScalar solveGroupCacheFriendlyFinish(btCollisionObject**,int,const btContactSolverInfo&) override;
    void solveGroupCacheFriendlySplitImpulseIterations(btCollisionObject**,int,btPersistentManifold**,int,btTypedConstraint**,int,const btContactSolverInfo&,btIDebugDraw*) override;
public:
    Solver()=default;
    explicit Solver(btAlignedObjectArray<VehicleSolver*>& v):vehicles_(&v){}
};
class World final:public btDiscreteDynamicsWorld {
    btAlignedObjectArray<VehicleSolver*> vehicles_;
    friend class Solver;
protected:
    void calculateSimulationIslands() override;
    void solveConstraints(btContactSolverInfo&) override;
public:
    World(btDispatcher*,btBroadphaseInterface*,Solver*,btCollisionConfiguration*);
    void performDiscreteCollisionDetection() override;
    // Host ownership adapter; preserve source order until native sorting runs.
    // Stop the physics worker before mutating this borrowed list.
    void add_vehicle_solver(VehicleSolver&);
    void remove_vehicle_solver(VehicleSolver&);
    btAlignedObjectArray<VehicleSolver*>& vehicle_solvers(){return vehicles_;}
};
class PhysicsWorld {
    // Authored RAII owner for recovered factory order. Dependencies outlive world.
    std::unique_ptr<btDefaultCollisionConfiguration> configuration_;
    std::unique_ptr<Dispatcher> dispatcher_;
    std::unique_ptr<btDbvtBroadphase> broadphase_;
    std::unique_ptr<Solver> solver_;
    std::unique_ptr<World> world_;
public:
    explicit PhysicsWorld(const btVector3& gravity);
    ~PhysicsWorld();
    PhysicsWorld(const PhysicsWorld&)=delete;
    PhysicsWorld& operator=(const PhysicsWorld&)=delete;
    World& world(){return *world_;}
    btCollisionDispatcher& dispatcher(){return *dispatcher_;}
};
// 0x14027f380 / 0x14027f3d0, separate from stock Bullet's continuous near callback.
void near_callback(btBroadphasePair&,btCollisionDispatcher&,const btDispatcherInfo&);
}
