#include "scene_spatial.h"
#include "scene_graph.h"
#include "scene_group_fixture.h"
#include <iostream>
#include <limits>
namespace {
using fs15_group_test::Fixture;using fs15_group_test::require;
int world(void*,FS15SceneStateRecord* n,float* out){if(!fs15_scene_update_world(n,sizeof(*n),sizeof(*n),0))return 0;std::memcpy(out,n->world,64);return 1;}
void run(){
    Fixture f;FS15SceneSpatialGrid g{};const float low[]={-1024,-500,-1024},high[]={1024,500,1024};
    require(fs15_scene_spatial_init(&g,low,high,16,&f.memory)&&g.count==341&&g.levels==5,"Initialize recovered spatial grid");
    FS15SceneSpatialSphere s{};s.center[0]=-900;s.center[2]=-900;s.radius=1;uint32_t a=0,b=0;
    require(fs15_scene_spatial_add(&g,&a,&s,10,100,7,&f.memory)&&fs15_scene_spatial_add(&g,&b,&s,20,200,8,&f.memory),"Register bounded spatial records");
    require((a>>17)==0&&(b>>17)==1,"Packed per-cell slots");
    require(fs15_scene_spatial_remove(&g,&a)&&(b>>17)==0,"Swap removal updates moved owner token");
    require(fs15_scene_spatial_move(&g,&b,nullptr,0,0,0xffffffff,&f.memory)&&b==0&&g.cells[0].entries[0].radius==std::numeric_limits<float>::max(),"Infinite object moves to root sentinel");
    require(fs15_scene_spatial_remove(&g,&b),"Remove root sentinel");
    std::vector<void*> retained;for(uint32_t i=0;i<g.count;++i)if(g.cells[i].entries)retained.push_back(g.cells[i].entries);
    require(fs15_scene_spatial_init(&g,low,high,4,&f.memory)&&g.count==21,"Reinitialize grid");
    require(fs15_scene_spatial_release(&g,&f.memory),"Release current grid");for(void* p:retained)f.memory.deallocate(f.memory.user,p);
    const float bad[]={0,0,0};require(!fs15_scene_spatial_init(&g,bad,bad,16,&f.memory),"Reject degenerate world extent");
    f.fail_after=0;require(!fs15_scene_spatial_init(&g,low,high,16,&f.memory)&&!g.cells,"Allocation failure leaves an empty grid unchanged");f.fail_after=-1;
    require(fs15_scene_spatial_init(&g,low,high,16,&f.memory),"Restore grid for active Light");
    auto* light=static_cast<FS15SceneLightRecord*>(f.memory.allocate(f.memory.user,sizeof(FS15SceneLightRecord)));FS15SceneLightParameters p{};p.type=2;p.range=20;p.cone_angle=60;p.drop_off=4;
    FS15SceneLightContext pending{};FS15SceneLightUpdateServices update{nullptr,&f.memory,world};
    require(fs15_scene_light_construct(light,sizeof(*light),"active",7,&p,&pending,&f.memory),"Construct standalone Light");light->node.flags|=2;
    require(fs15_scene_light_update(light,&g,&update)&&(light->node.flags&0x200)&&light->spatial.radius==20,"Register visible Light after recovered bounds update");
    require(std::fabs(light->spatial.center[2]+10.055083f)<1e-5f,"Spot bound center matches native cone fixture");
    light->node.flags|=0x404;require(fs15_scene_light_update(light,&g,&update)&&g.cells[0].entries[0].radius==std::numeric_limits<float>::max(),"Unbounded Light uses native root-cell sentinel");
    light->node.flags=(light->node.flags|0x400)&~2u;require(fs15_scene_light_update(light,&g,&update)&&!(light->node.flags&0x200)&&!g.cells[0].count,"Hidden Light releases spatial ownership");
    light->node.flags|=0x502;require(fs15_scene_light_update(light,&g,&update)&&(light->node.flags&0x200),"Visible Light re-registers");
    require(!fs15_scene_light_destruct(light,sizeof(*light),1,&pending,&f.memory),"Legacy pending-only destructor rejects active spatial ownership");
    require(fs15_scene_light_spatial_destruct(light,sizeof(*light),1,&g,&pending,&f.memory),"Destroy Light while registered in spatial grid");
    require(fs15_scene_spatial_release(&g,&f.memory),"Release active Light grid");f.close();
}
}
int main(){try{run();std::cout<<"Scene spatial checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
