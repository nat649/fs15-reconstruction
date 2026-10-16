// Asset-free resource-to-pose integration and authored failure-guard checks.
#include "animation_resources.h"
#include "animation_fixture.h"
#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
struct File {
    std::vector<uint8_t> bytes;
    void integer(uint32_t u){for(int shift=24;shift>=0;shift-=8)bytes.push_back(static_cast<uint8_t>(u>>shift));}
    void number(float f){uint32_t u;std::memcpy(&u,&f,4);integer(u);}
    void text(const std::string& s){integer(static_cast<uint32_t>(s.size()));bytes.insert(bytes.end(),s.begin(),s.end());while(bytes.size()%4)bytes.push_back(0);}
    explicit File(unsigned groups=1,uint32_t node=101) {
        integer(3);integer(groups);
        for(unsigned g=0;g<groups;++g) {
            text("character");integer(1);text("translation-and-visibility");number(10);integer(1);integer(node);integer(2);
            number(0);integer(0x8009);number(1);number(2);number(3);
            number(10);integer(9);number(11);number(12);number(13);
        }
    }
};
using Owner=fs15_animation_test::Owner;
using PoseHost=fs15_animation_test::PoseHost;
void run() {
    const File file;
    Owner o;auto services=o.services();FS15AnimationLoadStats stats{};
    require(fs15_animation_load_binary(file.bytes.data(),file.bytes.size(),1,&services,&stats)&&stats.consumed==file.bytes.size()&&stats.loaded==1&&!stats.skipped&&!stats.binding_warnings,"Load a version-three binary resource");
    require(o.groups==1&&o.diagnostic==0&&o.last&&o.last->references==1&&o.last->data->flags==9,"Loaded set retains original control mask and ownership");
    uint32_t found=UINT32_MAX;require(fs15_animation_find_clip(o.last,"translation-and-visibility",27,&found)&&found==0,"Loaded long clip name resolves");
    uint32_t flags=3;std::memcpy(&o.last->state[0],&flags,4);o.last->state[8]=0;
    require(fs15_animation_set_time(o.last,0,5),"Seek loaded animation");
    PoseHost host(o.node);FS15AnimationServices pose{&host,PoseHost::refresh,PoseHost::matrix,PoseHost::propagate};
    require(fs15_animation_update(o.last,0,&pose)&&o.node->local[12]==6&&o.node->local[13]==7&&o.node->local[14]==8&&(o.node->flags&1u),"Binary resource reaches recovered pose/visibility services");
    require(fs15_animation_set_time(o.last,0,10)&&fs15_animation_update(o.last,0,&pose)&&o.node->local[12]==11&&!(o.node->flags&1u),"Loaded endpoint changes translation and visibility");o.cleanup();

    Owner rebound;auto rs=rebound.services();const File twice(2);
    require(fs15_animation_load_binary(twice.bytes.data(),twice.bytes.size(),1,&rs,&stats)&&stats.loaded==2&&rebound.registry.size()==2&&rebound.groups==2,"Later groups rebind nodes and release earlier groups");rebound.cleanup();
    Owner missing;auto ms=missing.services();const File absent(1,999);
    require(fs15_animation_load_binary(absent.bytes.data(),absent.bytes.size(),1,&ms,&stats)&&stats.skipped==1&&missing.diagnostic==3&&!missing.last&&missing.registry.size()==1,"Missing-node group is skipped and destroyed");missing.cleanup();

    Owner malformed;auto guard=malformed.services();const auto owned=malformed.blocks.size();FS15AnimationLoadStats sentinel{123,4,5,6,7};
    for(size_t bytes=0;bytes<file.bytes.size();++bytes) {
        stats=sentinel;
        require(!fs15_animation_load_binary(file.bytes.data(),bytes,1,&guard,&stats)&&!std::memcmp(&stats,&sentinel,sizeof(stats))&&malformed.blocks.size()==owned&&malformed.groups==0,"Truncated file rejected before engine allocation");
    }
    auto invalid=file.bytes;invalid[3]=4;stats=sentinel;
    require(!fs15_animation_load_binary(invalid.data(),invalid.size(),1,&guard,&stats)&&malformed.blocks.size()==owned,"Unsupported version rejected");malformed.cleanup();
    unsigned failures=0,successes=0;
    for(int fail=0;fail<24;++fail) {
        Owner limited;auto ls=limited.services();limited.fail_after=fail;
        const bool ok=fs15_animation_load_binary(file.bytes.data(),file.bytes.size(),1,&ls,&stats)!=0;
        limited.fail_after=-1;if(ok)++successes;else ++failures;limited.cleanup();
    }
    require(failures>10&&successes>0,"Exercise acquisition failures and eventual success");
    FS15AnimationChannel empty{};empty.lookup[1]=0x1234;empty.index_scale=9;
    require(fs15_animation_channel_finalize(&empty)&&empty.lookup[0]==0&&empty.lookup[1]==0x1234&&empty.index_scale==0,"Empty finalization preserves unvisited native lookup slots");
}
}
int main(){try{run();std::cout<<"Recovered animation resource-to-pose and failure guards passed. No complete engine tested.\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
