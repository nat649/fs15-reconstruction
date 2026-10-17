#include "scene_projection.h"
#include "scene_visibility.h"
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
namespace {
void require(bool value,const char* text){if(!value)throw std::runtime_error(text);}
void run(){
    float p[16],o[16];require(fs15_scene_perspective(-2,2,-1,1,1,9,p,16),"Perspective matrix");
    require(p[0]==0.5f&&p[5]==1&&p[10]==-1.25f&&p[11]==-1&&p[14]==-2.25f&&p[15]==0,"Native perspective depth coefficients");
    auto depth=[&](float z){return (p[10]*z+p[14])/(p[11]*z);};
    require(depth(-1)==-1&&depth(-9)==1,"Perspective near/far map to native clip depth");
    require(fs15_scene_orthographic(-2,6,-1,3,1,9,o,16),"Off-axis orthographic matrix");
    require(o[0]==0.25f&&o[5]==0.5f&&o[12]==-0.5f&&o[13]==-0.5f&&o[10]==-0.25f&&o[14]==-1.25f,"Orthographic extents and depth");
    float view[16]{};for(size_t i=0;i<4;++i)view[5*i]=1;view[12]=-10;
    float combined[16];require(fs15_scene_projective_multiply(p,16,view,16,combined,16),"Projection times view");
    FS15SceneFrustum volume{};require(fs15_scene_frustum_matrix(&volume,combined,16),"Extract planes from projected camera");
    int accepted=0;float center[]={10,0,-5},side[]={30,0,-5};
    require(fs15_scene_frustum_point(&volume,center,&accepted)&&accepted,"Camera translation affects visible world point");
    require(fs15_scene_frustum_point(&volume,side,&accepted)&&!accepted,"Camera side plane rejects world point");
    float alias[16];std::memcpy(alias,view,sizeof(alias));require(fs15_scene_projective_multiply(p,16,alias,16,alias,16)&&!std::memcmp(alias,combined,sizeof(alias)),"Authored right-alias staging");
    std::memcpy(alias,p,sizeof(alias));require(fs15_scene_projective_multiply(alias,16,view,16,alias,16)&&!std::memcmp(alias,combined,sizeof(alias)),"Authored left-alias staging");
    float a[16];for(size_t i=0;i<16;++i)a[i]=float(i+1);a[0]=2;a[1]=a[2]=0;a[4]=1;a[5]=2;a[6]=0;a[8]=1;a[9]=1;a[10]=4;
    float before[16];std::memcpy(before,a,sizeof(a));require(fs15_scene_camera_basis(a,16),"Native camera basis conditioning");
    require(a[0]==1&&a[4]>0.44f&&a[4]<0.45f&&a[5]>0.89f&&a[5]<0.90f,"Retain native dot-product comparison near one before normalizing second vector");
    for(size_t i:{3u,7u,11u,12u,13u,14u,15u})require(a[i]==before[i],"Non-basis lanes preserved");
    std::memcpy(before,p,sizeof(p));require(!fs15_scene_perspective(1,1,-1,1,1,9,p,16)&&!std::memcmp(before,p,sizeof(p)),"Zero-span guard is atomic");
    require(fs15_scene_perspective(2,-2,1,-1,9,1,p,16),"Reversed native extents are allowed");
    float bad[16]{};bad[0]=std::numeric_limits<float>::infinity();std::memcpy(before,bad,sizeof(bad));require(!fs15_scene_camera_basis(bad,16)&&!std::memcmp(before,bad,sizeof(bad)),"Nonfinite basis rejects atomically");
    std::memcpy(before,p,sizeof(p));require(!fs15_scene_projective_multiply(nullptr,16,p,16,p,16)&&!std::memcmp(before,p,sizeof(p)),"Null product guard");
    const float max=std::numeric_limits<float>::max();require(!fs15_scene_perspective(-1,1,-1,1,max,max/2,p,16)&&!std::memcmp(before,p,sizeof(p)),"Overflow result guard");
}
}
int main(){try{run();std::cout<<"Camera projection checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
