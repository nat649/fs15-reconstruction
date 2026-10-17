#include "scene_visibility.h"
#include "scene_group_fixture.h"
#include <iostream>
#include <limits>
namespace {
using fs15_group_test::Fixture;using fs15_group_test::require;
void run(){
    Fixture f;FS15SceneFrustum volume{};volume.vtable=0x1404c7cc0;const float identity[]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    require(fs15_scene_frustum_matrix(&volume,identity,16)&&volume.planes[0][0]==-1&&volume.planes[5][2]==1,"Extract native identity clip planes");
    int accepted=0;float boundary[]={1,0,0,0},outside[]={2,0,0,1},center[]={0,0,0},extent[]={1,1,1};
    require(fs15_scene_frustum_point(&volume,center,&accepted)&&accepted,"Interior point is visible");
    require(fs15_scene_frustum_point(&volume,boundary,&accepted)&&!accepted,"Native point-on-plane boundary rejects");
    require(fs15_scene_frustum_sphere(&volume,outside,&accepted)&&!accepted,"Exterior sphere tangency rejects");
    require(fs15_scene_frustum_box(&volume,outside,extent,&accepted)&&!accepted,"Exterior box tangency rejects");
    FS15SceneClipPlanes empty{0x1404c7da8,nullptr,0,0};require(fs15_scene_clip_point(&empty,boundary,&accepted)&&accepted,"Empty dynamic volume accepts");
    FS15SceneSpatialGrid grid{};const float low[]={-4,-4,-4},high[]={4,4,4};require(fs15_scene_spatial_init(&grid,low,high,4,&f.memory),"Initialize visibility grid");
    uint32_t keys[3]{};FS15SceneSpatialSphere s{};for(float& value:s.center)value=0.5f;s.radius=0.25f;
    require(fs15_scene_spatial_add(&grid,keys,&s,1000,0,0xffffffff,&f.memory),"Add visible sphere");s.center[0]=3;
    require(fs15_scene_spatial_add(&grid,keys+1,&s,1000,0,0xffffffff,&f.memory),"Add clipped sphere");
    require(fs15_scene_spatial_add(&grid,keys+2,nullptr,0,0,0xffffffff,&f.memory),"Add native unbounded sentinel");
    uint32_t total=0;uint32_t* output[4]={nullptr,nullptr,reinterpret_cast<uint32_t*>(uintptr_t(0x1234)),nullptr};
    require(fs15_scene_spatial_volume_query(&grid,&volume,center,1,nullptr,output,1,&total)&&total==2&&output[0]==keys+2&&output[2]==reinterpret_cast<uint32_t*>(uintptr_t(0x1234)),"Depth-first output truncates writes while retaining total match count");
    require(fs15_scene_spatial_volume_query(&grid,&volume,center,1,nullptr,nullptr,0,&total)&&total==2,"Count-only volume query");
    const float on_sphere[]={0.75f,0.5f,0.5f};require(fs15_scene_spatial_point_query(&grid,on_sphere,nullptr,output,4,&total)&&total==2,"Point query includes sphere boundary");
    FS15SceneLodEntry lod_entry{};lod_entry.levels=2;lod_entry.distance_squared[1]=100;FS15SceneLodManager lod{};lod.entries=&lod_entry;lod.count=lod.capacity=1;lod.stamp=5;lod.distance_factor_squared=1;
    FS15SceneSpatialEntry e{};e.radius=1;e.clip[0]=1000;e.lod_token=0x40000000;
    require(fs15_scene_spatial_entry_visible(&volume,center,1,&e,&lod,&accepted)&&!accepted&&lod_entry.stamp==5,"LOD cache refresh precedes rejected branch");
    e.lod_token=0;e.clip[0]=-10;require(fs15_scene_spatial_entry_visible(&volume,center,1,&e,&lod,&accepted)&&!accepted,"Native first clip inequality rejects");
    auto snapshot=volume;float invalid[16]{};invalid[3]=std::numeric_limits<float>::infinity();require(!fs15_scene_frustum_matrix(&volume,invalid,16)&&!std::memcmp(&volume,&snapshot,sizeof(volume)),"Nonfinite extraction rejects without output commit");
    const uint32_t old_skip=grid.cells[0].skip;grid.cells[0].center_x=100;grid.cells[0].skip=0;require(!fs15_scene_spatial_volume_query(&grid,&volume,center,1,nullptr,output,4,&total),"Reject backward or cyclic subtree skip");grid.cells[0].skip=old_skip;
    require(fs15_scene_spatial_release(&grid,&f.memory),"Release visibility allocations");f.close();
}
}
int main(){try{run();std::cout<<"Scene visibility checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
