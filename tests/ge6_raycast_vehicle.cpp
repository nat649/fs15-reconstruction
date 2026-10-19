#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstddef>
extern "C" int fs15_ge6_vehicle_course(int,const float*,float*,std::size_t);
int main(){try{
    constexpr int frames=420;std::vector<float> commands(frames*5),output(frames*40);
    for(int scenario=0;scenario<3;++scenario){
        for(int i=0;i<frames;++i){float* c=commands.data()+i*5;c[0]=i<180?500.f:0.f;c[1]=300;c[2]=i<180?(scenario==1?-15.f:15.f):0.f;c[3]=scenario==2&&i>=60&&i<180?.08f:0;c[4]=i>=180?2000.f:0.f;}
        if(!fs15_ge6_vehicle_course(frames,commands.data(),output.data(),output.size()))throw std::runtime_error("GE6 vehicle course failed");
        for(float x:output)if(!std::isfinite(x))throw std::runtime_error("GE6 vehicle produced a nonfinite state");
        const float* rolling=output.data()+179*40;const float* stopped=output.data()+(frames-1)*40;
        const float rolling_speed=std::sqrt(rolling[3]*rolling[3]+rolling[5]*rolling[5]);const float stopped_speed=std::sqrt(stopped[3]*stopped[3]+stopped[5]*stopped[5]);
        std::cout<<"Scenario "<<scenario<<": displacement="<<rolling[0]<<','<<rolling[2]<<"; rolling="<<rolling_speed<<"; braked="<<stopped_speed<<"; height="<<stopped[1]<<'\n';
        if(rolling_speed<.5f||std::abs(rolling[2])<.5f)throw std::runtime_error("Recovered motor did not propel vehicle from rest");
        if(scenario==0&&rolling[2]<=0)throw std::runtime_error("Forward ratio did not move forward");
        if(scenario==1&&rolling[2]>=0)throw std::runtime_error("Reverse ratio did not reverse travel");
        if(scenario==2&&std::abs(rolling[0])<.1f)throw std::runtime_error("Steering did not change the vehicle path");
        if(stopped_speed>.15f||stopped[1]<.9f||stopped[1]>2)throw std::runtime_error("GE6 vehicle failed braking or suspension support");
    }
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
