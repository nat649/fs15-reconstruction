#pragma once
// Shared recovered seed initialization (0x1400524b0) and two refinements.
// Callers retain each native routine's float squared-length operation order.
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
namespace fs15_scene_math_detail {
inline uint32_t bits(float f){uint32_t b;std::memcpy(&b,&f,4);return b;}
inline float value(uint32_t b){float f;std::memcpy(&f,&b,4);return f;}
inline const std::array<uint32_t,512>& rsqrt_table(){
    static const auto data=[]{std::array<uint32_t,512> a{};for(uint32_t i=0;i<512;++i){const float seed=value((i|0x7e00u)<<15);a[i]=(bits(static_cast<float>(1.0/std::sqrt(static_cast<double>(seed))))+0x2000u)&0x7f8000u;}a[256]=0x7f8000u;return a;}();return data;
}
inline float inverse_length(float squared){
    const uint32_t b=bits(squared),seed=(((0x17cu-((b>>23)&255u))<<22)&0xff800000u)|rsqrt_table()[(b>>15)&511u];
    double r=static_cast<double>(value(seed));const double half=static_cast<double>(squared)*0.5;r=r*(1.5-(r*r)*half);return static_cast<float>((1.5-(r*r)*half)*r);
}
}
