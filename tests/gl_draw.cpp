#include "gl_draw.h"
#include <array>
#include <vector>
#include <stdexcept>
#include <iostream>
int main(){try{
    using Event=std::array<uint64_t,7>;std::vector<Event> events;FS15GLDrawServices s{};s.user=&events;
    s.point_size=[](void* p,float v){static_cast<std::vector<Event>*>(p)->push_back({1,uint64_t(v),0,0,0,0,0});};
    s.arrays=[](void* p,uint32_t m,int32_t f,int32_t n){static_cast<std::vector<Event>*>(p)->push_back({2,m,uint32_t(f),uint32_t(n),0,0,0});};
    s.elements=[](void* p,uint32_t m,int32_t n,uint32_t t,uintptr_t o){static_cast<std::vector<Event>*>(p)->push_back({3,m,uint32_t(n),t,o,0,0});};
    s.range=[](void* p,uint32_t m,uint32_t a,uint32_t b,int32_t n,uint32_t t,uintptr_t o){static_cast<std::vector<Event>*>(p)->push_back({4,m,a,b,uint32_t(n),t,o});};
    FS15GLDrawState state{};fs15_gl_draw(&state,3,2,0,0,0,&s);if(!events.empty())throw std::runtime_error("Draw issued without native layout");
    state.layout=1;fs15_gl_draw(&state,3,2,0,0,0,&s);if(events.back()!=Event{2,4,2,3,0,0,0})throw std::runtime_error("Non-indexed draw arguments differ");
    FS15GLIndexBuffer index{};index.type=0x1403;state.index=&index;fs15_gl_draw(&state,6,-1,0,4,0,&s);if(events.back()!=Event{3,4,6,0x1403,0xfffffffe,0,0})throw std::runtime_error("Native index-byte wrapping lost");
    index.type=0x1405;fs15_gl_draw(&state,9,3,1,0xfffffffe,4,&s);if(events.back()!=Event{4,5,0xfffffffe,1,9,0x1405,12})throw std::runtime_error("Native range draw differs");
    events.clear();fs15_gl_draw(&state,1,0,4,0,0,&s);fs15_gl_draw(&state,1,0,4,0,0,&s);if(events.size()!=3||events[0][0]!=1||state.point_size!=5)throw std::runtime_error("Native point size cache lost");
    std::cout<<"GE6 draw dispatch: arrays, indexed offsets, wrapped ranges and cached points passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
