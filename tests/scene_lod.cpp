// Asset-free LOD ownership, exact distance boundaries and native dispatch order.
#include "scene_lod.h"
#include "scene_owned.h"
#include "animation_fixture.h"
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <vector>
namespace {
using fs15_animation_test::Owner;
using fs15_animation_test::require;
struct Fixture:Owner {
    std::vector<FS15SceneStateRecord*> nodes;
    std::vector<uint8_t> kinds;
    FS15SceneLodContext lod{};
    FS15SceneLodServices services{this,&memory,nullptr,world,mark};
    std::array<size_t,32> requests{};FS15SceneLodQueue queue{requests.data(),requests.size(),0};
    size_t notifications=0;uint32_t attached=0;int warning=0;
    Fixture() {
        nodes.push_back(node);kinds.push_back(1);memory.unlink=unlink;memory.destroy_child=destroy;
        for(size_t i=1;i<8;++i) {
            const bool is_lod=i==1||i==5;const size_t bytes=is_lod?sizeof(FS15SceneLodRecord):sizeof(FS15SceneStateRecord);
            auto* p=static_cast<FS15SceneStateRecord*>(allocate(this,bytes));
            if(is_lod) {
                const float distances[]={0,10,100,1000};
                require(fs15_scene_lod_construct(reinterpret_cast<FS15SceneLodRecord*>(p),bytes,"lod",4,distances,i==1?3:2,&memory),"Construct owned LOD");
            }else require(fs15_scene_construct(p,bytes,FS15_OBJECT_TRS,"node",5,0,1,&memory),"Construct owned TRS");
            nodes.push_back(p);kinds.push_back(is_lod?2:1);
        }
    }
    FS15SceneOwnedView view(){return {nodes.data(),nodes.size(),kinds.data(),0};}
    static Fixture& self(void* p){return *static_cast<Fixture*>(static_cast<Owner*>(p));}
    static int mark(void*,FS15SceneStateRecord* n,uint32_t){return fs15_scene_owned_mark_branch(n);}
    static int world(void* p,FS15SceneStateRecord* n,float* out) {
        auto& f=*static_cast<Fixture*>(p);auto v=f.view();size_t index=0;
        while(index<f.nodes.size()&&f.nodes[index]!=n)++index;
        return fs15_scene_owned_world(&v,index,out,3);
    }
    static int unlink(void*,void* parent,uint32_t index,int suppress) {
        auto& n=*static_cast<FS15SceneStateRecord*>(parent);if(!suppress||n.vtable!=0x1404c7dd0)return 0;
        auto* children=reinterpret_cast<uintptr_t*>(n.children_begin);const size_t size=(n.children_end-n.children_begin)/8;
        if(index>=size)return 0;
        reinterpret_cast<FS15SceneStateRecord*>(children[index])->parent=0;
        for(size_t i=index;i+1<size;++i)children[i]=children[i+1];n.children_end-=8;return 1;
    }
    static int destroy(void* p,void* child) {
        auto& f=self(p);auto* n=static_cast<FS15SceneStateRecord*>(child);
        if(n->vtable==0x1404c1c48)return fs15_scene_lod_destruct(static_cast<FS15SceneLodRecord*>(child),sizeof(FS15SceneLodRecord),1,&f.lod,&f.services);
        return fs15_scene_destruct(n,sizeof(*n),FS15_OBJECT_TRS,1,&f.memory);
    }
    void enqueue() {
        for(size_t i=0;i<queue.count;++i)require(fs15_scene_lod_enqueue(&lod,reinterpret_cast<FS15SceneLodRecord*>(nodes[requests[i]]),&memory),"Append LOD request");
        queue.count=0;
    }
    void flush() {
        for(uintptr_t p=lod.pending.begin;p<lod.pending.end;p+=8)
            require(fs15_scene_lod_update(*reinterpret_cast<FS15SceneLodRecord**>(p),&lod,&services),"Update world position and LOD registry");
        lod.pending.end=lod.pending.begin;
    }
    void link(size_t parent,size_t child) {
        auto v=view();require(fs15_scene_owned_link_lod(&v,parent,child,UINT32_MAX,1,&memory,nullptr,0,&notifications,&attached,&queue),"Link LOD forest");enqueue();
    }
    void state(size_t index,uint32_t mask) {
        auto v=view();require(fs15_scene_owned_state_lod(&v,index,FS15_SCENE_PROPAGATE,mask,0,nullptr,0,&notifications,&queue),"LOD-aware state dispatch");enqueue();
    }
    void close() {
        flush();require(fs15_scene_destruct(node,sizeof(*node),FS15_OBJECT_TRS,1,&memory),"Recursive LOD forest destruction");node=nullptr;
        require(lod.manager.count==0&&lod.pending.begin==lod.pending.end,"No registered or pending destroyed LOD");
        release(this,lod.manager.entries);release(this,reinterpret_cast<void*>(lod.pending.begin));lod={};
        require(blocks.empty()&&registry.empty(),"All LOD buffers and registrations released");
    }
};
void run() {
    Fixture f;f.link(0,1);f.link(0,5);f.link(1,2);f.link(1,3);f.link(1,4);f.link(5,6);f.link(5,7);
    auto* a=reinterpret_cast<FS15SceneLodRecord*>(f.nodes[1]);auto* b=reinterpret_cast<FS15SceneLodRecord*>(f.nodes[5]);uint32_t token=0;
    require(a->distances[0]==0&&a->distances[1]==10&&a->distances[2]==100&&a->distances[3]==100,"Constructor repeats last supplied distance");
    require(fs15_scene_lod_child_token(a,f.nodes[3],&token)&&token==UINT32_MAX,"Unregistered child token is invalid");
    f.state(0,0x3f);require(f.lod.pending.end-f.lod.pending.begin==16,"Visibility queues two groups");
    f.flush();require(f.lod.manager.count==2&&f.lod.manager.capacity==1024&&a->index==0&&b->index==1,"Initial 1024-entry registry and owner indices");
    require(fs15_scene_inherited_lod_token(f.nodes[3],&token)&&token==0x40000000u,"Child branch encoded in upper two bits");
    int visible=0;auto& m=f.lod.manager;m.distance_factor_squared=1;
    m.viewer[0]=9;++m.stamp;require(fs15_scene_lod_test(&m,0,&visible)&&visible,"First level below its next threshold");
    m.viewer[0]=100;require(fs15_scene_lod_test(&m,0,&visible)&&visible,"Unchanged stamp retains cached selection");
    m.stamp+=65536;require(fs15_scene_lod_test(&m,0,&visible)&&visible,"Native cache compares only low sixteen epoch bits");
    m.viewer[0]=10;++m.stamp;require(fs15_scene_lod_test(&m,0x40000000u,&visible)&&visible,"Exact threshold selects the next level");
    m.viewer[0]=100;++m.stamp;require(fs15_scene_lod_test(&m,0x80000000u,&visible)&&visible,"Last level at final threshold");
    m.viewer[0]=19;m.distance_factor_squared=4;++m.stamp;require(fs15_scene_lod_test(&m,0,&visible)&&visible,"Squared distance factor scales level boundary");
    const auto saved_factor=m.distance_factor_squared;m.distance_factor_squared=std::numeric_limits<float>::quiet_NaN();
    require(!fs15_scene_lod_test(&m,0,&visible),"Nonfinite distance factor rejected");m.distance_factor_squared=saved_factor;
    a->node.flags&=~uint32_t(1);f.state(1,2);f.flush();
    require(m.count==1&&b->index==0&&a->index==0x3fffffffu&&!(a->node.flags&0x200u),"Hiding swap-removes and updates moved owner index");
    require(fs15_scene_inherited_lod_token(f.nodes[3],&token)&&token==UINT32_MAX,"Hidden unregistered ancestor invalidates branch token");
    b->node.translation[0]=500;auto v=f.view();
    require(fs15_scene_owned_trs_lod(&v,5,FS15_SCENE_TRS_REBUILD,nullptr,0,nullptr,0,&f.notifications,&f.warning,&f.queue),"LOD TRS rebuild uses derived state dispatch");f.enqueue();f.flush();
    require(m.entries[0].position[0]==500&&m.entries[0].stamp==static_cast<uint16_t>(m.stamp-1u),"Registered move updates world position and invalidates cache");
    FS15SceneLodQueue short_queue{nullptr,0,0};const auto before=b->node;
    require(!fs15_scene_owned_state_lod(&v,5,0,1,0,nullptr,0,&f.notifications,&short_queue)&&short_queue.count==1&&std::memcmp(&b->node,&before,sizeof(before))==0,"Queue capacity guard preserves live records");
    require(!fs15_scene_owned_state(&v,5,0,1,0,nullptr,0,&f.notifications),"Original base/TRS API rejects unsupported derived dispatch");
    f.close();
    Owner owner;FS15SceneLodManager empty{};FS15SceneLodRecord record{};const float position[3]={0,0,0},distances[2]={0,10};uint32_t index=99;
    owner.fail_after=0;require(!fs15_scene_lod_register(&empty,&record,position,distances,2,&index,&owner.memory)&&empty.entries==nullptr&&empty.capacity==0&&index==99,"Allocation guard retains caller-owned registry state");owner.fail_after=-1;owner.cleanup();
}
}
int main(){try{run();std::cout<<"LOD ownership and distance checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
