// Asset-free invariants, separate from comparisons with original machine code.
#include "animation.h"
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
namespace {
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
void word(float& f,uint32_t v){std::memcpy(&f,&v,4);}
template<class T,size_t N>FS15SceneVector view(std::array<T,N>& a) {
    const auto p=reinterpret_cast<uintptr_t>(a.data());return {p,p+sizeof(a),p+sizeof(a)};
}
struct Key {FS15AnimationKey header;float translation[3];int16_t xyzw[4];float scale[3];};
struct Host {
    unsigned refreshed=0,matrices=0,transforms=0,visibility=0;
    static int refresh(void* p,FS15SceneStateRecord* n){++static_cast<Host*>(p)->refreshed;n->flags&=~0x80u;return 1;}
    static int matrix(void* p,FS15SceneStateRecord* n,const float* m){++static_cast<Host*>(p)->matrices;std::memcpy(n->local,m,64);n->flags|=0x80u;return 1;}
    static int propagate(void* p,FS15SceneStateRecord*,uint32_t mask,int32_t origin) {
        auto& h=*static_cast<Host*>(p);if(origin)return 0;if(mask==1)++h.transforms;else if(mask==2)++h.visibility;else return 0;return 1;
    }
};
void run() {
    std::array<uint32_t,512> seeds{};require(fs15_animation_rsqrt_table(seeds.data(),seeds.size())&&seeds[256]==0x7f8000u,"Recovered rsqrt boundary seed");
    float zero[4]{};require(fs15_animation_normalize(zero,4)&&zero[0]==0,"Zero quaternion preserves native zero result");
    float q[4]={2,0,0,0};require(fs15_animation_normalize(q,4)&&q[0]==1,"Native normalization");
    float a[4]={1,0,0,0},b[4]={-1,0,0,0};
    require(fs15_animation_nlerp(a,b,0.5f,q,4)&&q[0]==1,"Antipodal shortest path");
    const std::array<float,4> saved={q[0],q[1],q[2],q[3]};
    require(!fs15_animation_nlerp(a,b,std::numeric_limits<float>::infinity(),q,4)&&!std::memcmp(q,saved.data(),16),"Nonfinite interpolation rejects without output writes");
    Key first{{1,0x8000,12,20},{0,0,0},{0,0,0,32767},{1,1,1}};
    Key last{{3,0,12,20},{10,20,30},{0,0,0,32767},{2,3,4}};
    std::array<const FS15AnimationKey*,2> keys={&first.header,&last.header};
    FS15AnimationChannel channel{};channel.flags=15;channel.keys=view(keys);channel.index_scale=32;channel.last_time=3;
    for(auto& i:channel.lookup)i=1;
    std::array<FS15AnimationChannel*,1> channels={&channel};FS15AnimationClip clip{};clip.duration=4;clip.channels=view(channels);
    require(fs15_animation_sample(&clip,4,1)&&channel.first==&last.header&&channel.second==&first.header&&channel.weights[0]==0.5f,"Exact duration endpoint uses loop seam");
    require(fs15_animation_sample(&clip,-0.5f,1)&&channel.weights[0]==0.75f,"Negative looping time");
    require(fs15_animation_sample(&clip,9,1)&&channel.first==&first.header&&channel.weights[0]==1,"Positive time wraps");
    require(fs15_animation_sample(&clip,2,0)&&channel.weights[0]==0.5f,"Middle key bracket");
    last.header.flags=0x10;require(fs15_animation_sample(&clip,2.5f,0)&&channel.weights[0]==0.15625f&&channel.weights[1]==0.25f,"Per-component easing bits");last.header.flags=0;
    const auto snapshot=channel;channel.lookup[32]=0;
    require(!fs15_animation_sample(&clip,2,0)&&channel.first==snapshot.first&&channel.weights[0]==snapshot.weights[0],"Invalid lookup rejected before sample commit");channel.lookup[32]=1;
    FS15CharacterData data{};std::array<FS15AnimationClip*,1> clips={&clip};data.clips=view(clips);
    std::memcpy(clip.name,"walk",5);size_t size=4,cap=15;std::memcpy(clip.name+16,&size,8);std::memcpy(clip.name+24,&cap,8);
    FS15CharacterSet set{};set.data=&data;set.flags=0x0101;word(set.state[0],7);set.state[8]=1;set.state[16]=1;
    uint32_t found=9;require(fs15_animation_find_clip(&set,"walk",5,&found)&&found==0,"Short clip name");
    require(fs15_animation_find_clip(&set,"Walk",5,&found)&&found==UINT32_MAX,"Clip lookup is case sensitive");
    set.flags=1;set.state[12]=5;require(fs15_animation_set_time(&set,0,6)&&set.flags==1&&set.state[12]==6,"Dirty uses clamped values, stores raw time on loops too");
    require(fs15_animation_set_time(&set,0,2)&&set.flags==0x101,"In-range seek dirties pose");
    FS15SceneStateRecord node{};for(unsigned i=0;i<4;++i)node.local[i*5]=1;node.flags=1;
    std::array<FS15SceneStateRecord*,1> nodes={&node};set.transforms=view(nodes);
    std::array<float,3> translation={0,0,0},scale={1,1,1};std::array<uint32_t,1> visibility={1};
    data.translation=view(translation);data.scale=view(scale);data.visibility_words=view(visibility);data.visibility_count=1;
    Host host;FS15AnimationServices services{&host,Host::refresh,Host::matrix,Host::propagate};
    require(fs15_animation_update(&set,0,&services)&&node.local[12]==5&&node.local[13]==10&&node.local[14]==15,"Four-track update applies sampled translation");
    require(node.local[0]==1.5f&&node.local[5]==2&&node.local[10]==2.5f&&host.matrices==1&&host.transforms==1&&host.refreshed==0,"Quaternion basis plus scale");
    require(set.flags==1&&host.visibility==0,"Dirty cleared, unchanged visibility suppressed");
    require(fs15_animation_set_time(&set,0,3.5f)&&fs15_animation_update(&set,0,&services)&&host.visibility==1&&(node.flags&1u)==0,"Visibility uses first selected key");
    channel.flags=4;node.scale[0]=1.5f;node.scale[1]=2;node.scale[2]=2.5f;
    require(fs15_animation_set_time(&set,0,2)&&fs15_animation_update(&set,0,&services)&&host.refreshed==1,"Scale-only update refreshes cached TRS");
    channel.flags=1;word(set.state[0],3);word(set.state[1],3);
    set.state[8]=set.state[9]=0;set.state[16]=set.state[17]=0.5f;set.flags=0x101;
    set.state[12]=1;set.state[13]=3;
    require(fs15_animation_update(&set,0,&services)&&node.local[12]==10,"Shared clip selection uses last active track for both contributions");
    set.flags=0;const auto before=node;const float clock=set.state[12];
    require(fs15_animation_update(&set,1,&services)&&set.state[12]==clock&&!std::memcmp(&node,&before,sizeof(node)),"Disabled set does not advance clocks");
    require(!fs15_animation_set_weight(&set,4,1)&&!fs15_animation_set_time(&set,4,0),"Reject fifth track");
}
}
int main(){try{run();std::cout<<"Recovered animation invariants passed; full engine/skinning not tested.\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
