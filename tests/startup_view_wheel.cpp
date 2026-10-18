#include "scene_view_lifecycle.h"
#include "wheel_forces.h"
#include "wheel_record.h"
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
namespace {
void check(bool b){if(!b)throw std::runtime_error("Startup/view/wheel fixture failed");}
struct Buffers {
    FS15SceneRenderView* view;unsigned created=0,destroyed=0;bool null_first=false;
    static uintptr_t create(void* p,uint32_t n,uintptr_t initial,uint32_t flags){
        auto& s=*static_cast<Buffers*>(p);++s.created;
        check(n==(s.created==1?196u:176u)&&!initial&&flags==8);
        check(s.view->frustum.vtable==0x1404c7cc0&&s.view->world[15]==1);
        if(s.created==2)check(s.view->buffer==(s.null_first?0u:1u));
        return s.null_first&&s.created==1?0:s.created;
    }
    static void destroy(void* p,uintptr_t token,uint32_t deleting){
        auto& s=*static_cast<Buffers*>(p);check(deleting==1&&s.view->frustum.vtable==0x1404c7cc0);
        ++s.destroyed;check(token==(s.null_first?2:s.destroyed));
    }
};
}
int main(){try{
    for(bool null_first:{false,true}){
        FS15SceneRenderView v;std::memset(&v,0xa5,sizeof(v));const auto before=v;
        FS15SceneViewContext c{};c.width=1920;c.height=1080;
        Buffers b{&v,0,0,null_first};FS15SceneViewLifetimeServices s{&b,Buffers::create,Buffers::destroy};
        check(!fs15_scene_view_construct(&v,sizeof(v)-1,&c,sizeof(c),&s));
        check(std::memcmp(&v,&before,sizeof(v))==0&&b.created==0);
        check(fs15_scene_view_construct(&v,sizeof(v),&c,sizeof(c),&s));
        check(b.created==2&&v.settings==&c.settings&&v.secondary_buffer==2);
        check(std::memcmp(v.combined,before.combined,64)==0&&std::memcmp(&v.unknown19c,&before.unknown19c,4)==0);
        check(fs15_scene_view_destruct(&v,sizeof(v),&s));
        check(b.destroyed==(null_first?1u:2u)&&v.frustum.vtable==0x1404c1010);
    }
    fs15::WheelRecord r{};r.shape_id=7;r.radius=0.5f;r.lateral_stiffness=2;r.reference_load=1;r.longitudinal_stiffness=8;r.friction=1.1f;
    fs15::WheelSet set{};set.count=1;set.records=&r;
    fs15::WheelTreeNode nil{},node{};nil.is_nil=1;nil.parent=&node;node.left=node.right=&nil;node.actor_key=17;node.value=&set;
    fs15::PhysicsManagerView manager{};manager.wheels=&nil;
    fs15::PhysicsActorData data{};data.flags=8;data.actor_key=17;
    fs15::SceneObjectView object{};object.physics=&data;
    uint8_t frame[0x130];std::memset(frame,0x5a,sizeof(frame));
    auto input=[&](int32_t id,float load){const float sx=0.2f,angle=-0.1f;std::memcpy(frame,&id,4);std::memcpy(frame+0x10,&sx,4);std::memcpy(frame+0x20,&angle,4);std::memcpy(frame+0x30,&load,4);};
    input(7,2);check(fs15_script_wheel_forces(&manager,&object,frame,sizeof(frame),nullptr,nullptr));
    float longitudinal,lateral;std::memcpy(&longitudinal,frame+0xf0,4);std::memcpy(&lateral,frame+0x100,4);
    check(longitudinal>0&&lateral>0&&frame[0xf8]==3&&frame[0x108]==3&&frame[0xfc]==0x5a);
    for(float load:{-1.f,0.f,1e-5f}){input(7,load);check(fs15_script_wheel_forces(&manager,&object,frame,sizeof(frame),nullptr,nullptr));std::memcpy(&longitudinal,frame+0xf0,4);check(longitudinal==0);}
    input(99,2);check(fs15_script_wheel_forces(&manager,&object,frame,sizeof(frame),nullptr,nullptr));std::memcpy(&longitudinal,frame+0xf0,4);check(longitudinal==0);
    input(7,2);data.flags=0;check(fs15_script_wheel_forces(&manager,&object,frame,sizeof(frame),nullptr,nullptr));std::memcpy(&longitudinal,frame+0xf0,4);check(longitudinal==0);data.flags=8;
    uint8_t short_saved[sizeof(frame)];std::memcpy(short_saved,frame,sizeof(frame));
    check(!fs15_script_wheel_forces(&manager,&object,frame,sizeof(frame)-1,nullptr,nullptr)&&std::memcmp(short_saved,frame,sizeof(frame))==0);
    input(7,std::numeric_limits<float>::infinity());uint8_t saved[sizeof(frame)];std::memcpy(saved,frame,sizeof(frame));
    check(!fs15_script_wheel_forces(&manager,&object,frame,sizeof(frame),nullptr,nullptr)&&std::memcmp(saved,frame,sizeof(frame))==0);
    std::cout<<"Recovered view lifetime and wheel-force adapter fixtures passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
