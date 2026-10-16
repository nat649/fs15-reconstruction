// Asset-free callbacks-to-animation integration and failure ownership contracts.
#include "scene_group_fixture.h"
#include <iostream>
namespace {
using fs15_group_test::Fixture;
using fs15_group_test::require;
using fs15_animation_test::Owner;
void run() {
    Fixture f;FS15SceneGroupResult result{};
    require(f.begin({{"lodDistance","0  10 100"},{"name","animated"},{"nodeId","42"},{"translation","5 0 0"}},&result)&&result.kind==2&&result.bytes==0x188,"LOD before name still constructs from first attribute");
    auto* target=reinterpret_cast<FS15SceneLodRecord*>(result.node);
    require(target->distances[0]==0&&target->distances[1]==10&&target->distances[2]==10&&target->distances[3]==100,"LOD vector retains repeated-space behavior");
    require(f.begin({{"name","child"},{"name","ignored"},{"nodeId","43"},{"translation","2 0 0"}},&result)&&result.kind==1,"Create and attach plain child");
    auto* child=result.node;f.end();f.end();require(f.current==f.node&&target->node.parent==reinterpret_cast<uintptr_t>(f.node)&&child->parent==reinterpret_cast<uintptr_t>(target),"Native parent stack and owned hierarchy");
    require(std::strcmp(reinterpret_cast<const char*>(child->name),"child")==0&&f.ids.at(42)==&target->node,"First name and source ID registration");
    auto* current=f.current;const auto blocks=f.blocks.size();
    require(f.begin({{"name",nullptr},{"name","ignored"}},&result)&&result.node==nullptr&&f.current==current&&f.blocks.size()==blocks,"Null first duplicate name ignores begin without allocation");
    require(f.begin({{"Name","wrong-case"}},&result)&&result.node==nullptr,"Case-sensitive missing name is ignored");
    require(f.begin({{"name","empty-lod"},{"lodDistance",""}},&result)&&result.kind==2,"Empty nonnull lodDistance still chooses LOD");
    auto* empty=reinterpret_cast<FS15SceneLodRecord*>(result.node);require(empty->distances[0]==0&&empty->distances[1]==100,"Empty LOD uses constructor defaults");f.end();
    require(f.begin({{"name","null-lod"},{"lodDistance",nullptr},{"lodDistance","0 100"}},&result)&&result.kind==1,"Null first lodDistance keeps plain TRS");f.end();
    f.flush();require(f.lod.manager.count==2,"Callback-created LOD groups reach registry");
    // Recovered animation callbacks resolve the source ID emitted by the scene
    // callback, bind the actual loaded LOD, and update its owned subtree.
    auto services=f.animation_xml();void* xml=fs15_animation_xml_create(&services);require(xml!=nullptr,"Create animation XML context");int diagnostic=0;
    auto event=[&](unsigned op,std::initializer_list<std::pair<const char*,const char*>> pairs={}) {
        std::vector<FS15AnimationXmlAttribute> attrs;for(const auto& p:pairs)attrs.push_back({p.first,std::strlen(p.first)+1,p.second,p.second?std::strlen(p.second)+1:0});
        return fs15_animation_xml_event(xml,op,attrs.data(),attrs.size(),&diagnostic)!=0;
    };
    require(event(0,{{"name","loaded-scene"}})&&event(2,{{"name","move"},{"duration","10"}})&&event(4,{{"nodeId","42"}})&&
        event(6,{{"time","0"},{"translation","5 0 0"},{"iptin","linear"},{"iptout","linear"}})&&
        event(6,{{"time","10"},{"translation","15 0 0"},{"iptin","linear"},{"iptout","linear"}})&&event(5)&&event(3)&&event(1),"Bind animation to source ID from loaded group");
    require(f.last&&f.last->references==1&&target->node.shared_resource==reinterpret_cast<uintptr_t>(f.last)+0x28,"Loaded node owns animation references");
    const uint32_t flags=3;std::memcpy(f.last->state,&flags,4);f.last->state[8]=0;auto playback=f.animation();
    require(fs15_animation_set_time(f.last,0,5)&&fs15_animation_update(f.last,0,&playback),"Animate callback-created LOD through recovered setters");f.flush();
    require(target->node.local[12]==10&&f.lod.manager.entries[target->index].position[0]==10,"Animation propagates to LOD world registry");
    float position[3];require(Fixture::world(&f,child,position)&&position[0]==12,"Animated parent reaches child world transform");
    require(fs15_animation_xml_destroy(xml),"Close animation transport");f.close();

    Fixture ignored;require(ignored.begin({{"name","parent"}}),"Open parent");current=ignored.current;
    require(ignored.begin({{"name",nullptr}},&result)&&ignored.current==current,"Ignored native begin retains current");ignored.end();
    require(ignored.current==ignored.node,"End callback still pops existing current after an ignored begin");ignored.close();
    Fixture failure;failure.fail_link=true;current=failure.current;
    require(!failure.begin({{"name","retained"},{"nodeId","99"}},&result)&&result.node&&failure.current==current&&!result.node->parent&&failure.ids.at(99)==result.node,"Failed attachment retains registered owned prefix");failure.fail_link=false;failure.close();
    Fixture constructed_failure;constructed_failure.fail_constructed=true;
    require(!constructed_failure.begin({{"name","host-failure"}},&result)&&result.node&&constructed_failure.nodes.size()==2,"Constructed callback failure retains node for cleanup");constructed_failure.close();
    unsigned failed=0,succeeded=0;
    for(int limit=0;limit<30;++limit) {
        Fixture attempt;attempt.fail_after=limit;
        const bool ok=attempt.begin({{"name","allocation-prefix"},{"lodDistance","0 10 100"},{"dynamic","true"},{"joint","true"},{"nodeId","7"}},&result);
        if(ok)++succeeded;else ++failed;
        attempt.fail_after=-1;attempt.close();
    }
    require(failed>=8&&succeeded>0,"Allocation failures before/after construction clean owned prefixes");
    FS15SceneStateRecord unsupported{},parent{};unsupported.parent=reinterpret_cast<uintptr_t>(&parent);unsupported.type_flags=0x800;auto* cursor=&unsupported;
    require(!fs15_scene_group_end(&cursor)&&cursor==&parent,"Unrecovered shape finalization reports failure after native parent pop");
    cursor=nullptr;require(!fs15_scene_group_end(&cursor),"Guard null callback current");
    fs15_animation_test::Owner owner;FS15SceneVector values{};
    require(fs15_scene_float_vector("",1,0,&values,&owner.memory)&&(values.end-values.begin)/4==1&&*reinterpret_cast<const float*>(values.begin)==0,"Direct zero-length vector parser appends final scalar");
    const auto saved=values;require(!fs15_scene_float_vector("1 2",4,4,&values,&owner.memory)&&std::memcmp(&values,&saved,sizeof(values))==0,"Guard scan beyond first NUL");
    Owner::release(&owner,reinterpret_cast<void*>(values.begin));owner.cleanup();
}
}
int main(){try{run();std::cout<<"Scene group callback/animation checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
