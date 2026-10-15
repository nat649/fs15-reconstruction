#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>

// Authored host scheduling, not a recovered GE6 physics tick implementation.
// Rendering may vary; every gameplay update receives the same time quantum.
class SimulationClock {
    double remainder_=0, discarded_=0;
    std::uint64_t ticks_=0;
    bool paused_=false;
public:
    static constexpr double step_seconds=1.0/60.0;
    static constexpr double maximum_frame_seconds=0.25;
    unsigned advance(double elapsed) {
        if(!std::isfinite(elapsed)||elapsed<0)throw std::invalid_argument("Invalid simulation elapsed time");
        if(paused_)return 0;
        const double accepted=std::min(elapsed,maximum_frame_seconds);
        discarded_+=elapsed-accepted;
        remainder_+=accepted;
        // Tolerate rounding of repeated fractions (e.g. 1/144), not a missing tick.
        const auto steps=unsigned(std::floor((remainder_+step_seconds*1e-9)/step_seconds));
        remainder_=std::max(0.0,remainder_-steps*step_seconds);
        ticks_+=steps;
        return steps;
    }
    void set_paused(bool value) {
        if(value!=paused_)remainder_=0; // No suspended time or fractional debt on resume.
        paused_=value;
    }
    bool paused()const{return paused_;}
    std::uint64_t ticks()const{return ticks_;}
    double discarded_seconds()const{return discarded_;}
};
