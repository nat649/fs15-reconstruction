#pragma once
// Authored transport joining recovered group, animation and owned scene paths.
#include "scene_groups.h"
#include "scene_owned.h"
#include "animation_fixture.h"
#include <array>
#include <cmath>
#include <map>
#include <vector>
namespace fs15_group_test {
using fs15_animation_test::Owner;
using fs15_animation_test::require;
struct Fixture:Owner {
    std::vector<FS15SceneStateRecord*> nodes;
    std::vector<uint8_t> kinds;
    std::map<FS15SceneStateRecord*,size_t> indices;
    std::map<uint32_t,FS15SceneStateRecord*> ids;
    std::vector<std::pair<uint32_t,FS15CharacterSet*>> orphan_sets;
    FS15SceneStateRecord* current=nullptr;
    FS15SceneLodContext lod{};
    FS15SceneLodServices lod_services{this,&memory,nullptr,world,mark};
    FS15SceneGroupServices group_services{{this,&memory,rebuild,propagate,integer,registered_id},constructed,link};
    std::array<size_t,128> requests{};FS15SceneLodQueue queue{requests.data(),requests.size(),0};
    size_t used=0;uint32_t attached=0;int warning=0;
    bool fail_constructed=false,fail_link=false;
    Fixture() {
        nodes.push_back(node);kinds.push_back(1);indices.emplace(node,0);current=node;
        memory.unlink=unlink;memory.destroy_child=destroy;
        auto v=view();require(fs15_scene_owned_state_lod(&v,0,0,2,0,nullptr,0,&used,&queue)&&enqueue(),"Initialize visible group root");
    }
    FS15SceneOwnedView view(){return {nodes.data(),nodes.size(),kinds.data(),0};}
    static Fixture& memory_owner(void* p){return *static_cast<Fixture*>(static_cast<Owner*>(p));}
    int enqueue(){for(size_t i=0;i<queue.count;++i)if(!fs15_scene_lod_enqueue(&lod,reinterpret_cast<FS15SceneLodRecord*>(nodes[requests[i]]),&memory)){queue.count=0;return 0;}queue.count=0;return 1;}
    static int rebuild(void* p,FS15SceneStateRecord* n) {
        auto& f=*static_cast<Fixture*>(p);auto v=f.view();const int ok=fs15_scene_owned_trs_lod(&v,f.indices.at(n),1,nullptr,0,nullptr,0,&f.used,&f.warning,&f.queue);return ok&&f.enqueue();
    }
    static int refresh(void* p,FS15SceneStateRecord* n) {
        auto& f=*static_cast<Fixture*>(p);auto v=f.view();const int ok=fs15_scene_owned_trs_lod(&v,f.indices.at(n),0,nullptr,0,nullptr,0,&f.used,&f.warning,&f.queue);return ok&&f.enqueue();
    }
    static int propagate(void* p,FS15SceneStateRecord* n,uint32_t mask,int32_t origin) {
        auto& f=*static_cast<Fixture*>(p);auto v=f.view();const int ok=fs15_scene_owned_state_lod(&v,f.indices.at(n),0,mask,origin,nullptr,0,&f.used,&f.queue);return ok&&f.enqueue();
    }
    static int matrix(void* p,FS15SceneStateRecord* n,const float* m){auto& f=*static_cast<Fixture*>(p);auto v=f.view();return fs15_scene_owned_matrix(&v,f.indices.at(n),m,16);}
    static int world(void* p,FS15SceneStateRecord* n,float* out){auto& f=*static_cast<Fixture*>(p);auto v=f.view();return fs15_scene_owned_world(&v,f.indices.at(n),out,3);}
    static int mark(void*,FS15SceneStateRecord* n,uint32_t){return fs15_scene_owned_mark_branch(n);}
    static int integer(void*,const char* text,uint32_t* out){*out=static_cast<uint32_t>(std::strtol(text,nullptr,10));return 1;}
    static int duration(void*,const char* text,float* out){*out=static_cast<float>(std::strtod(text,nullptr));return std::isfinite(*out);}
    static int registered_id(void* p,uint32_t id,FS15SceneStateRecord* n){static_cast<Fixture*>(p)->ids.emplace(id,n);return 1;}
    static int constructed(void* p,FS15SceneStateRecord* n,unsigned kind,size_t) {
        auto& f=*static_cast<Fixture*>(p);f.indices.emplace(n,f.nodes.size());f.nodes.push_back(n);f.kinds.push_back(static_cast<uint8_t>(kind));return !f.fail_constructed;
    }
    static int link(void* p,FS15SceneStateRecord* parent,FS15SceneStateRecord* child,uint32_t position) {
        auto& f=*static_cast<Fixture*>(p);if(f.fail_link)return 0;auto v=f.view();
        const int ok=fs15_scene_owned_link_lod(&v,f.indices.at(parent),f.indices.at(child),position,1,&f.memory,nullptr,0,&f.used,&f.attached,&f.queue);return ok&&f.enqueue();
    }
    static int unlink(void*,void* parent,uint32_t slot,int suppress) {
        auto& n=*static_cast<FS15SceneStateRecord*>(parent);if(!suppress||n.vtable!=0x1404c7dd0)return 0;
        const size_t count=(n.children_end-n.children_begin)/8;if(slot>=count)return 0;auto* children=reinterpret_cast<uintptr_t*>(n.children_begin);
        reinterpret_cast<FS15SceneStateRecord*>(children[slot])->parent=0;
        for(size_t i=slot;i+1<count;++i)children[i]=children[i+1];n.children_end-=8;return 1;
    }
    static int destroy(void* p,void* child) {
        auto& f=memory_owner(p);auto* n=static_cast<FS15SceneStateRecord*>(child);
        if(f.kinds[f.indices.at(n)]==2)return fs15_scene_lod_destruct(static_cast<FS15SceneLodRecord*>(child),sizeof(FS15SceneLodRecord),1,&f.lod,&f.lod_services);
        return fs15_scene_destruct(n,sizeof(*n),FS15_OBJECT_TRS,1,&f.memory);
    }
    bool begin(std::initializer_list<std::pair<const char*,const char*>> pairs,FS15SceneGroupResult* output=nullptr) {
        std::vector<FS15SceneXmlAttribute> attrs;
        for(const auto& p:pairs)attrs.push_back({p.first,std::strlen(p.first)+1,p.second,p.second?std::strlen(p.second)+1:0});
        FS15SceneGroupResult result{};const bool ok=fs15_scene_group_begin(&current,attrs.data(),attrs.size(),&group_services,&result)!=0;if(output)*output=result;return ok;
    }
    void end(){require(fs15_scene_group_end(&current),"Close group callback");}
    void flush(){for(uintptr_t p=lod.pending.begin;p<lod.pending.end;p+=8)require(fs15_scene_lod_update(*reinterpret_cast<FS15SceneLodRecord**>(p),&lod,&lod_services),"Process loaded LOD");lod.pending.end=lod.pending.begin;}
    static FS15SceneStateRecord* resolve(void* p,uint32_t id){auto& f=*static_cast<Fixture*>(p);const auto found=f.ids.find(id);return found==f.ids.end()?nullptr:found->second;}
    static void loaded(void* p,FS15CharacterSet* set,uint32_t diagnostic) {
        auto& f=*static_cast<Fixture*>(p);f.last=set;f.diagnostic=diagnostic;++f.groups;
        if(set&&!set->references){uint32_t id;std::memcpy(&id,set->object_header+0x18,4);f.orphan_sets.emplace_back(id,set);}
    }
    FS15AnimationXmlServices animation_xml(){return {{this,&memory,&context,resolve,loaded},duration,integer};}
    FS15AnimationServices animation(){return {this,refresh,matrix,propagate};}
    void close() {
        flush();std::vector<FS15SceneStateRecord*> roots;for(auto* n:nodes)if(!n->parent)roots.push_back(n);
        for(auto* n:roots)require(destroy(static_cast<Owner*>(this),n),"Destroy callback-created forest");node=nullptr;current=nullptr;
        for(const auto& orphan:orphan_sets)if(registry.count(orphan.first))require(fs15_character_set_destruct(orphan.second,sizeof(FS15CharacterSet),1,&context,&memory),"Release zero-node animation set");
        require(!lod.manager.count&&lod.pending.begin==lod.pending.end&&context.active.begin==context.active.end&&context.pending.begin==context.pending.end,"All registered updates removed");
        release(this,lod.manager.entries);release(this,reinterpret_cast<void*>(lod.pending.begin));
        release(this,reinterpret_cast<void*>(context.active.begin));release(this,reinterpret_cast<void*>(context.pending.begin));
        lod={};context={};last=nullptr;nodes.clear();kinds.clear();indices.clear();ids.clear();orphan_sets.clear();
        require(blocks.empty()&&registry.empty(),"No callback-created resource leak");
    }
};
}
