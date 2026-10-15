// Authored invariants/guard checks; native differential evidence is separate.
#include "scene_state.h"
#include "scene_graph.h"
#include <array>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
struct Fixture {
    std::array<FS15SceneStateRecord,3> nodes{};
    std::array<FS15ScenePropertyView,3> props{};
    std::array<uint8_t,3> kinds={0,1,0};
    std::array<uintptr_t,2> children{};
    std::array<uintptr_t,2> observers={123,456};
    FS15SceneStateArena arena{};
    Fixture() {
        children={reinterpret_cast<uintptr_t>(&nodes[1]),reinterpret_cast<uintptr_t>(&nodes[2])};
        for(size_t i=0;i<3;++i) {
            auto& n=nodes[i];n.type_flags=0x20;n.flags=1;
            n.local[0]=n.local[5]=n.local[10]=n.local[15]=1;
            std::memcpy(n.world,n.local,64);
            n.local[12]=float(i+1);n.world[12]=float(i+10);
            n.parent=i?reinterpret_cast<uintptr_t>(&nodes[0]):0;
            n.children_begin=n.children_end=n.children_capacity=reinterpret_cast<uintptr_t>(children.data());
            n.observers_begin=n.observers_end=n.observers_capacity=reinterpret_cast<uintptr_t>(observers.data());
            n.properties=reinterpret_cast<uintptr_t>(&props[i]);
            n.local_clip[0]=float(i+2);n.local_clip[1]=float(50-i);
            n.local_masks[0]=0xff;n.local_masks[1]=0x7f;
        }
        nodes[0].children_end+=2*sizeof(uintptr_t);nodes[0].children_capacity=nodes[0].children_end;
        nodes[0].observers_end+=sizeof(uintptr_t);nodes[0].observers_capacity=nodes[0].observers_end;
        nodes[1].observers_begin+=sizeof(uintptr_t);nodes[1].observers_end=nodes[1].observers_begin+sizeof(uintptr_t);
        nodes[1].observers_capacity=nodes[1].observers_end;
        arena={nodes.data(),sizeof(nodes),sizeof(nodes[0]),children.data(),children.size(),observers.data(),observers.size(),
            props.data(),props.size(),kinds.data(),kinds.size(),0};
    }
};
void run() {
    Fixture f;std::array<FS15SceneNotification,4> events{};size_t used=99;
    require(fs15_scene_state_update(&f.arena,0,FS15_SCENE_PROPAGATE,0x3f,0,events.data(),events.size(),&used),"Propagate subtree");
    require(used==2&&events[0].node_index==0&&events[0].observer==123&&events[1].node_index==1&&events[1].observer==456,"Depth-first notification order");
    for(const auto& n:f.nodes)require((n.flags&0x42)==0x42&&n.world_masks[0]==0xff&&n.world_clip[0]==2&&n.world_clip[1]==50,"Visibility, dirty flags, clip union and masks");
    require(fs15_scene_state_update(&f.arena,1,FS15_SCENE_RESCAN_OBSERVERS,1,0,events.data(),events.size(),&used)&&used==0,"Rescan child and ancestors");
    require((f.nodes[0].flags&0x10)&&(f.nodes[1].flags&0x10),"Observer presence reaches parent");
    f.nodes[0].observers_end=f.nodes[0].observers_begin;f.nodes[1].observers_end=f.nodes[1].observers_begin;
    require(fs15_scene_state_update(&f.arena,1,FS15_SCENE_RESCAN_OBSERVERS,1,0,events.data(),events.size(),&used),"Remove observer state");
    require(!(f.nodes[0].flags&0x10)&&!(f.nodes[1].flags&0x10),"Observer presence clears upward");
    require(fs15_scene_state_update(&f.arena,0,FS15_SCENE_MARK_INHERITED,0,0,events.data(),events.size(),&used),"Inherited flag traversal");
    for(const auto& n:f.nodes)require((n.flags&8)!=0,"Inherited flag visits every child");

    Fixture capacity;auto before=capacity.nodes;events[0]={99,98,97,96};const auto old_event=events[0];
    require(!fs15_scene_state_update(&capacity.arena,0,FS15_SCENE_PROPAGATE,1,0,events.data(),1,&used)&&used==2,"Insufficient event capacity reports requirement");
    require(std::memcmp(before.data(),capacity.nodes.data(),sizeof(before))==0&&std::memcmp(&old_event,&events[0],sizeof(old_event))==0,"Capacity failure is atomic");
    capacity.children[0]=1;
    require(!fs15_scene_state_update(&capacity.arena,0,FS15_SCENE_PROPAGATE,1,0,events.data(),events.size(),&used)&&std::memcmp(before.data(),capacity.nodes.data(),sizeof(before))==0,"External child pointer rejected atomically");
    Fixture malformed;malformed.nodes[0].observers_end=malformed.nodes[0].observers_begin+1;before=malformed.nodes;
    require(!fs15_scene_state_update(&malformed.arena,0,FS15_SCENE_RESCAN_OBSERVERS,1,0,events.data(),events.size(),&used)&&
        std::memcmp(before.data(),malformed.nodes.data(),sizeof(before))==0,"Misaligned observer span rejected atomically");
    Fixture cycle;cycle.nodes[0].parent=reinterpret_cast<uintptr_t>(&cycle.nodes[1]);
    cycle.nodes[1].children_begin=reinterpret_cast<uintptr_t>(&cycle.children[1]);cycle.nodes[1].children_end=cycle.nodes[1].children_begin+8;
    cycle.children[1]=reinterpret_cast<uintptr_t>(&cycle.nodes[0]);
    require(!fs15_scene_state_update(&cycle.arena,0,FS15_SCENE_MARK_INHERITED,0,0,events.data(),events.size(),&used),"Cyclic child graph rejected");

    Fixture physical;physical.props[0].flags=0x10;physical.props[1].flags=0x88;
    const auto old_world=physical.nodes[1].world[12];
    require(fs15_scene_state_update(&physical.arena,0,FS15_SCENE_PROPAGATE,1,1,events.data(),events.size(),&used),"World-preserving child path");
    require(physical.nodes[1].world[12]==old_world&&physical.nodes[1].local[12]==old_world-physical.nodes[0].world[12]&&
        (physical.nodes[1].flags&0x80)&&!(physical.nodes[1].flags&0x40),"Child stays in world, local TRS becomes stale, direct propagation skipped");
    float matrix[16]{};matrix[0]=matrix[5]=matrix[10]=matrix[15]=1;matrix[12]=23;
    const auto old_flags=physical.nodes[0].flags;
    require(fs15_scene_set_local_matrix(&physical.arena,0,matrix,16)&&physical.nodes[0].local[12]==23&&physical.nodes[0].flags==old_flags,"Plain matrix setter does not mark TRS fields");
    matrix[0]=std::numeric_limits<float>::infinity();before=physical.nodes;
    require(!fs15_scene_set_local_matrix(&physical.arena,1,matrix,16)&&std::memcmp(before.data(),physical.nodes.data(),sizeof(before))==0,"Nonfinite matrix rejected atomically");
    require(!fs15_scene_state_update(&physical.arena,3,0,1,0,events.data(),events.size(),&used),"Out-of-range node");
    require(!fs15_scene_state_update(&physical.arena,0,9,1,0,events.data(),events.size(),&used),"Unknown state operation");
    Fixture singular;singular.props[0].flags=0x10;singular.props[1].flags=0x88;singular.nodes[0].local[0]=0;
    before=singular.nodes;
    require(!fs15_scene_state_update(&singular.arena,0,FS15_SCENE_PROPAGATE,1,1,events.data(),events.size(),&used)&&
        std::memcmp(before.data(),singular.nodes.data(),sizeof(before))==0,"Singular world-preserving parent rejected atomically");
    std::vector<FS15SceneStateRecord> deep(257);
    std::vector<uintptr_t> links(256);std::vector<uint8_t> kinds(257);
    for(size_t i=0;i<deep.size();++i) {
        if(i)deep[i].parent=reinterpret_cast<uintptr_t>(&deep[i-1]);
        if(i<links.size()) {
            links[i]=reinterpret_cast<uintptr_t>(&deep[i+1]);
            deep[i].children_begin=reinterpret_cast<uintptr_t>(&links[i]);
            deep[i].children_end=deep[i].children_begin+sizeof(uintptr_t);
        }
    }
    FS15SceneStateArena deep_arena={deep.data(),deep.size()*sizeof(deep[0]),sizeof(deep[0]),
        links.data(),links.size(),nullptr,0,nullptr,0,kinds.data(),kinds.size(),0};
    require(!fs15_scene_state_update(&deep_arena,0,FS15_SCENE_MARK_INHERITED,0,0,events.data(),events.size(),&used),"Excessive descendant depth rejected");
    for(const auto& node:deep)require(node.flags==0,"Depth guard preserves complete tree");
}
}
int main() {
    try {run();std::cout<<"Recovered scene state and authored bounded-view guards passed. No allocator or engine startup tested.\n";return 0;}
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
