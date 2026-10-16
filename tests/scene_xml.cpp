// Asset-free common I3D attribute flow, native quirks and ownership boundaries.
#include "scene_xml.h"
#include "animation_fixture.h"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <vector>
namespace {
using fs15_animation_test::Owner;
using fs15_animation_test::require;
struct Scene {
    Owner owner;std::map<uint32_t,FS15SceneStateRecord*> ids;
    FS15SceneXmlServices services{this,&owner.memory,rebuild,propagate,integer,registered};
    static FS15SceneStateArena arena(FS15SceneStateRecord* node,const uint8_t* kind) {
        return {node,sizeof(*node),sizeof(*node),nullptr,0,nullptr,0,reinterpret_cast<FS15ScenePropertyView*>(node->properties),1,kind,1,SIZE_MAX};
    }
    static int rebuild(void*,FS15SceneStateRecord* node) {
        const uint8_t kind=1;const auto view=arena(node,&kind);size_t count=0;int warning=0;
        return fs15_scene_trs_update(&view,0,FS15_SCENE_TRS_REBUILD,nullptr,0,nullptr,0,&count,&warning);
    }
    static int propagate(void*,FS15SceneStateRecord* node,uint32_t mask,int32_t origin) {
        const uint8_t kind=1;const auto view=arena(node,&kind);size_t count=0;
        return fs15_scene_state_update(&view,0,FS15_SCENE_PROPAGATE,mask,origin,nullptr,0,&count);
    }
    static int integer(void*,const char* text,uint32_t* out){*out=static_cast<uint32_t>(std::strtol(text,nullptr,10));return 1;}
    static int registered(void* p,uint32_t id,FS15SceneStateRecord* node){static_cast<Scene*>(p)->ids.emplace(id,node);return 1;}
    bool apply(std::initializer_list<std::pair<const char*,const char*>> values,int physics=1,int registration=1,int joint=1) {
        std::vector<FS15SceneXmlAttribute> attributes;
        for(const auto& value:values)attributes.push_back({value.first,std::strlen(value.first)+1,value.second,value.second?std::strlen(value.second)+1:0});
        return fs15_scene_xml_apply(owner.node,attributes.data(),attributes.size(),physics,registration,joint,"",1,&services)!=0;
    }
};
void run() {
    Scene scene;auto* node=scene.owner.node;
    require(scene.apply({{"name","field"},{"name","ignored"},{"translation","1  2 3"},{"rotation","0 90 0"},{"scale","2 3 4"},
        {"visibility","FALSE"},{"oc","true"},{"clipDistance","200"},{"minClipDistance","5"},{"objectMask","123"},{"nodeId","41"},{"refId","42"}}),"Apply common node attributes");
    require(std::strcmp(reinterpret_cast<const char*>(node->name),"field")==0&&node->translation[0]==1&&node->translation[1]==2&&node->translation[2]==3,"First name and scalar-cursor TRS grammar");
    require(node->local[12]==1&&node->local[13]==2&&node->local[14]==3&&std::abs(node->rotation[1]-1.57079637f)<1e-7f,"Recovered cache rebuild and degree conversion");
    require(!(node->flags&1u)&&(node->flags&0x20u)&&node->local_clip[0]==200&&node->local_clip[1]==5&&node->local_masks[0]==123,"Visibility, occlusion, clipping and masks");
    require(scene.ids.size()==1&&scene.ids.at(41)==node,"Node ID precedes ref ID");
    require(scene.apply({{"static","true"},{"dynamic","true"},{"compound","true"},{"compoundChild","true"},{"collision","false"},{"trigger","true"},
        {"collisionMask","-1"},{"density","-2"},{"solverIterationCount","8"},{"restitution","0.3"}}),"Load pending rigid body attributes");
    auto* body=reinterpret_cast<FS15RigidBodyProperty*>(node->properties);
    require(body->flags==0x124c&&body->collision_mask==0xffffffff&&body->density==0.0001f&&body->solver_iterations==8,"Static/compound precedence, collision toggle, trigger and density minimum");
    require(body->changes==0x20c1a48&&body->owner==node&&body->manager==0&&body->actor==0,"Pending property dirty bits and detached backend");
    require(node->translation[0]==0&&node->scale[0]==1&&node->flags&1u,"Absent TRS/visibility attributes restore native defaults");
    require(!fs15_scene_destruct(node,sizeof(*node),FS15_OBJECT_TRS,0,&scene.owner.memory),"Native trigger teardown is outside supported lifetime scope");
    require(fs15_scene_property_flags(body,0x1000,0,nullptr),"Clear unactivated trigger fixture");
    // A new joint replaces the previous body in native order, preserving ownership.
    require(scene.apply({{"dynamic","true"},{"joint","true"},{"projection","true"},{"projDistance","2"},{"projAngle","3"},
        {"driveForceLimit","27"},{"driveSpring","4"},{"drivePos","true"},{"breakableJoint","true"},{"jointBreakForce","99"},
        {"rotMinX","-10"},{"rotMaxZ","20"},{"transMinY","-3"}}),"Replace pending body with joint");
    auto* joint=reinterpret_cast<FS15JointProperty*>(node->properties);
    require(joint->flags==0xc54&&joint->projection_distance==2&&joint->projection_angle==3,"Joint flags and projection");
    require(joint->drive_force==27&&joint->drive_spring==4&&joint->break_force==99,"Registration switch enables drive values without axis switches");
    require(joint->rotation_limited[0]&&joint->rotation_limited[2]&&joint->translation_limited[1]&&joint->rotation_min[0]==-10&&joint->rotation_max[2]==20,"Joint limit presence and unconverted angle values");
    joint->manager=1;const auto saved=*joint;
    require(!fs15_scene_property_destruct_detached(joint,0,&scene.owner.memory)&&std::memcmp(joint,&saved,sizeof(saved))==0,"Active backend rejects before mutation");joint->manager=0;
    scene.owner.cleanup();
    Scene no_registration;
    require(no_registration.apply({{"joint","true"},{"driveForceLimit","27"},{"projection","false"},{"projDistance","9"}},0,0,1),"Joint with registration disabled");
    joint=reinterpret_cast<FS15JointProperty*>(no_registration.owner.node->properties);
    require(joint->drive_force==100000&&joint->projection_distance==0.01f,"Disabled projection and drive values remain at constructor defaults");
    require(no_registration.apply({{"joint","true"},{"xAxisDrive","true"},{"driveForceLimit","27"}},0,0,1),"Axis enables drive parsing independently");
    joint=reinterpret_cast<FS15JointProperty*>(no_registration.owner.node->properties);require(joint->drive_force==27&&(joint->flags&0x80u),"Axis drive metadata");
    no_registration.owner.cleanup();
    unsigned failed=0,succeeded=0;
    for(int limit=0;limit<12;++limit) {
        Scene attempt;attempt.owner.fail_after=limit;
        const bool ok=attempt.apply({{"name","allocation"},{"dynamic","true"},{"joint","true"},{"nodeId","17"}});
        if(ok)++succeeded;else ++failed;
        attempt.owner.fail_after=-1;attempt.owner.cleanup();
    }
    require(failed>=3&&succeeded>0,"Allocation failures leave owned prefixes destructible");
    Scene invalid;
    require(!invalid.apply({{"translation","1 2"}}),"Short triple rejects undefined native reads");
    require(!invalid.apply({{"scale","1e99999 1 1"}}),"Exponent work guard");
    const FS15SceneXmlAttribute attrs[]={{"same",5,nullptr,0},{"same",5,"second",7}};const char* result="unchanged";
    require(fs15_scene_xml_find(attrs,2,"same",5,&result)&&result==nullptr,"First null duplicate lookup is retained");
    invalid.owner.cleanup();
}
}
int main(){try{run();std::cout<<"Common scene XML and detached property checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
