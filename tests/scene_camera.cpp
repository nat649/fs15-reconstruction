#include "scene_group_fixture.h"
#include "scene_math.h"
#include <iostream>
#include <limits>
namespace {
using fs15_group_test::Fixture;
using fs15_group_test::require;
int world(void* p,FS15SceneStateRecord* n,float* out){float position[3];if(!Fixture::world(p,n,position))return 0;std::memcpy(out,n->world,64);return 1;}
void run() {
    Fixture f;FS15SceneCameraServices motion{&f,world,Fixture::refresh,Fixture::rebuild};FS15SceneGroupResult result{};
    auto begin=[&](std::initializer_list<std::pair<const char*,const char*>> pairs){std::vector<FS15SceneXmlAttribute> a;for(const auto& p:pairs)a.push_back({p.first,std::strlen(p.first)+1,p.second,p.second?std::strlen(p.second)+1:0});return fs15_scene_camera_begin(&f.current,a.data(),a.size(),&f.group_services,&result);};
    require(f.begin({{"name","parent"},{"translation","5 0 0"}}),"Create camera parent");
    require(begin({{"fov","45"},{"name","camera"},{"nearClip","0.2"},{"orthographic","TrUe"},{"orthographicHeight","20"},{"nodeId","12"},{"dynamic","true"},{"joint","true"}}),"Load camera with first-attribute FOV");
    auto* n=reinterpret_cast<FS15SceneCameraRecord*>(result.node);
    require(result.kind==1&&result.bytes==0x190&&n->node.class_tag==0x16&&n->node.type_flags==0x8072&&n->fov==45&&n->near_clip==0.2f&&n->far_clip==1000&&n->orthographic&&n->orthographic_height==20,"Native camera identity, parameters and defaults");
    require(*reinterpret_cast<uintptr_t*>(n->node.properties)==0x1404c0ca8&&f.ids.at(12)==&n->node,"Camera registers source ID without physics/joint creation");
    require(f.begin({{"name","lod-child"},{"lodDistance","0 100"}}),"Attach LOD beneath Camera");auto* lod=reinterpret_cast<FS15SceneLodRecord*>(f.current);f.end();
    require(f.begin({{"name","sibling"}}),"Attach camera child");f.end();f.end();
    float pose[16]{};pose[0]=pose[5]=pose[10]=pose[15]=1;pose[12]=10;pose[13]=2;
    require(fs15_scene_camera_pose(n,pose,16,&motion)&&n->node.translation[0]==5&&n->node.translation[1]==2,"World pose converts through transformed parent");
    f.flush();require(f.lod.manager.entries[lod->index].position[0]==10,"Camera motion reaches descendant LOD manager");
    const float target[3]={1,2,3};require(fs15_scene_camera_follow(n,target,3,10,&motion),"Follow native Z axis");float output[16];require(world(&f,&n->node,output)&&output[12]==1&&output[13]==2&&output[14]==13,"Follow reaches world target plus distance");
    std::array<uint8_t,0x60> frame{};const float parameters[]={1,2,3,4,0.5f,2};for(unsigned i=0;i<6;++i)std::memcpy(frame.data()+i*16,parameters+i,4);
    require(fs15_scene_camera_script_follow(n,frame.data(),frame.size(),&motion),"Script follow frame");
    require(fs15_scene_camera_script_follow(nullptr,nullptr,0,nullptr),"Null script camera is no-op");
    auto saved=*n;pose[0]=std::numeric_limits<float>::infinity();require(!fs15_scene_camera_pose(n,pose,16,&motion)&&std::memcmp(n,&saved,sizeof(saved))==0,"Reject nonfinite pose without camera mutation");
    float vector[3]={3,4,0};require(fs15_scene_normalize3(vector,3)&&std::fabs(vector[0]-0.6f)<1e-6f,"Recovered vector normalization");float zero[3]{};require(fs15_scene_normalize3(zero,3)&&zero[0]==0,"Native zero-vector normalization");
    f.end();require(f.current==f.node,"Camera/group end stack");f.close();
    Fixture script;const char name[]="script";const char* name_pointer=name;std::memcpy(frame.data(),&name_pointer,sizeof(name_pointer));
    const float projection[]={60,0.1f,2000};for(unsigned i=0;i<3;++i)std::memcpy(frame.data()+(i+1)*16,projection+i,4);
    FS15SceneCameraRecord* created=nullptr;require(fs15_scene_camera_script_create(frame.data(),frame.size(),sizeof(name),&script.memory,&created)&&created->fov==60&&!created->orthographic&&created->orthographic_height==1,"Create script camera");
    require(fs15_scene_camera_destruct(created,sizeof(*created),1,&script.memory),"Release script camera");script.close();
    for(int limit=0;limit<16;++limit){Fixture failure;failure.fail_after=limit;FS15SceneXmlAttribute attrs[]={{"name",5,"failure",8},{"nodeId",7,"42",3}};fs15_scene_camera_begin(&failure.current,attrs,2,&failure.group_services,&result);failure.fail_after=-1;failure.close();}
}
}
int main(){try{run();std::cout<<"Scene Camera checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
