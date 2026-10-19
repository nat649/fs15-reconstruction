#include "vehicle_velocity.h"
#include <cmath>
namespace fs15::ge6 {
namespace {
float normal_relative(const WheelJacobian& j,const btSolverBody& a,const btSolverBody& b){
    float r=((j.angular_b.y()*b.m_deltaAngularVelocity.y()+b.m_deltaAngularVelocity.x()*j.angular_b.x())+j.angular_b.z()*b.m_deltaAngularVelocity.z())
        -((j.axis.x()*b.m_deltaLinearVelocity.x()+j.axis.y()*b.m_deltaLinearVelocity.y())+j.axis.z()*b.m_deltaLinearVelocity.z());
    r+=j.angular_a.y()*a.m_deltaAngularVelocity.y();r+=a.m_deltaAngularVelocity.x()*j.angular_a.x();r+=j.angular_a.z()*a.m_deltaAngularVelocity.z();
    r+=j.axis.x()*a.m_deltaLinearVelocity.x();r+=j.axis.y()*a.m_deltaLinearVelocity.y();r+=j.axis.z()*a.m_deltaLinearVelocity.z();return r;
}
void apply(btSolverBody& a,btSolverBody& b,const btVector3& axis,const WheelJacobian& j,float impulse){
    if(a.m_originalBody){
        for(int k=0;k<3;++k)a.m_deltaLinearVelocity[k]=((axis[k]*a.m_linearFactor[k])*impulse)*a.m_invMass[k]+a.m_deltaLinearVelocity[k];
        for(int k=0;k<3;++k)a.m_deltaAngularVelocity[k]=(impulse*a.m_angularFactor[k])*j.component_a[k]+a.m_deltaAngularVelocity[k];
    }
    impulse=-impulse;
    if(b.m_originalBody){
        for(int k=0;k<3;++k)b.m_deltaLinearVelocity[k]=((axis[k]*b.m_linearFactor[k])*impulse)*b.m_invMass[k]+b.m_deltaLinearVelocity[k];
        b.m_deltaAngularVelocity[2]=(impulse*b.m_angularFactor.z())*j.component_b.z()+b.m_deltaAngularVelocity.z();
        b.m_deltaAngularVelocity[0]=(impulse*b.m_angularFactor.x())*j.component_b.x()+b.m_deltaAngularVelocity.x();
        b.m_deltaAngularVelocity[1]=(impulse*b.m_angularFactor.y())*j.component_b.y()+b.m_deltaAngularVelocity.y();
    }
}
float longitudinal_relative(const WheelJacobian& j,const btSolverBody& a,const btSolverBody& b){
    float r=j.axis.y()*a.m_deltaLinearVelocity.y()+j.axis.x()*a.m_deltaLinearVelocity.x();r+=j.axis.z()*a.m_deltaLinearVelocity.z();
    r+=j.angular_a.x()*a.m_deltaAngularVelocity.x();r+=j.angular_a.y()*a.m_deltaAngularVelocity.y();r+=a.m_deltaAngularVelocity.z()*j.angular_a.z();
    r-=((j.axis.x()*b.m_deltaLinearVelocity.x()+j.axis.y()*b.m_deltaLinearVelocity.y())+j.axis.z()*b.m_deltaLinearVelocity.z());
    r+=j.angular_b.x()*b.m_deltaAngularVelocity.x();r+=j.angular_b.y()*b.m_deltaAngularVelocity.y();r+=j.angular_b.z()*b.m_deltaAngularVelocity.z();return r;
}
float lateral_relative(const WheelJacobian& j,const btSolverBody& a,const btSolverBody& b){
    float r=j.angular_a.y()*a.m_deltaAngularVelocity.y()+j.angular_a.x()*a.m_deltaAngularVelocity.x();r+=j.angular_a.z()*a.m_deltaAngularVelocity.z();
    r+=j.axis.x()*a.m_deltaLinearVelocity.x();r+=j.axis.y()*a.m_deltaLinearVelocity.y();r+=j.axis.z()*a.m_deltaLinearVelocity.z();
    r-=((j.axis.x()*b.m_deltaLinearVelocity.x()+j.axis.y()*b.m_deltaLinearVelocity.y())+j.axis.z()*b.m_deltaLinearVelocity.z());
    r+=j.angular_b.y()*b.m_deltaAngularVelocity.y();r+=b.m_deltaAngularVelocity.x()*j.angular_b.x();r+=j.angular_b.z()*b.m_deltaAngularVelocity.z();return r;
}
float clamp_impulse(float delta,float& accumulated,float lower,float upper){
    const float next=accumulated+delta;
    if(next<=upper){if(lower<=next)accumulated=next;else{delta=lower-accumulated;accumulated=lower;}}
    else{delta=upper-accumulated;accumulated=upper;}return delta;
}
float gravity_dot(const btVector3& g,const btVector3& n){return (g.y()*n.y()+g.x()*n.x())+g.z()*n.z();}
}
void solve_wheel_velocity(WheelVelocity* wheels,std::size_t count,int activation,bool active,float dt,const WheelVelocityServices& services){
    if(activation==ISLAND_SLEEPING||!active)return;
    if(services.drive)services.drive(services.context,dt);
    for(std::size_t i=0;i<count;++i){auto& w=wheels[i];if(!w.contact)continue;auto& c=w.suspension;auto& a=*w.a;auto& b=*w.b;
        const float relative=normal_relative(c.jacobian,a,b);
        float delta=clamp_impulse((c.rhs-c.impulse*c.cfm)-relative*c.diagonal,c.impulse,c.lower_limit,0);
        if(c.push_diagonal!=0){
            const float correction=c.correction_rhs-c.push_diagonal*relative;const float next=w.correction_impulse+correction;
            if(next<=0){w.correction_impulse=next;delta+=correction;}
            else{delta+=0.f-w.correction_impulse;w.correction_impulse=0;}
        }
        apply(a,b,w.normal,c.jacobian,delta*c.jacobian.projection);
        float gravity=0;
        if(!(0<gravity_dot(services.gravity(services.context),w.normal)))gravity=gravity_dot(services.gravity(services.context),w.normal);
        const float load=(c.impulse*c.jacobian.projection)/dt-gravity*w.mass_load;
        if(!(0<load))continue;
        fs15::TireForces forces;fs15::tire_forces(w.tire,w.slip_longitudinal,w.slip_lateral,load,forces);
        const float floor=(load*w.tire.friction)*w.friction_floor;
        float limit=std::abs(forces.longitudinal);if(limit<floor)limit=floor;limit*=dt;
        unsigned iteration=0;bool done;
        do{
            done=true;
            if(w.brake!=0){
                const float bound=(dt*w.inverse_inertia)*w.brake;
                float brake_delta=-w.speed;const float next=brake_delta+w.brake_impulse;
                if(next<=bound){if(-bound<=next){w.brake_impulse=next;done=false;}else{brake_delta=-bound-w.brake_impulse;w.brake_impulse=-bound;}}
                else{brake_delta=bound-w.brake_impulse;w.brake_impulse=bound;}
                w.speed=brake_delta+w.speed;
            }
            auto& longitudinal=w.longitudinal;
            const float d=clamp_impulse((w.tire.radius*w.speed-longitudinal_relative(longitudinal.jacobian,a,b))*longitudinal.diagonal+longitudinal.rhs,longitudinal.impulse,-limit,limit);
            w.speed=w.speed-(d*w.tire.radius)*w.inverse_inertia;
            apply(a,b,longitudinal.jacobian.axis,longitudinal.jacobian,d);
        }while(!done&&++iteration<10);
        auto& lateral=w.lateral;float lateral_limit=std::abs(forces.lateral);if(lateral_limit<floor)lateral_limit=floor;lateral_limit*=dt;
        const float d=clamp_impulse(lateral.rhs-lateral_relative(lateral.jacobian,a,b)*lateral.diagonal,lateral.impulse,-lateral_limit,lateral_limit);
        apply(a,b,lateral.jacobian.axis,lateral.jacobian,d);
    }
}
}
