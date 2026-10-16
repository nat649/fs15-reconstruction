// Independent ownership/binding invariants; original-code proof is separate.
#include "shared_scene.h"
#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
namespace {
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
struct Owner {
    std::map<void*,size_t> blocks;std::map<uint32_t,void*> registry;uint32_t id=0;int fail_after=-1;
    FS15SharedSceneContext context{};FS15SceneLifecycleServices services{};
    Owner(){services={this,allocate,release,next,registered,unregistered,nullptr,nullptr,shared};}
    ~Owner(){for(const auto& b:blocks)std::free(b.first);}
    static void* allocate(void* u,size_t size) {
        auto& o=*static_cast<Owner*>(u);if(o.fail_after==0)return nullptr;if(o.fail_after>0)--o.fail_after;
        void* p=std::malloc(size);if(p){std::memset(p,0xa5,size);o.blocks.emplace(p,size);}return p;
    }
    static void release(void* u,void* p){if(p){auto& o=*static_cast<Owner*>(u);if(!o.blocks.erase(p))std::abort();std::free(p);}}
    static uint32_t next(void* u){return ++static_cast<Owner*>(u)->id;}
    static void registered(void* u,uint32_t id,void* p){static_cast<Owner*>(u)->registry.emplace(id,p);}
    static void unregistered(void* u,uint32_t id,void*){static_cast<Owner*>(u)->registry.erase(id);}
    static int shared(void* u,void* p){auto& o=*static_cast<Owner*>(u);return fs15_shared_scene_release(p,0x10,&o.context,&o.services);}
    template<class T>T* memory(){auto* p=static_cast<T*>(allocate(this,sizeof(T)));require(p!=nullptr,"Allocate fixture record");return p;}
    FS15CharacterSet* set(FS15CharacterData* data=nullptr) {
        auto* p=memory<FS15CharacterSet>();require(fs15_character_set_construct(p,sizeof(*p),"set",4,data,&context,&services),"Construct CharacterSet");return p;
    }
    FS15SceneStateRecord* node() {
        auto* p=memory<FS15SceneStateRecord>();require(fs15_scene_construct(p,sizeof(*p),FS15_OBJECT_TRS,"bone",5,0,1,&services),"Construct bone");return p;
    }
    void clear_context() {
        require(context.active.begin==context.active.end&&context.pending.begin==context.pending.end,"All observers removed");
        release(this,reinterpret_cast<void*>(context.active.begin));release(this,reinterpret_cast<void*>(context.pending.begin));context={};
    }
};
void run() {
    Owner o;auto* data=o.memory<FS15CharacterData>();require(fs15_character_data_construct(data,sizeof(*data)),"Construct shared data");
    auto* first=o.set(data);auto* second=o.set(data);
    require(data->references==2&&first->references==0&&first->reference_table==0x1404c9688,"Native data ownership and embedded interface");
    require(first->state[0]==0&&first->state[8]==1&&first->state[12]==0&&first->state[16]==1,"Native initial animation state");
    std::array<FS15SceneStateRecord*,3> bones={o.node(),o.node(),o.node()};
    for(size_t i=0;i<bones.size();++i){bones[i]->translation[0]=float(i*2);bones[i]->translation[1]=3;bones[i]->scale[0]=2;}
    int diagnostic=-1;
    require(fs15_character_set_bind(first,bones.data(),bones.size(),&o.context,&o.services,&diagnostic)&&diagnostic==0,"Bind default poses");
    require(first->references==3&&first->flags==0x101&&data->visibility_count==3&&data->translation.end-data->translation.begin==36,"Pose and visibility counts");
    const auto* positions=reinterpret_cast<const float*>(data->translation.begin);
    require(positions[0]==0&&positions[1]==3&&positions[6]==4&&reinterpret_cast<const uint32_t*>(data->visibility_words.begin)[0]==7,"Original translation and visibility data");
    ++first->references;
    require(fs15_character_set_bind(second,bones.data(),bones.size(),&o.context,&o.services,&diagnostic)&&first->references==1&&second->references==3&&data->references==2,"Rebinding releases old transform references without duplicating shared data");
    require(fs15_shared_scene_release(reinterpret_cast<uint8_t*>(first)+0x28,0x10,&o.context,&o.services)&&data->references==1,"Last old-set reference deletes set and retains shared pose data");
    for(auto* bone:bones)require(fs15_scene_destruct(bone,sizeof(*bone),FS15_OBJECT_TRS,1,&o.services),"Node lifetime releases CharacterSet reference");
    o.clear_context();require(o.blocks.empty()&&o.registry.empty(),"Last node releases both set and data");

    Owner prefix;auto* set=prefix.set();auto* valid=prefix.node();auto* invalid=prefix.node();invalid->type_flags&=~uint32_t(0x40);
    std::array<FS15SceneStateRecord*,2> mixed={valid,invalid};
    require(fs15_character_set_bind(set,mixed.data(),2,&prefix.context,&prefix.services,&diagnostic)&&diagnostic==1&&set->references==1&&set->flags==0&&invalid->shared_resource==0,"Non-TRS warning preserves native bound prefix");
    require(fs15_scene_destruct(valid,sizeof(*valid),FS15_OBJECT_TRS,1,&prefix.services)&&fs15_scene_destruct(invalid,sizeof(*invalid),FS15_OBJECT_TRS,1,&prefix.services),"Clean up warning prefix");
    prefix.clear_context();require(prefix.blocks.empty(),"Warning prefix owns no leaked resources");

    Owner guard;uint64_t unknown[2]={123,10};
    require(!fs15_shared_scene_release(unknown,sizeof(unknown),&guard.context,&guard.services)&&unknown[1]==10,"Reject unknown resource dispatch before decrement");
    FS15SceneStateRecord record{};
    for(int failure=0;failure<3;++failure) {
        guard.fail_after=failure;
        FS15CharacterSet failed{};
        require(!fs15_character_set_construct(&failed,sizeof(failed),"set",4,nullptr,&guard.context,&guard.services)&&guard.blocks.empty()&&guard.registry.empty(),"Failed shared construction cleans acquired storage and registration");
    }
    (void)record;
}
}
int main(){try{run();std::cout<<"Recovered shared animation ownership and binding invariants passed. No animation evaluator or full engine tested.\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
