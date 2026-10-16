// Asset-free resource-to-pose integration and authored failure-guard checks.
#include "animation_resources.h"
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
struct Owner {
    std::map<void*,size_t> blocks;std::map<uint32_t,void*> registry;uint32_t id=0;int fail_after=-1;
    FS15SharedSceneContext context{};FS15SceneLifecycleServices memory{};
    FS15SceneStateRecord* node=nullptr;FS15CharacterSet* last=nullptr;unsigned diagnostic=0,groups=0;
    Owner() {
        memory={this,allocate,release,next,registered,unregistered,nullptr,nullptr,shared};
        node=static_cast<FS15SceneStateRecord*>(allocate(this,sizeof(FS15SceneStateRecord)));
        require(fs15_scene_construct(node,sizeof(*node),FS15_OBJECT_TRS,"node",5,0,1,&memory),"Construct resource fixture node");
    }
    ~Owner(){for(const auto& b:blocks)std::free(b.first);}
    static void* allocate(void* p,size_t size) {
        auto& o=*static_cast<Owner*>(p);if(o.fail_after==0)return nullptr;if(o.fail_after>0)--o.fail_after;
        void* block=std::malloc(size);if(block){std::memset(block,0xa5,size);o.blocks.emplace(block,size);}return block;
    }
    static void release(void* p,void* block){if(!block)return;auto& o=*static_cast<Owner*>(p);if(!o.blocks.erase(block))std::abort();std::free(block);}
    static uint32_t next(void* p){return ++static_cast<Owner*>(p)->id;}
    static void registered(void* p,uint32_t id,void* object){static_cast<Owner*>(p)->registry.emplace(id,object);}
    static void unregistered(void* p,uint32_t id,void*){static_cast<Owner*>(p)->registry.erase(id);}
    static int shared(void* p,void* object){auto& o=*static_cast<Owner*>(p);return fs15_shared_scene_release(object,0x10,&o.context,&o.memory);}
    static FS15SceneStateRecord* resolve(void* p,uint32_t id){return id==101?static_cast<Owner*>(p)->node:nullptr;}
    static void loaded(void* p,FS15CharacterSet* set,uint32_t d){auto& o=*static_cast<Owner*>(p);o.last=set;o.diagnostic=d;++o.groups;}
    FS15AnimationResourceServices services(){return {this,&memory,&context,resolve,loaded};}
    void cleanup() {
        // A group with no transform references is explicitly owned by the host.
        if(last) {
            uint32_t identifier;std::memcpy(&identifier,last->object_header+0x18,4);
            if(registry.count(identifier)&&last->references==0)
                require(fs15_character_set_destruct(last,sizeof(*last),1,&context,&memory),"Release zero-reference resource group");
        }
        require(fs15_scene_destruct(node,sizeof(*node),FS15_OBJECT_TRS,1,&memory),"Node destruction releases loaded resources");node=nullptr;
        require(registry.empty(),"Loaded object registry is empty");
        require(context.active.begin==context.active.end&&context.pending.begin==context.pending.end,"Loaded observers removed");
        release(this,reinterpret_cast<void*>(context.active.begin));release(this,reinterpret_cast<void*>(context.pending.begin));context={};
        require(blocks.empty(),"Resource loading owns no leaked buffers");last=nullptr;
    }
};
struct PoseHost {
    uint8_t kind=FS15_SCENE_TRS_NODE;FS15SceneStateArena arena{};
    explicit PoseHost(FS15SceneStateRecord* node){arena={node,sizeof(*node),sizeof(*node),nullptr,0,nullptr,0,reinterpret_cast<FS15ScenePropertyView*>(node->properties),1,&kind,1,SIZE_MAX};}
    static int refresh(void* p,FS15SceneStateRecord*){auto& h=*static_cast<PoseHost*>(p);size_t count=0;int warning=0;return fs15_scene_trs_update(&h.arena,0,FS15_SCENE_TRS_REFRESH,nullptr,0,nullptr,0,&count,&warning);}
    static int matrix(void* p,FS15SceneStateRecord*,const float* m){return fs15_scene_set_local_matrix(&static_cast<PoseHost*>(p)->arena,0,m,16);}
    static int propagate(void* p,FS15SceneStateRecord*,uint32_t mask,int32_t origin){size_t count=0;return fs15_scene_state_update(&static_cast<PoseHost*>(p)->arena,0,FS15_SCENE_PROPAGATE,mask,origin,nullptr,0,&count);}
};
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
