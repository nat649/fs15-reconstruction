// Independent decimal quirks, XML resource-to-pose flow and failure ownership.
#include "animation_xml.h"
#include "animation_fixture.h"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include <vector>
namespace {
using fs15_animation_test::require;
using fs15_animation_test::Owner;
using fs15_animation_test::PoseHost;
int duration(void*,const char* text,float* out){*out=static_cast<float>(std::strtod(text,nullptr));return std::isfinite(*out);}
int node_id(void*,const char* text,uint32_t* out){*out=static_cast<uint32_t>(std::strtol(text,nullptr,10));return 1;}
struct Xml {
    Owner& owner;void* state=nullptr;int diagnostic=0;
    explicit Xml(Owner& o):owner(o){FS15AnimationXmlServices services{o.services(),duration,node_id};state=fs15_animation_xml_create(&services);}
    bool event(unsigned op,std::initializer_list<std::pair<const char*,const char*>> values={}) {
        std::vector<FS15AnimationXmlAttribute> attributes;
        for(const auto& value:values)attributes.push_back({value.first,std::strlen(value.first)+1,value.second,value.second?std::strlen(value.second)+1:0});
        return fs15_animation_xml_event(state,op,attributes.data(),attributes.size(),&diagnostic)!=0;
    }
    FS15AnimationXmlView view()const{FS15AnimationXmlView value{};require(fs15_animation_xml_view(state,&value),"Inspect XML state");return value;}
    void close(){if(state){require(fs15_animation_xml_destroy(state),"Destroy XML pending resources and rejected keys");state=nullptr;}}
    bool load() {
        return state&&event(0,{{"name","xml-set"}})&&event(2,{{"name","animation"},{"duration","10"}})&&event(4,{{"nodeId","101"}})&&
            event(6,{{"time","0"},{"translation","1 2 3"},{"visibility","true"},{"iptin","linear"},{"iptout","linear"}})&&
            event(6,{{"time","10"},{"translation","11 12 13"},{"visibility","FALSE"},{"iptin","linear"},{"iptout","linear"}})&&
            event(5)&&event(3)&&event(1);
    }
};
void run() {
    float value=99;size_t consumed=99;
    require(fs15_parse_scene_float("  -12.5e-2tail",15,&value,&consumed)&&value==-0.125f&&consumed==10,"Native decimal prefix and cursor");
    require(fs15_parse_scene_float("+12",4,&value,&consumed)&&value==0&&consumed==0,"Native grammar does not accept leading plus");
    require(fs15_parse_scene_float("1E3",4,&value,&consumed)&&value==1&&consumed==1,"Native exponent is lowercase-only");
    require(fs15_parse_scene_float("1e-45",6,&value,&consumed)&&value==0,"Original float exponent factor overflows before division");
    require(fs15_parse_scene_float("-0",3,&value,&consumed)&&value==0&&std::signbit(value),"Native negative zero");
    float vector[3]={8,9,10};size_t written=99;
    require(fs15_parse_scene_float_vector("1  2 3",7,vector,3,&written)&&written==3&&vector[0]==1&&vector[1]==2&&vector[2]==2,"Repeated spaces retain native duplicate-number behavior");
    value=55;consumed=77;require(!fs15_parse_scene_float("1e999999",9,&value,&consumed)&&value==55&&consumed==77,"Exponent guard preserves outputs");
    Owner owner;Xml xml(owner);require(xml.load(),"Load decoded XML events");
    require(owner.last&&owner.last->references==1&&owner.last->data->flags==9&&xml.view().set==nullptr,"XML ownership transferred to node references");
    uint32_t flags=3;std::memcpy(&owner.last->state[0],&flags,4);owner.last->state[8]=0;
    PoseHost host(owner.node);FS15AnimationServices services{&host,PoseHost::refresh,PoseHost::matrix,PoseHost::propagate};
    require(fs15_animation_set_time(owner.last,0,5)&&fs15_animation_update(owner.last,0,&services)&&owner.node->local[12]==6&&owner.node->local[13]==7&&owner.node->local[14]==8,"XML resources reach recovered playback and scene updates");
    require(fs15_animation_set_time(owner.last,0,10)&&fs15_animation_update(owner.last,0,&services)&&!(owner.node->flags&1u),"Case-insensitive false visibility");xml.close();owner.cleanup();

    Owner orphan;Xml rejected(orphan);
    require(rejected.event(0,{{"name","pending"}})&&rejected.event(2,{{"name","clip"},{"duration","10"}})&&rejected.event(4,{{"nodeId","101"}}),"Open pending XML channel");
    require(rejected.event(6,{{"translation","0 0 0"}})&&rejected.view().channel->keys.begin==rejected.view().channel->keys.end,"Missing time is a native no-op");
    require(rejected.event(6,{{"time","0"},{"translation","0 0 0"}}),"Add first XML key");
    require(rejected.event(6,{{"time","0"},{"translation","1 1 1"}})&&rejected.diagnostic==1&&rejected.view().rejected_keys==1,"Rejected time retains native orphan ownership");
    require(rejected.event(6,{{"time","1"},{"rotation","0 0 0"}})&&rejected.diagnostic==2&&rejected.view().rejected_keys==2,"Changed key controls retain orphan ownership");
    const auto before=rejected.view().channel->keys.end;
    require(!rejected.event(6,{{"time","2"},{"translation","1 2"}})&&rejected.view().channel->keys.end==before,"Short vectors reject undefined native stack reads");
    require(!rejected.event(0,{{"name","wrong nesting"}})&&rejected.view().channel!=nullptr,"Invalid nesting retains pending resources for cleanup");rejected.close();orphan.cleanup();

    Owner rotation;Xml angle(rotation);
    require(angle.event(0,{{"name","rotation"}})&&angle.event(2,{{"name","clip"},{"duration","10"}})&&angle.event(4,{{"nodeId","101"}})&&angle.event(6,{{"time","0"},{"rotation","0 180 0"}}),"Decode degrees into packed rotation");
    const auto* key=*reinterpret_cast<const FS15AnimationKey* const*>(angle.view().channel->keys.begin);int16_t packed[4];std::memcpy(packed,reinterpret_cast<const uint8_t*>(key)+8+key->rotation_offset,8);
    require(std::abs(int(packed[1]))==32767&&(key->flags&0x8122u)==0x8122u,"Default easing and default-visible flag are preserved");angle.close();rotation.cleanup();
    unsigned failed=0,succeeded=0;
    for(int failure=0;failure<36;++failure) {
        Owner limited;limited.fail_after=failure;Xml attempt(limited);const bool ok=attempt.load();limited.fail_after=-1;
        if(ok)++succeeded;else ++failed;attempt.close();limited.cleanup();
    }
    require(failed>12&&succeeded>0,"Exercise XML allocation failures through partial binding");
}
}
int main(){try{run();std::cout<<"Recovered XML numeric/resource-to-pose and failure guards passed. Full engine not tested.\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
