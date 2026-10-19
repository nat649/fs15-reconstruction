#include "vehicle_constraint.h"

namespace fs15::ge6 {
namespace {
btVector3 inertia_component(const btRigidBody& b,const btVector3& v,bool first_row_xy){
    const auto& m=b.getInvInertiaTensorWorld();const auto& f=b.getAngularFactor();
    return btVector3(f.x()*(first_row_xy?((m[0][0]*v.x()+m[0][1]*v.y())+m[0][2]*v.z()):((v.y()*m[0][1]+v.x()*m[0][0])+v.z()*m[0][2])),
                    f.y()*((v.y()*m[1][1]+v.x()*m[1][0])+v.z()*m[1][2]),
                    f.z()*((v.y()*m[2][1]+v.x()*m[2][0])+v.z()*m[2][2]));
}
}
WheelJacobianStats setup_wheel_jacobian(WheelJacobian& j,const btVector3& n,const btVector3& p,const btVector3& application,const btVector3& lever,
    const btRigidBody& a,const btRigidBody& b,const btSolverBody& sa,const btSolverBody& sb){
    const auto ra=p-sa.m_worldTransform.getOrigin(),rb=p-sb.m_worldTransform.getOrigin();
    const btVector3 aa(n.z()*ra.y()-n.y()*ra.z(),n.x()*ra.z()-n.z()*ra.x(),n.y()*ra.x()-n.x()*ra.y());
    const btVector3 ab(n.y()*rb.z()-n.z()*rb.y(),n.z()*rb.x()-n.x()*rb.z(),n.x()*rb.y()-n.y()*rb.x());
    const auto ia=inertia_component(a,aa,true),ib=inertia_component(b,ab,false);
    WheelJacobianStats s;
    s.denominator_a=((n.y()*(ia.z()*ra.x()-ra.z()*ia.x())+n.x()*(ia.y()*ra.z()-ia.z()*ra.y()))+n.z()*(ra.y()*ia.x()-ia.y()*ra.x()))+a.getInvMass();
    s.denominator_b=b.getInvMass()-((n.y()*(ib.z()*rb.x()-rb.z()*ib.x())+n.x()*(ib.y()*rb.z()-ib.z()*rb.y()))+n.z()*(rb.y()*ib.x()-ib.y()*rb.x()));
    s.velocity_a=((((aa.y()*sa.m_angularVelocity.y()+aa.x()*sa.m_angularVelocity.x())+aa.z()*sa.m_angularVelocity.z())
        +n.y()*(sa.m_externalForceImpulse.y()+sa.m_linearVelocity.y()))+n.x()*(sa.m_externalForceImpulse.x()+sa.m_linearVelocity.x()))
        +(sa.m_externalForceImpulse.z()+sa.m_linearVelocity.z())*n.z();
    s.velocity_b=((n.y()*(sb.m_linearVelocity.y()+sb.m_externalForceImpulse.y())+n.x()*(sb.m_linearVelocity.x()+sb.m_externalForceImpulse.x()))
        +(sb.m_linearVelocity.z()+sb.m_externalForceImpulse.z())*n.z())-((ab.y()*sb.m_angularVelocity.y()+ab.x()*sb.m_angularVelocity.x())+ab.z()*sb.m_angularVelocity.z());
    j.component_a=inertia_component(a,lever.cross(application),true);
    j.component_b=inertia_component(b,rb.cross(application),false);
    j.angular_a=aa;j.angular_b=ab;j.axis=n;
    j.projection=(n.x()*application.x()+application.y()*n.y())+application.z()*n.z();
    return s;
}
void setup_suspension_constraint(SuspensionConstraint& c,const btVector3& axis,const btVector3& point,const btVector3& contact_normal,const btVector3& lever,float length,const SuspensionSpring& spring,
    const btRigidBody& a,const btRigidBody& b,const btSolverBody& sa,const btSolverBody& sb,const btContactSolverInfo& info){
    const auto s=setup_wheel_jacobian(c.jacobian,axis,point,contact_normal,lever,a,b,sa,sb);
    float relative=s.velocity_a-s.velocity_b;
    const float damping=relative<=0?spring.nonpositive_velocity_damping:spring.positive_velocity_damping;
    const float spring_dt=info.m_timeStep*spring.stiffness;
    float damping_force=damping*relative;
    const float softness=1.f/((spring_dt+damping)*info.m_timeStep);
    // The instruction stream sums the two body terms before adding softness;
    // Ghidra's expression loses that association near cancelling terms.
    const float body_sum=s.denominator_a+s.denominator_b;
    const float diagonal=1.f/(softness+body_sum);
    c.diagonal=diagonal;c.cfm=diagonal*softness;
    length-=spring.rest;relative=-relative;
    c.rhs=(length*(spring_dt*softness))*diagonal+diagonal*relative;
    if(damping_force<0)damping_force=0;
    c.lower_limit=-((spring.stiffness*spring.compression+damping_force)*info.m_timeStep);
    length+=spring.compression;
    if(.03999999910593033f<=length){c.push_diagonal=0;c.correction_rhs=0;c.push_rhs=0;}
    else{
        c.push_diagonal=1.f/body_sum;
        if(!info.m_splitImpulse||(info.m_splitImpulsePenetrationThreshold<=length&&length!=info.m_splitImpulsePenetrationThreshold)){
            c.push_rhs=0;c.correction_rhs=((length*info.m_erp)/info.m_timeStep+relative)*c.push_diagonal;
        }else{
            c.correction_rhs=c.push_diagonal*relative;c.push_rhs=((length*info.m_erp2)/info.m_timeStep)*c.push_diagonal;
        }
    }
}
void solve_wheel_penetration(PenetrationWheel* wheels,std::size_t count,int activation,bool active){
    if(activation==ISLAND_SLEEPING||!active)return;
    for(std::size_t i=0;i<count;++i){auto& w=wheels[i];if(!w.contact)continue;auto& c=*w.constraint;if(c.push_rhs==0)continue;
        auto& a=*w.a;auto& b=*w.b;const auto& j=c.jacobian;
        float relative=(((j.angular_b.y()*b.m_turnVelocity.y()+b.m_turnVelocity.x()*j.angular_b.x())+j.angular_b.z()*b.m_turnVelocity.z())
            -((j.axis.x()*b.m_pushVelocity.x()+j.axis.y()*b.m_pushVelocity.y())+j.axis.z()*b.m_pushVelocity.z()));
        relative+=j.angular_a.y()*a.m_turnVelocity.y();relative+=a.m_turnVelocity.x()*j.angular_a.x();relative+=j.angular_a.z()*a.m_turnVelocity.z();
        relative+=j.axis.y()*a.m_pushVelocity.y();relative+=j.axis.x()*a.m_pushVelocity.x();relative+=j.axis.z()*a.m_pushVelocity.z();
        float impulse=c.push_rhs-relative*c.push_diagonal;const float accumulated=c.push_impulse+impulse;
        if(accumulated<=0)c.push_impulse=accumulated;else{impulse=0.f-c.push_impulse;c.push_impulse=0;}
        impulse*=j.projection;
        // These explicit associations retain GE6's application order. Using
        // Bullet's convenience applyPushImpulse would reorder products.
        if(a.m_originalBody){
            a.m_pushVelocity[0]=((w.normal.x()*a.m_linearFactor.x())*impulse)*a.m_invMass.x()+a.m_pushVelocity.x();
            a.m_pushVelocity[1]=((a.m_linearFactor.y()*w.normal.y())*impulse)*a.m_invMass.y()+a.m_pushVelocity.y();
            a.m_pushVelocity[2]=((a.m_linearFactor.z()*w.normal.z())*impulse)*a.m_invMass.z()+a.m_pushVelocity.z();
            for(int k=0;k<3;++k)a.m_turnVelocity[k]=(impulse*a.m_angularFactor[k])*j.component_a[k]+a.m_turnVelocity[k];
        }
        impulse=-impulse;
        if(b.m_originalBody){
            b.m_pushVelocity[0]=((w.normal.x()*b.m_linearFactor.x())*impulse)*b.m_invMass.x()+b.m_pushVelocity.x();
            b.m_pushVelocity[1]=((b.m_linearFactor.y()*w.normal.y())*impulse)*b.m_invMass.y()+b.m_pushVelocity.y();
            b.m_pushVelocity[2]=((b.m_linearFactor.z()*w.normal.z())*impulse)*b.m_invMass.z()+b.m_pushVelocity.z();
            // Native writes z,x,y; preserve that even when A and B alias.
            b.m_turnVelocity[2]=(impulse*b.m_angularFactor.z())*j.component_b.z()+b.m_turnVelocity.z();
            b.m_turnVelocity[0]=(impulse*b.m_angularFactor.x())*j.component_b.x()+b.m_turnVelocity.x();
            b.m_turnVelocity[1]=(impulse*b.m_angularFactor.y())*j.component_b.y()+b.m_turnVelocity.y();
        }
    }
}
}
