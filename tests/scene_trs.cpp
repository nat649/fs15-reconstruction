// Asset-free transform invariants/guards. Original-code comparisons are separate.
#include "scene_trs.h"
#include "scene_state.h"
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void require(bool value,const char* text){if(!value)throw std::runtime_error(text);}
bool near(float a,float b){return std::fabs(a-b)<=0.000004f;}
void run() {
    const float pi=3.14159265358979323846f;
    float angles[]={pi/2,0,0},scale[]={2,3,4},position[]={10,20,30},m[16]{};
    require(fs15_compose_trs(angles,scale,position,m,16),"Compose native TRS");
    require(near(m[0],2)&&near(m[6],3)&&near(m[9],-4)&&m[12]==10&&m[13]==20&&m[14]==30&&m[15]==1,"X rotation, column scale and translation");
    float extracted[3]{};
    require(fs15_matrix_euler(m,16,scale,extracted,3)&&near(extracted[0],pi/2)&&near(extracted[1],0)&&near(extracted[2],0),"Scaled Euler extraction");
    angles[0]=0.2f;angles[1]=0.7f;angles[2]=-0.9f;
    require(fs15_compose_trs(angles,scale,position,m,16)&&fs15_matrix_euler(m,16,scale,extracted,3),"Combined rotation round trip");
    for(unsigned i=0;i<3;++i)require(near(angles[i],extracted[i]),"XYZ angle convention round trip");
    angles[1]=pi/2;
    require(fs15_compose_trs(angles,scale,position,m,16)&&fs15_matrix_euler(m,16,scale,extracted,3)&&extracted[2]==0,"Native gimbal branch sets Z to zero");
    const auto before=m[0];
    require(!fs15_rotate_affine_axis(m,16,0,3,m,16)&&m[0]==before,"Unknown axis preserves matrix");
    angles[0]=std::numeric_limits<float>::quiet_NaN();
    require(!fs15_compose_trs(angles,scale,position,m,16)&&m[0]==before,"Nonfinite rotation rejected");

    FS15SceneStateRecord node{};FS15ScenePropertyView properties{};uint8_t kind=FS15_SCENE_TRS_NODE;
    node.type_flags=0x20;node.flags=1;node.properties=reinterpret_cast<uintptr_t>(&properties);
    node.scale[0]=node.scale[1]=node.scale[2]=1;
    FS15SceneStateArena view={&node,sizeof(node),sizeof(node),nullptr,0,nullptr,0,&properties,1,&kind,1,0};
    unsigned char frame[0x120]{};const float p[]={7,8,9};
    for(unsigned i=0;i<3;++i)std::memcpy(frame+i*16,p+i,4);
    size_t used=99;int warning=-1;
    require(fs15_scene_trs_update(&view,0,FS15_SCENE_SET_TRANSLATION,frame,sizeof(frame),nullptr,0,&used,&warning),"Translation setter and rebuild");
    require(node.local[12]==7&&node.local[13]==8&&node.local[14]==9&&(node.flags&0x40)&&!(node.flags&0x80)&&used==0,"Setter dirties world and leaves TRS fresh");
    require(fs15_scene_trs_update(&view,0,FS15_SCENE_GET_TRANSLATION,frame,sizeof(frame),nullptr,0,&used,&warning),"Translation getter");
    for(unsigned i=0;i<3;++i){float value=0;uint16_t type=0;std::memcpy(&value,frame+0xf0+i*16,4);std::memcpy(&type,frame+0xf8+i*16,2);require(value==p[i]&&type==3,"Script result slot/value tags");}
    node.local[12]=42;node.flags|=0x80;
    require(fs15_scene_trs_update(&view,0,FS15_SCENE_GET_TRANSLATION,frame,sizeof(frame),nullptr,0,&used,&warning)&&node.translation[0]==42&&!(node.flags&0x80),"Lazy cached TRS refresh");
    properties.flags=0x88;
    require(fs15_scene_trs_update(&view,0,FS15_SCENE_SET_SCALE,frame,sizeof(frame),nullptr,0,&used,&warning)&&warning==1&&node.scale[0]==7,"Dynamic scale diagnostic preserves original store behavior");
    const auto saved=node;const float inf=std::numeric_limits<float>::infinity();std::memcpy(frame,&inf,4);
    require(!fs15_scene_trs_update(&view,0,FS15_SCENE_SET_TRANSLATION,frame,sizeof(frame),nullptr,0,&used,&warning)&&std::memcmp(&saved,&node,sizeof(node))==0,"Rejected input preserves complete node");
    require(!fs15_scene_trs_update(&view,0,FS15_SCENE_GET_SCALE,frame,0x119,nullptr,0,&used,&warning),"Truncated getter frame rejected");
}
}
int main(){try{run();std::cout<<"Recovered TRS invariants and authored guards passed; platform libm used, no engine startup.\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
