#include "vehicle_drive.h"
#include <array>
#include <cmath>
namespace fs15::ge6 {
void prepare_differentials(DriveWheel* wheels,const Differential* nodes,std::size_t count,const VehicleMotor& m){
    if(!count)return;
    std::array<float,20> speeds{},wa{},wb{};
    const float direction=0<=m.ratio*m.torque?1.f:-1.f;
    for(std::size_t i=0;i<count;++i){const auto& n=nodes[i];
        const float radius_a=n.a<20?wheels[n.a].radius:1.f,radius_b=n.b<20?wheels[n.b].radius:1.f;
        float a=n.a<20?radius_a*wheels[n.a].speed:speeds[n.a-20],b=n.b<20?radius_b*wheels[n.b].speed:speeds[n.b-20];
        speeds[i]=(b+a)*.5f;float left=n.ratio,right=1.f-left;
        if(left<.9999f&&.0001f<left){
            a*=direction;b*=direction;const float high=b<=a?a:b,low=a<=b?a:b;bool normalize=false;
            if(high<0||low<0){
                if(0<high||0<low){if(a<b){right=0;left=1;}else{left=0;right=1;}}
                else{
                    float error=low*n.bias-high;
                    if(0<error){const float adjustment=-(error/high);left*=a<b?adjustment+1.f:1.f-adjustment;right*=a<b?1.f-adjustment:adjustment+1.f;normalize=true;}
                }
            }else{
                float error=high-low*n.bias;
                if(0<error){const float adjustment=error/high;left*=a<b?adjustment+1.f:1.f-adjustment;right*=a<b?1.f-adjustment:adjustment+1.f;normalize=true;}
            }
            if(normalize){const float inverse=1.f/(right+left);left*=inverse;right*=inverse;}
        }
        wa[i]=left*radius_a;wb[i]=right*radius_b;
    }
    for(std::size_t i=count;i-->0;){const auto& n=nodes[i];
        if(n.a<20)wheels[n.a].weight=wa[i];else{wa[n.a-20]*=wa[i];wb[n.a-20]*=wa[i];}
        if(n.b<20)wheels[n.b].weight=wb[i];else{wa[n.b-20]=wb[i]*wa[n.b-20];wb[n.b-20]=wb[i]*wb[n.b-20];}
    }
}
void solve_drivetrain(DriveWheel* wheels,const Differential* nodes,std::size_t count,VehicleMotor& m,float dt){
    if(!count||m.ratio==0)return;
    std::array<float,20> speeds{},effective{},coast_effective{},weights{},coast_weights{};
    for(std::size_t i=0;i<count;++i){const auto& n=nodes[i];float a,b,ia,ib,ca,cb;
        if(n.a<20){const auto& w=wheels[n.a];a=w.radius*w.speed;ia=(w.radius*w.inverse_inertia)*w.radius;ca=ia;}else{a=speeds[n.a-20];ia=effective[n.a-20];ca=coast_effective[n.a-20];}
        if(n.b<20){const auto& w=wheels[n.b];b=w.radius*w.speed;ib=(w.radius*w.inverse_inertia)*w.radius;cb=ib;}else{b=speeds[n.b-20];ib=effective[n.b-20];cb=coast_effective[n.b-20];}
        float ratio=n.ratio,drive=ratio,aa=std::abs(a),ab=std::abs(b),high=ab<=aa?aa:ab,low=ab<aa?ab:aa;
        if(.01f<high){
            if((0<a)==(0<b)){
                const float excess=high-low*n.bias;
                if(0<excess){float blend=1;const float width=(low*n.bias)*.3f;if(excess<width)blend=excess/width;
                    float numerator,other;
                    if(a<=b){drive=(blend+1.f)*ratio;drive=.0001f<=drive?drive/((1.f-blend)*(1.f-ratio)+drive):0.f;numerator=(1.f-blend)*ratio;other=blend+1.f;
                        ratio=.0001<=double(numerator)?numerator/((1.f-ratio)*other+numerator):0.f;
                    }else{drive=(1.f-blend)*ratio;drive=.0001f<=drive?drive/((1.f-ratio)*(blend+1.f)+drive):0.f;numerator=(blend+1.f)*ratio;other=1.f-blend;
                        ratio=.0001f<=numerator?numerator/((1.f-ratio)*other+numerator):0.f;
                    }
                }
            }else{drive=b<a?.001f:.999f;ratio=1.f-drive;}
        }
        weights[i]=drive;coast_weights[i]=ratio;speeds[i]=(b+a)*.5f;
        effective[i]=((1.f-drive)*ib+drive*ia)*.5f;coast_effective[i]=((1.f-ratio)*cb+ratio*ca)*.5f;
    }
    if(m.speed<m.target_speed&&0<m.torque){
        const float limit=(m.torque*m.inverse_inertia)*dt;float delta=m.target_speed-m.speed;if(limit<=delta)delta=limit;
        const float next=m.torque_impulse+delta;
        if(next<=limit){if(0<=next)m.torque_impulse=next;else{delta=-m.torque_impulse;m.torque_impulse=0;}}
        else{delta=limit-m.torque_impulse;m.torque_impulse=limit;}m.speed+=delta;
    }
    const auto root=count-1;const float root_speed=speeds[root];float delta=m.speed-root_speed*m.ratio;
    if(m.ratio*delta<0){effective=coast_effective;weights=coast_weights;}
    const float root_effective=effective[root],bound=dt*m.clutch_torque;
    delta=(1.f/(((root_effective*m.ratio)*m.ratio)+m.inverse_inertia))*delta;
    const float next=m.clutch_impulse+delta;
    if(bound<next){delta=bound-m.clutch_impulse;m.clutch_impulse=bound;}
    else if(next<-bound){delta=-bound-m.clutch_impulse;m.clutch_impulse=-bound;}
    else m.clutch_impulse=next;
    m.speed=-(m.inverse_inertia*delta)+m.speed;
    float clamped=0;if(0<=m.speed){clamped=m.speed;if(m.target_speed<m.speed)clamped=m.target_speed;}m.speed=clamped;
    effective[root]=m.ratio*delta;m.driven_speed=((root_effective*m.ratio)*delta+root_speed)*m.ratio;
    for(std::size_t i=count;i-->0;){const auto& n=nodes[i];const float a=weights[i]*effective[i],b=(1.f-weights[i])*effective[i];
        if(n.a<20){auto& w=wheels[n.a];w.speed=(a*w.radius)*w.inverse_inertia+w.speed;}else effective[n.a-20]=a;
        if(n.b<20){auto& w=wheels[n.b];w.speed=(b*w.radius)*w.inverse_inertia+w.speed;}else effective[n.b-20]=b;
    }
}
}
