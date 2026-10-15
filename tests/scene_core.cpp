// Asset-free invariants and authored guard tests, separate from native parity.
#include "scene_math.h"
#include "scene_graph.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
bool near(float a,float b){return std::fabs(a-b)<0.00002f;}
constexpr std::array<float,16> identity={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
template<class T>void write(unsigned char* at,const T& value){std::memcpy(at,&value,sizeof(value));}
void scene_tests() {
    constexpr size_t stride=0x118;
    std::array<unsigned char,stride*3> nodes{};
    for(size_t i=0;i<3;++i) {
        auto matrix=identity;matrix[12]=float(i+1);matrix[13]=float(i*2);
        std::memcpy(nodes.data()+i*stride+0x98,matrix.data(),64);
        write(nodes.data()+i*stride+0x50,uint32_t(0x12345678)); // dirty bit set
        const uintptr_t parent=i?reinterpret_cast<uintptr_t>(nodes.data()+(i-1)*stride):0;
        write(nodes.data()+i*stride+0x28,parent);
    }
    require(fs15_scene_update_world(nodes.data(),nodes.size(),stride,2)==1,"Dirty world chain");
    std::array<float,16> world{};std::memcpy(world.data(),nodes.data()+2*stride+0xd8,64);
    require(world[12]==6&&world[13]==6&&world[15]==1,"Parent translations accumulate");
    uint32_t flags=0;std::memcpy(&flags,nodes.data()+2*stride+0x50,4);
    require(flags==(0x12345678u&~0x40u),"Only native dirty bit cleared");
    const auto before=nodes;
    require(fs15_scene_update_world(nodes.data(),nodes.size(),stride,2)&&nodes==before,"Cache hit preserves arena");
    std::array<unsigned char,0x120> frame;frame.fill(0xa5);
    const float x=7,y=8,z=9;write(frame.data(),x);write(frame.data()+16,y);write(frame.data()+32,z);
    const auto initial_frame=frame;
    require(fs15_scene_transform_frame(FS15_WORLD_TO_LOCAL,nodes.data(),nodes.size(),stride,2,frame.data(),frame.size()),"World-to-local script bridge");
    for(size_t lane=0;lane<3;++lane) {
        float v=0;uint16_t type=0;std::memcpy(&v,frame.data()+0xf0+lane*16,4);std::memcpy(&type,frame.data()+0xf8+lane*16,2);
        require(v==(lane==0?1:lane==1?2:9)&&type==3,"Script value/type layout");
    }
    for(size_t i=0;i<frame.size();++i) {
        const bool changed=(i>=0xf0&&i<0xf4)||(i>=0x100&&i<0x104)||(i>=0x110&&i<0x114)||
            (i>=0xf8&&i<0xfa)||(i>=0x108&&i<0x10a)||(i>=0x118&&i<0x11a);
        if(!changed)require(frame[i]==initial_frame[i],"Unrelated frame bytes preserved");
    }
    require(!fs15_scene_transform_frame(9,nodes.data(),nodes.size(),stride,2,frame.data(),frame.size()),"Unknown scene operation");
    require(!fs15_scene_transform_frame(0,nodes.data(),nodes.size(),stride,2,frame.data(),0x119),"Truncated script frame");
    require(!fs15_scene_transform_frame(0,nodes.data(),nodes.size(),stride,2,nodes.data(),0x120)&&nodes==before,"Frame/arena overlap rejection");
    write(nodes.data()+2*stride+0x50,uint32_t(0x40));
    write(nodes.data()+2*stride+0x28,reinterpret_cast<uintptr_t>(nodes.data()+2*stride));
    const auto cycle=nodes;
    require(!fs15_scene_update_world(nodes.data(),nodes.size(),stride,2)&&nodes==cycle,"Cycle rejection is atomic");
    write(nodes.data()+2*stride+0x28,reinterpret_cast<uintptr_t>(nodes.data()+1));
    const auto misaligned=nodes;
    require(!fs15_scene_update_world(nodes.data(),nodes.size(),stride,2)&&nodes==misaligned,"Interior parent pointer rejected");
    write(nodes.data()+2*stride+0x28,uintptr_t(1));
    require(!fs15_scene_update_world(nodes.data(),nodes.size(),stride,2),"External parent pointer rejected");
    require(!fs15_scene_update_world(nodes.data(),nodes.size(),stride,3),"Out-of-range node rejected");
    require(!fs15_scene_update_world(nodes.data(),nodes.size(),0x117,0),"Truncated node layout rejected");
    nodes=before;auto zero=identity;zero[0]=0;
    std::memcpy(nodes.data()+2*stride+0xd8,zero.data(),64);frame=initial_frame;
    require(!fs15_scene_transform_frame(FS15_WORLD_TO_LOCAL,nodes.data(),nodes.size(),stride,2,frame.data(),frame.size())&&frame==initial_frame,"Singular inverse leaves script frame intact");
    nodes=before;write(nodes.data()+2*stride+0x50,uint32_t(0x40));
    write(nodes.data()+2*stride+0x98,std::numeric_limits<float>::infinity());
    const auto nonfinite=nodes;
    require(!fs15_scene_update_world(nodes.data(),nodes.size(),stride,2)&&nodes==nonfinite,"Nonfinite dirty update is atomic");
}
}
int main() {
    try {
        auto a=identity;a[0]=2;a[5]=-3;a[10]=4;a[12]=10;a[13]=20;a[14]=30;
        auto inverse=identity;
        require(fs15_affine_inverse(a.data(),16,inverse.data(),16),"Affine inversion");
        auto product=identity;
        require(fs15_affine_multiply(a.data(),16,inverse.data(),16,product.data(),16),"Inverse product");
        for(unsigned i=0;i<16;++i)require(near(product[i],identity[i]),"Scaled/reflected inverse round trip");
        require(fs15_affine_multiply(a.data(),16,inverse.data(),16,a.data(),16),"Aliased composition");
        for(unsigned i=0;i<16;++i)require(near(a[i],identity[i]),"Aliased composition result");
        const float q[]={0,0,1,0};a=identity;a[12]=73;a[3]=99;
        require(fs15_quaternion_to_matrix(q,4,a.data(),16)&&a[0]==-1&&a[10]==-1&&a[5]==1,"WXYZ half-turn around Y");
        require(a[12]==73&&a[3]==99,"Quaternion conversion retains translation/padding");
        float back[4]{};require(fs15_matrix_to_quaternion(a.data(),16,back,4)&&back[2]==1,"Dominant-axis quaternion extraction");
        const float point[]={5,6,7};float local[3]{};
        require(fs15_rigid_inverse_point(a.data(),16,point,local,3)&&local[0]==68&&local[1]==6&&local[2]==-7,"Rigid inverse point");
        float scale[3]{};require(fs15_matrix_scale(a.data(),16,scale,3)&&scale[0]==1&&scale[1]==1&&scale[2]==1,"Rotation scale magnitudes");
        a=identity;a[0]=0;const auto before=a;
        require(!fs15_affine_inverse(a.data(),16,a.data(),16)&&a==before,"Singular inverse preserves aliased output");
        require(!fs15_affine_multiply(a.data(),15,a.data(),16,a.data(),16)&&a==before,"Short matrix rejected");
        a[0]=std::numeric_limits<float>::quiet_NaN();
        require(!fs15_affine_inverse(a.data(),16,inverse.data(),16),"Nonfinite matrix rejected");
        scene_tests();
        std::cout<<"Recovered scene math/cache/script layouts and authored bounds guards passed. No engine/console startup tested.\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
