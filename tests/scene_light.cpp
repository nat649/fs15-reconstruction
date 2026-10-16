#include "scene_group_fixture.h"
#include "scene_clone_tree.h"
#include <iostream>
namespace {
using fs15_group_test::Fixture;
using fs15_group_test::require;
int unsigned_integer(void*,const char* text,uint32_t* out){*out=static_cast<uint32_t>(std::strtoul(text,nullptr,0));return 1;}
bool begin(Fixture& f,std::initializer_list<std::pair<const char*,const char*>> pairs,FS15SceneGroupResult& result) {
    std::vector<FS15SceneXmlAttribute> attrs;for(const auto& p:pairs)attrs.push_back({p.first,std::strlen(p.first)+1,p.second,p.second?std::strlen(p.second)+1:0});
    FS15SceneLightXmlServices services{f.group_services,unsigned_integer};return fs15_scene_light_begin(&f.current,attrs.data(),attrs.size(),&services,&f.lights,&result)!=0;
}
void run() {
    Fixture f;require(f.begin({{"name","lod"},{"lodDistance","0 100"},{"translation","5 0 0"}}),"Create Light parent LOD");auto* source=f.current;FS15SceneGroupResult result{};
    require(begin(f,{{"type","SpOt"},{"name","light"},{"diffuseColor","0.5 1 0.25"},{"castShadowMap","false"},{"depthMapBias","-99"},{"coneAngle","60"},{"dropOff","9"},{"affectedLightMasks","0xff"},{"dynamic","true"},{"joint","true"}},result),"Load native Light attributes");
    auto* light=reinterpret_cast<FS15SceneLightRecord*>(result.node);
    require(result.bytes==0x200&&result.kind==3&&light->node.class_tag==0x17&&light->node.type_flags==0x2072&&light->type==2&&light->drop_off==5&&light->depth_bias==0.005f&&light->affected_masks==255,"Light identity, scalar clamp and conditional shadow parsing");
    require(light->diffuse[0]==0.5f&&std::fabs(light->outer_cos-0.8660254f)<1e-6f&&light->inner_cos==1&&*reinterpret_cast<uintptr_t*>(light->node.properties)==0x1404c0ca8,"Native cone factors/colors without physics creation");
    require(begin(f,{{"name","nested"},{"type","directional"},{"numShadowMapSplits","3"},{"castShadowMap","true"},{"decayRate","0"}},result),"Load nested Light");auto* nested=reinterpret_cast<FS15SceneLightRecord*>(result.node);f.end();f.end();f.end();f.flush();
    require(nested->shadow_splits==3&&nested->decay==1&&(f.lights.pending.end-f.lights.pending.begin)/8==2&&light->lod_token!=0xffffffff&&nested->lod_token==light->lod_token,"LOD tokens propagate through nested Light override without duplicate requests");
    require(fs15_scene_light_dirty(light,&f.lights,&f.memory)&&(f.lights.pending.end-f.lights.pending.begin)/8==2,"Already queued Light is not appended again");
    FS15SceneCloneTreeServices clone{{f.group_services.common,Fixture::refresh,Fixture::constructed},Fixture::link};FS15SceneCloneMap remap{};FS15SceneVector list{};FS15SceneGroupResult unfinished{};
    require(fs15_scene_clone_map_init(&remap,&f.memory)&&fs15_scene_clone_tree(source,&remap,&list,&clone,&result,&unfinished)&&remap.count==3,"Clone mixed LOD/nested-Light subtree through native dispatch");
    auto** copies=reinterpret_cast<FS15SceneStateRecord**>(list.begin);require(copies[1]->vtable==0x1404c1d98&&reinterpret_cast<FS15SceneLightRecord*>(copies[1])->affected_masks==0xffffffff&&reinterpret_cast<FS15SceneLightRecord*>(copies[1])->light_flags==light->light_flags,"Native Light clone retains flags but resets affected-mask constructor field");
    require(fs15_scene_clone_map_clear(&remap,1,&f.memory),"Release Light remap");f.memory.deallocate(f.memory.user,reinterpret_cast<void*>(list.begin));f.close();
    Fixture guards;require(begin(guards,{{"name","guard"}},result),"Construct queue capacity fixture");auto* n=reinterpret_cast<FS15SceneLightRecord*>(result.node);guards.end();n->node.flags=(n->node.flags|2u)&~uint32_t(0x800);guards.lights.pending.end=guards.lights.pending.begin;
    const auto snapshot=*n;auto view=guards.view();FS15SceneLodQueue insufficient{nullptr,0,0};
    require(!fs15_scene_owned_state_derived(&view,guards.indices.at(&n->node),0,5,0,nullptr,0,&guards.used,&guards.queue,&insufficient)&&insufficient.count==1&&std::memcmp(n,&snapshot,sizeof(snapshot))==0,"Queue capacity rejects without committing node/token changes");
    require(Fixture::propagate(&guards,&n->node,4,0)&&guards.enqueue(),"Retry Light propagation with sufficient queue");guards.close();
    unsigned failed=0,succeeded=0;
    for(int limit=0;limit<24;++limit){Fixture attempt;attempt.fail_after=limit;if(begin(attempt,{{"name","prefix"},{"castShadowMap","true"},{"depthMapResolution","1024"}},result))++succeeded;else ++failed;attempt.fail_after=-1;attempt.close();}
    require(failed>=6&&succeeded>0,"Light allocation failures retain cleanable prefixes");
}
}
int main(){try{run();std::cout<<"Scene Light checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
