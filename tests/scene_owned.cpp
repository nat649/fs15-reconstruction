// Asset-free owned graph integration and authored transactional failure guards.
#include "scene_owned.h"
#include "scene_xml.h"
#include "animation_fixture.h"
#include <array>
#include <cstring>
#include <iostream>
#include <vector>
namespace {
using fs15_animation_test::Owner;
using fs15_animation_test::require;
struct Forest:Owner {
    std::vector<FS15SceneStateRecord*> nodes;
    std::vector<uint8_t> kinds;
    FS15SceneOwnedView view{};
    std::array<FS15SceneNotification,32> events{};size_t used=0;uint32_t attached=99;int warning=0;
    explicit Forest(size_t count) {
        nodes.push_back(node);memory.unlink=unlink;memory.destroy_child=destroy;
        for(size_t i=1;i<count;++i) {
            auto* n=static_cast<FS15SceneStateRecord*>(allocate(this,sizeof(FS15SceneStateRecord)));
            require(fs15_scene_construct(n,sizeof(*n),FS15_OBJECT_TRS,"node",5,0,1,&memory),"Construct separately allocated node");nodes.push_back(n);
        }
        kinds.assign(count,1);view={nodes.data(),nodes.size(),kinds.data(),0};
    }
    static int unlink(void*,void* parent,uint32_t slot,int suppress) {
        auto& p=*static_cast<FS15SceneStateRecord*>(parent);
        if(!suppress||p.vtable!=0x1404c7dd0)return 0;
        auto* children=reinterpret_cast<uintptr_t*>(p.children_begin);const size_t size=(p.children_end-p.children_begin)/8;
        if(slot>=size)return 0;
        reinterpret_cast<FS15SceneStateRecord*>(children[slot])->parent=0;
        for(size_t i=slot;i+1<size;++i)children[i]=children[i+1];
        p.children_end-=8;return 1;
    }
    static int destroy(void* user,void* child){return fs15_scene_destruct(child,sizeof(FS15SceneStateRecord),FS15_OBJECT_TRS,1,&static_cast<Owner*>(user)->memory);}
    bool link(size_t parent,size_t child,uint32_t position=UINT32_MAX) {
        return fs15_scene_owned_link(&view,parent,child,position,1,&memory,events.data(),events.size(),&used,&attached)!=0;
    }
    void close() {
        std::vector<FS15SceneStateRecord*> roots;
        for(auto* n:nodes)if(!n->parent)roots.push_back(n);
        for(auto* n:roots)require(fs15_scene_destruct(n,sizeof(*n),FS15_OBJECT_TRS,1,&memory),"Recursive owned destruction");
        node=nullptr;require(blocks.empty()&&registry.empty(),"Owned forest releases every member allocation");
    }
};
void run() {
    Owner allocator;FS15SceneVector values{};
    for(uintptr_t i=0;i<8;++i)require(fs15_scene_pointer_append(&values,&i,&allocator.memory),"Append external vector element");
    require((values.capacity-values.begin)/8==9,"Original 1,2,3,4,6,9 growth sequence");
    const auto* alias=reinterpret_cast<const uintptr_t*>(values.begin+16);
    require(fs15_scene_pointer_append(&values,alias,&allocator.memory),"Aliased append without growth");
    alias=reinterpret_cast<const uintptr_t*>(values.begin+8);
    require(fs15_scene_pointer_append(&values,alias,&allocator.memory)&&(values.capacity-values.begin)/8==13,"Aliased append across relocation");
    require(reinterpret_cast<const uintptr_t*>(values.begin)[8]==2&&reinterpret_cast<const uintptr_t*>(values.begin)[9]==1,"Relocated alias values retained");
    const auto spare=values;
    require(!fs15_scene_pointer_append(&values,reinterpret_cast<const uintptr_t*>(values.end),&allocator.memory)&&std::memcmp(&values,&spare,sizeof(values))==0,"Reject input alias into unused capacity");
    const auto saved=values;const auto before=allocator.blocks.size();allocator.fail_after=0;
    require(!fs15_scene_pointer_reserve(&values,40,&allocator.memory)&&std::memcmp(&values,&saved,sizeof(values))==0&&allocator.blocks.size()==before,"Failed vector reserve preserves ownership");
    allocator.fail_after=-1;Owner::release(&allocator,reinterpret_cast<void*>(values.begin));values={};
    require(fs15_scene_pointer_append(&values,&values.begin,&allocator.memory)&&*reinterpret_cast<const uintptr_t*>(values.begin)==values.begin,"External input reads vector metadata after relocation");
    Owner::release(&allocator,reinterpret_cast<void*>(values.begin));allocator.cleanup();

    Forest f(7);require(f.link(0,1)&&f.link(1,2)&&f.link(0,3)&&f.link(0,4)&&f.link(0,5),"Build owned multi-level hierarchy with growth");
    require((f.node->children_capacity-f.node->children_begin)/8==4&&f.node->children_end-f.node->children_begin==32,"Root vector grows to fit four children");
    require(f.link(0,4,0)&&f.attached==0,"Same-parent reorder does not require growth");
    require(reinterpret_cast<const uintptr_t*>(f.node->children_begin)[0]==reinterpret_cast<uintptr_t>(f.nodes[4]),"Native insertion order");
    require(f.link(3,2)&&f.nodes[1]->children_end==f.nodes[1]->children_begin,"Reparent owned subtree");
    require(*reinterpret_cast<const uintptr_t*>(f.nodes[1]->children_begin)==reinterpret_cast<uintptr_t>(f.nodes[2]),"Erased trailing slot retained");
    require(!f.link(2,0),"Reject cycle without altering live objects");
    const auto root=*f.node;f.fail_after=0;
    require(!f.link(0,6)&&std::memcmp(f.node,&root,sizeof(root))==0&&!f.nodes[6]->parent,"Failed graph growth preserves entire forest");f.fail_after=-1;
    require(f.link(0,6),"Retry allocation after failure");
    f.nodes[3]->translation[0]=10;f.nodes[2]->translation[0]=2;
    require(fs15_scene_owned_trs(&f.view,3,FS15_SCENE_TRS_REBUILD,nullptr,0,f.events.data(),f.events.size(),&f.used,&f.warning),"Owned subtree TRS rebuild");
    require(fs15_scene_owned_trs(&f.view,2,FS15_SCENE_TRS_REBUILD,nullptr,0,f.events.data(),f.events.size(),&f.used,&f.warning),"Owned child TRS rebuild");
    require(fs15_scene_owned_state(&f.view,0,FS15_SCENE_PROPAGATE,0x3f,0,f.events.data(),f.events.size(),&f.used)&&f.nodes[2]->flags&2u,"Inherited state across allocated objects");
    // The observer identity is opaque to portable transport; delivery is external.
    const uintptr_t observer=1234;auto* observer_vector=static_cast<uintptr_t*>(Owner::allocate(&f,8));*observer_vector=observer;
    auto* observed=f.nodes[2];observed->observers_begin=reinterpret_cast<uintptr_t>(observer_vector);observed->observers_end=observed->observers_capacity=observed->observers_begin+8;
    require(fs15_scene_owned_state(&f.view,2,FS15_SCENE_RESCAN_OBSERVERS,1,0,f.events.data(),f.events.size(),&f.used),"Observer presence rescan");
    const auto observed_before=*observed;
    require(!fs15_scene_owned_link(&f.view,5,2,UINT32_MAX,1,&f.memory,nullptr,0,&f.used,&f.attached)&&f.used>0&&std::memcmp(observed,&observed_before,sizeof(*observed))==0,"Insufficient event capacity preserves linked state and frees staging growth");
    require(f.link(5,2)&&f.used>=1&&f.events[0].node_index==2&&f.events[0].observer==observer,"Notifications retain owned node index and observer identity");
    const auto* begin_before=reinterpret_cast<const uintptr_t*>(f.nodes[5]->children_begin);const auto capacity_before=f.nodes[5]->children_capacity;
    require(fs15_scene_owned_unlink(&f.view,5,0,1,f.events.data(),f.events.size(),&f.used)&&!observed->parent,"Suppressed unlink");
    require(f.nodes[5]->children_capacity==capacity_before&&*begin_before==reinterpret_cast<uintptr_t>(observed),"Unlink retains capacity and native pointer residue");
    f.nodes[6]->parent=reinterpret_cast<uintptr_t>(f.nodes[5]);
    require(!f.link(0,2),"Reject inconsistent external membership before allocation");f.nodes[6]->parent=reinterpret_cast<uintptr_t>(f.node);
    f.close();
}
}
int main(){try{run();std::cout<<"Owned scene/vector checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
