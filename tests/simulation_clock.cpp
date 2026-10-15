#include "../src/engine/simulation_clock.h"
#include <iostream>
#include <limits>

static void require(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
int main(){try{
    for(unsigned hz:{5,15,30,60,144,240}){
        SimulationClock clock;
        for(unsigned i=0;i<hz*10;++i)clock.advance(1.0/hz);
        require(clock.ticks()==600,"Simulation tick count depends on render rate");
        require(clock.discarded_seconds()==0,"Normal frame time was discarded");
    }
    SimulationClock irregular;
    for(unsigned i=0;i<100;++i){irregular.advance(0.001);irregular.advance(0.004);irregular.advance(0.025);}
    require(irregular.ticks()==180,"Irregular frame partitions changed simulation duration");
    SimulationClock stalled;
    require(stalled.advance(10)==15,"Stall recovery is not bounded to 15 ticks");
    require(std::abs(stalled.discarded_seconds()-9.75)<1e-9,"Discarded stall time was not accounted for");
    stalled.advance(SimulationClock::step_seconds/2);stalled.set_paused(true);
    require(stalled.advance(120)==0&&stalled.ticks()==15,"Pause advanced simulation");
    stalled.set_paused(false);
    require(stalled.advance(SimulationClock::step_seconds/2)==0,"Pause retained fractional catch-up debt");
    require(stalled.advance(SimulationClock::step_seconds/2)==1,"Resume did not accept new elapsed time");
    for(double value:{-1.0,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}){
        bool rejected=false;try{stalled.advance(value);}catch(const std::invalid_argument&){rejected=true;}
        require(rejected,"Invalid elapsed time accepted");
    }
    std::cout<<"Fixed-step rates, irregular frames, stalls, pause/resume and invalid elapsed-time checks pass\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
