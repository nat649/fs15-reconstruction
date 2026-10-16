#pragma once
// Shared allocator/scene fixture for resource-to-pose integration tests.
#include "animation_resources.h"
#include <cstdlib>
#include <cstring>
#include <map>
#include <stdexcept>
namespace fs15_animation_test {
inline void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
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
}
