#include "scene_view.h"
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
namespace {
void require(bool v,const char* s){if(!v)throw std::runtime_error(s);}
struct Fixture {
    FS15SceneRenderView view{};FS15SceneViewSettings settings{};FS15SceneCameraRecord camera{};
    float output[64];size_t capacity=64;unsigned maps=0,unmaps=0,worlds=0;bool null_map=false,unmap_ok=true;
    FS15SceneViewServices services{};
    Fixture(){
        for(float& v:output)v=123.5f;
        camera.node.vtable=0x1404c1078;camera.fov=60;camera.near_clip=1;camera.far_clip=9;camera.orthographic_height=2;camera.unknown180=0.4f;camera.unknown184=10;
        for(size_t i=0;i<4;++i)camera.node.world[5*i]=view.inverse[5*i]=1;
        camera.node.world[12]=10;view.settings=&settings;view.buffer=0x1234;view.camera=0x5678;
        services={this,world,map,unmap,nullptr};settings.scalar30=3;settings.scalar34=4;settings.scalar60=6;settings.scalar64=7;
    }
    static int world(void* p,FS15SceneStateRecord* c,float* out){auto& f=*static_cast<Fixture*>(p);++f.worlds;std::memcpy(out,c->world,64);return 1;}
    static float* map(void* p,uintptr_t token,size_t* count){auto& f=*static_cast<Fixture*>(p);++f.maps;require(token==0x1234&&f.view.volume==reinterpret_cast<uintptr_t>(&f.view),"Map sees correct buffer and self-volume");require(f.view.camera==0x5678,"Camera identity remains old until after unmap");*count=f.capacity;return f.null_map?nullptr:f.output;}
    static int unmap(void* p,uintptr_t){auto& f=*static_cast<Fixture*>(p);++f.unmaps;require(f.view.camera==0x5678,"Unmap precedes Camera identity update");return f.unmap_ok;}
};
void run(){
    Fixture f;require(fs15_scene_view_prepare(&f.view,sizeof(f.view),&f.camera,1,0,0,2500,&f.services),"Prepare perspective view");
    require(f.maps==1&&f.unmaps==1&&f.worlds==1&&f.view.position[0]==10&&f.view.position[3]==0,"World and balanced mapped upload");
    require(f.view.camera==reinterpret_cast<uintptr_t>(&f.camera)&&f.output[3]==10&&f.output[35]==3&&f.output[39]==4&&f.output[46]==6&&f.output[47]==7&&f.output[48]==2.5f,"Shader constants and identity order");
    for(size_t i=49;i<64;++i)require(f.output[i]==123.5f,"Mapped spare lanes preserved");
    Fixture left,right;left.camera.orthographic=right.camera.orthographic=1;
    require(fs15_scene_view_prepare(&left.view,sizeof(left.view),&left.camera,1,1,0,0,&left.services)&&fs15_scene_view_prepare(&right.view,sizeof(right.view),&right.camera,1,1,1,0,&right.services),"Prepare orthographic stereo views");
    require(left.view.world[12]>10&&right.view.world[12]<10&&left.view.projection[12]>0&&right.view.projection[12]<0,"Stereo offsets and opposite off-axis shifts");
    Fixture short_map;short_map.capacity=48;require(!fs15_scene_view_prepare(&short_map.view,sizeof(short_map.view),&short_map.camera,1,0,0,0,&short_map.services)&&short_map.maps==1&&short_map.unmaps==1&&short_map.view.camera==0x5678,"Short map balances unmap and retains prefix mutations");
    for(float value:short_map.output)require(value==123.5f,"Short map writes no constants");
    Fixture missing;missing.null_map=true;require(!fs15_scene_view_prepare(&missing.view,sizeof(missing.view),&missing.camera,1,0,0,0,&missing.services)&&missing.maps==1&&missing.unmaps==0,"Null map fails without unmap");
    Fixture bad;const auto before=bad.view;require(!fs15_scene_view_prepare(&bad.view,sizeof(bad.view),&bad.camera,0,0,0,0,&bad.services)&&!std::memcmp(&bad.view,&before,sizeof(before))&&!bad.maps,"Invalid projection is staged before map");
    bad.settings.scalar30=std::numeric_limits<float>::infinity();require(!fs15_scene_view_upload(&bad.view,sizeof(bad.view),0,&bad.services)&&!bad.maps,"Nonfinite shader parameters reject before map");
    Fixture singular;singular.camera.orthographic=1;for(float& value:singular.camera.node.world)value=0;singular.view.inverse[12]=7;
    require(fs15_scene_view_prepare(&singular.view,sizeof(singular.view),&singular.camera,1,0,0,0,&singular.services)&&singular.view.inverse[12]==7,"Native singular inverse preserves prior inverse bytes");
    Fixture failed;failed.unmap_ok=false;require(!fs15_scene_view_prepare(&failed.view,sizeof(failed.view),&failed.camera,1,0,0,0,&failed.services)&&failed.view.camera==0x5678&&failed.output[3]==10,"Unmap failure retains written constants and old camera identity");
}
}
int main(){try{run();std::cout<<"Camera view/upload checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
