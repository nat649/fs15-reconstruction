// Authored lifecycle invariants; native differential evidence is separate.
#include "scene_state.h"
#include "scene_graph.h"
#include <array>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
struct Fixture {
    std::array<FS15SceneStateRecord,5> nodes{};
    std::array<FS15ScenePropertyView,5> props{};
    std::array<uint8_t,5> kinds={0,1,0,1,0};
    std::array<uintptr_t,40> links{};
    std::array<uintptr_t,2> observers={123,456};
    FS15SceneStateArena arena{};
    uintptr_t pointer(size_t i){return reinterpret_cast<uintptr_t>(&nodes[i]);}
    Fixture() {
        const float translations[]={0,10,2,20,5};
        for(size_t i=0;i<nodes.size();++i) {
            auto& n=nodes[i];n.type_flags=0x20;n.flags=0x51;
            n.local[0]=n.local[5]=n.local[10]=n.local[15]=1;n.local[12]=translations[i];
            n.children_begin=n.children_end=reinterpret_cast<uintptr_t>(&links[i*8]);
            n.children_capacity=n.children_begin+8*sizeof(uintptr_t);
            n.observers_begin=n.observers_end=n.observers_capacity=reinterpret_cast<uintptr_t>(observers.data());
            n.properties=reinterpret_cast<uintptr_t>(&props[i]);n.local_clip[1]=100;
            n.local_masks[0]=n.local_masks[1]=0xffffffff;
        }
        for(const auto& edge:std::array<std::array<size_t,2>,3>{{{0,1},{1,2},{2,4}}}) {
            const auto parent=edge[0],child=edge[1];nodes[child].parent=pointer(parent);
            links[parent*8]=pointer(child);nodes[parent].children_end+=sizeof(uintptr_t);
        }
        nodes[3].flags&=~uint32_t(0x10);
        nodes[2].observers_end+=sizeof(uintptr_t);nodes[2].observers_capacity=nodes[2].observers_end;
        nodes[4].observers_begin+=sizeof(uintptr_t);nodes[4].observers_end=nodes[4].observers_begin+sizeof(uintptr_t);
        nodes[4].observers_capacity=nodes[4].observers_end;
        arena={nodes.data(),sizeof(nodes),sizeof(nodes[0]),links.data(),links.size(),observers.data(),observers.size(),
            props.data(),props.size(),kinds.data(),kinds.size(),0};
    }
};
void run() {
    Fixture f;std::array<FS15SceneNotification,8> events{};size_t used=0;uint32_t slot=99;
    require(fs15_scene_link(&f.arena,f.links.data(),3,2,0xffffffff,1,events.data(),events.size(),&used,&slot),"Reparent subtree");
    require(slot==0&&f.nodes[2].parent==f.pointer(3)&&f.links[24]==f.pointer(2),"Parent and ordered child vector agree");
    require(f.nodes[1].children_begin==f.nodes[1].children_end&&f.links[8]==f.pointer(2),"Native erase preserves stale trailing slot");
    require(!(f.nodes[0].flags&0x10)&&!(f.nodes[1].flags&0x10)&&(f.nodes[3].flags&0x10),"Observer presence follows subtree to new ancestors");
    require(used==4&&events[0].node_index==2&&events[1].node_index==4&&events[2].node_index==2&&events[3].node_index==4,"Detach then attach notification ordering");
    for(size_t i=0;i<used;++i)require(events[i].origin==0,"Native hierarchy notifications use origin zero");
    require(fs15_scene_update_world(f.nodes.data(),sizeof(f.nodes),sizeof(f.nodes[0]),4),"Resolve new world chain");
    require(f.nodes[4].world[12]==27,"Reparent retains local coordinates and changes descendant world pose");
    require(fs15_scene_link(&f.arena,f.links.data(),3,1,0,1,events.data(),events.size(),&used,&slot)&&slot==0,"Insert before existing child");
    require(f.links[24]==f.pointer(1)&&f.links[25]==f.pointer(2),"Insertion order retained");
    require(fs15_scene_link(&f.arena,f.links.data(),3,1,0xffffffff,0,events.data(),events.size(),&used,&slot)&&slot==1,"Same-parent raw reorder uses post-removal count");
    require(f.links[24]==f.pointer(2)&&f.links[25]==f.pointer(1),"Same-parent reorder rotates children");
    const auto child_before=f.nodes[2];
    require(fs15_scene_unlink(&f.arena,f.links.data(),3,0,1,events.data(),events.size(),&used),"Suppressed unlink");
    auto expected=child_before;expected.parent=0;
    require(!used&&std::memcmp(&expected,&f.nodes[2],sizeof(expected))==0,"Suppressed unlink only clears child's parent");
    require(!(f.nodes[3].flags&0x10),"Suppressed unlink still rescans old parent");

    Fixture capacity;const auto before=capacity.nodes;const auto links_before=capacity.links;slot=1234;
    events[0]={98,97,96,95};const auto old_event=events[0];
    require(!fs15_scene_link(&capacity.arena,capacity.links.data(),3,2,0,1,events.data(),1,&used,&slot)&&used==4,"Report required notification capacity");
    require(std::memcmp(before.data(),capacity.nodes.data(),sizeof(before))==0&&capacity.links==links_before&&slot==1234&&
        std::memcmp(&old_event,&events[0],sizeof(old_event))==0,"Event capacity failure preserves nodes, links, output index and events");
    capacity.nodes[3].children_capacity=capacity.nodes[3].children_begin;
    const auto full_before=capacity.nodes;
    require(!fs15_scene_link(&capacity.arena,capacity.links.data(),3,2,0,1,events.data(),events.size(),&used,&slot)&&
        std::memcmp(full_before.data(),capacity.nodes.data(),sizeof(full_before))==0&&capacity.links==links_before,"No allocator: full target rejects atomically");
    Fixture cycle;
    require(!fs15_scene_link(&cycle.arena,cycle.links.data(),4,0,0,1,events.data(),events.size(),&used,&slot),"Reject ancestor under descendant");
    require(!fs15_scene_link(&cycle.arena,cycle.links.data(),2,2,0,1,events.data(),events.size(),&used,&slot),"Reject self-parenting");
    require(!fs15_scene_unlink(&cycle.arena,cycle.links.data(),0,1,0,events.data(),events.size(),&used),"Reject nonexistent child slot");
    require(!fs15_scene_link(&cycle.arena,cycle.links.data(),0,1,0,7,events.data(),events.size(),&used,&slot),"Reject unknown link dispatch");
    std::array<uintptr_t,40> wrong{};
    require(!fs15_scene_link(&cycle.arena,wrong.data(),3,2,0,1,events.data(),events.size(),&used,&slot),"Require matching writable view");
    Fixture overlap;overlap.nodes[3].children_begin=overlap.nodes[3].children_end=overlap.nodes[0].children_begin;
    overlap.nodes[3].children_capacity=overlap.nodes[0].children_capacity;
    require(!fs15_scene_link(&overlap.arena,overlap.links.data(),3,2,0,1,events.data(),events.size(),&used,&slot),"Reject overlapping reserved vectors");
    Fixture missing;missing.nodes[1].children_end=missing.nodes[1].children_begin;
    require(!fs15_scene_link(&missing.arena,missing.links.data(),3,2,0,1,events.data(),events.size(),&used,&slot),"Reject missing reciprocal child link");

    // Identity-root optimization intentionally does not dirty an attached root.
    Fixture identity;require(fs15_scene_unlink(&identity.arena,identity.links.data(),0,0,1,events.data(),events.size(),&used),"Detach fixture subtree");
    identity.nodes[1].flags&=~uint32_t(0x40);
    require(fs15_scene_link(&identity.arena,identity.links.data(),0,1,0,1,events.data(),events.size(),&used,&slot)&&
        !(identity.nodes[1].flags&0x40)&&used==0,"Identity root avoids unnecessary transform notifications");
    Fixture translated;translated.nodes[3].local[12]=0.0001f;
    translated.nodes[0].flags&=~uint32_t(0x40);
    require(fs15_scene_link(&translated.arena,translated.links.data(),3,0,0,1,events.data(),events.size(),&used,&slot)&&
        (translated.nodes[0].flags&0x40)&&used==2,"Native identity threshold is strict, translation at epsilon is transformed");
    Fixture below;below.nodes[3].local[12]=0.00005f;below.nodes[0].flags&=~uint32_t(0x40);
    require(fs15_scene_link(&below.arena,below.links.data(),3,0,0,1,events.data(),events.size(),&used,&slot)&&
        !(below.nodes[0].flags&0x40)&&used==0,"Translation below identity epsilon skips transform invalidation");
}
}
int main() {
    try {run();std::cout<<"Recovered hierarchy invariants and authored capacity/forest guards passed; no native allocator or destructor tested.\n";return 0;}
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
