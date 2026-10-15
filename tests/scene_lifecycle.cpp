// Authored ownership invariants; original-code comparisons are Windows-only.
#include "scene_lifecycle.h"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <initializer_list>
#include <stdexcept>
#include <vector>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Owner {
    std::map<void*,size_t> allocations;
    std::map<uint32_t,void*> registry;
    std::map<void*,unsigned> kinds;
    std::vector<uint32_t> deleted;
    std::vector<uint32_t> registration_flags;
    uint32_t sequence=0xfffffffe;
    int fail_after=-1;
    FS15SceneLifecycleServices services{};
    Owner() {
        services={this,allocate,release,next,registered,unregistered,unlink,destroy_child};
    }
    ~Owner(){for(const auto& entry:allocations)std::free(entry.first);}
    static Owner& self(void* user){return *static_cast<Owner*>(user);}
    static void* allocate(void* user,size_t size) {
        auto& owner=self(user);
        if(owner.fail_after==0)return nullptr;
        if(owner.fail_after>0)--owner.fail_after;
        void* p=std::malloc(size);
        if(p){std::memset(p,0xa5,size);owner.allocations.emplace(p,size);}
        return p;
    }
    static void release(void* user,void* pointer) {
        if(!pointer)return;
        auto& owner=self(user);
        if(!owner.allocations.erase(pointer))std::abort();
        std::free(pointer);
    }
    static uint32_t next(void* user){return ++self(user).sequence;}
    static void registered(void* user,uint32_t id,void* object) {
        auto& owner=self(user);owner.registry.emplace(id,object);
        owner.registration_flags.push_back(static_cast<FS15SceneStateRecord*>(object)->type_flags);
    }
    static void unregistered(void* user,uint32_t id,void*) {
        auto& owner=self(user);owner.registry.erase(id);owner.deleted.push_back(id);
    }
    static int unlink(void*,void* parent,uint32_t slot,int suppress) {
        auto& p=*static_cast<FS15SceneStateRecord*>(parent);
        if(!suppress||p.vtable!=0x1404c7dd0)return 0;
        auto* children=reinterpret_cast<uintptr_t*>(p.children_begin);
        const size_t count=(p.children_end-p.children_begin)/sizeof(uintptr_t);
        if(slot>=count)return 0;
        reinterpret_cast<FS15SceneStateRecord*>(children[slot])->parent=0;
        std::memmove(children+slot,children+slot+1,(count-slot-1)*sizeof(uintptr_t));
        p.children_end-=sizeof(uintptr_t);return 1;
    }
    static int destroy_child(void* user,void* object) {
        auto& owner=self(user);
        return fs15_scene_destruct(object,sizeof(FS15SceneStateRecord),owner.kinds.at(object),1,&owner.services);
    }
    FS15SceneStateRecord* create(unsigned kind,const char* name,uint32_t tag=42) {
        auto* object=static_cast<FS15SceneStateRecord*>(allocate(this,sizeof(FS15SceneStateRecord)));
        require(object&&fs15_scene_construct(object,sizeof(*object),kind,name,std::strlen(name)+1,tag,1,&services),"Create recovered object");
        kinds.emplace(object,kind);return object;
    }
    void children(FS15SceneStateRecord* parent,std::initializer_list<FS15SceneStateRecord*> list) {
        auto* slots=static_cast<uintptr_t*>(allocate(this,list.size()*sizeof(uintptr_t)));size_t at=0;
        for(auto* child:list){slots[at++]=reinterpret_cast<uintptr_t>(child);child->parent=reinterpret_cast<uintptr_t>(parent);}
        parent->children_begin=reinterpret_cast<uintptr_t>(slots);
        parent->children_end=parent->children_capacity=parent->children_begin+list.size()*sizeof(uintptr_t);
    }
};
void run() {
    Owner owner;auto* root=owner.create(FS15_OBJECT_TRS,"farm-root");
    require(root->type_flags==0x72&&root->flags==0x41,"Recovered TRS flags");
    require(root->local[0]==1&&root->local[5]==1&&root->local[10]==1&&root->local[15]==1,"Recovered identity local matrix");
    require(root->translation[0]==0&&root->rotation[2]==0&&root->scale[0]==1&&root->scale[2]==1,"Recovered initial TRS cache");
    require(root->local_clip[0]==1000000&&root->local_clip[1]==0&&root->local_masks[0]==0xffff&&root->local_masks[1]==0xffffffff,"Recovered clip and mask defaults");
    uint32_t opaque=0;std::memcpy(&opaque,root->world,4);
    require(opaque==0xa5a5a5a5,"Constructor preserves opaque lazy world cache bytes");
    require(root->class_tag==0x1d,"TRS constructor fixes class tag");
    auto* first=owner.create(FS15_OBJECT_MATRIX,"");auto* second=owner.create(FS15_OBJECT_TRS_TYPED,"tractor",77);
    auto* grandchild=owner.create(FS15_OBJECT_NODE,"attachment");
    require(owner.registry.count(0xffffffff)&&owner.registry.count(0)&&owner.registry.count(1)&&owner.registry.count(2),"Identifiers wrap as native uint32");
    for(uint32_t flags:owner.registration_flags)require(flags==0,"Registration occurs before derived type flags");
    require(second->class_tag==77,"Typed TRS constructor retains class tag");
    require(fs15_scene_rename(first,sizeof(*first),"",1,&owner.services)&&first->name&&*reinterpret_cast<char*>(first->name)==0,"Empty rename allocates a NUL string");
    require(fs15_scene_rename(second,sizeof(*second),"vehicle",8,&owner.services),"Rename existing object");
    auto* current=reinterpret_cast<char*>(second->name);
    require(!fs15_scene_rename(second,sizeof(*second),current+6,2,&owner.services)&&std::strcmp(current,"vehicle")==0,"Reject aliased rename before freeing");
    owner.children(root,{first,second});owner.children(second,{grandchild});
    require(fs15_scene_destruct(root,sizeof(*root),FS15_OBJECT_TRS,1,&owner.services),"Delete recovered owned forest");
    require(owner.deleted==std::vector<uint32_t>({0,2,1,0xffffffff}),"Children are deleted in order, descendants before parent");
    require(owner.registry.empty()&&owner.allocations.empty(),"All names, sentinels, properties, child vectors and objects released");

    Owner guards;auto* protected_node=guards.create(FS15_OBJECT_TRS,"protected");
    auto* props=reinterpret_cast<FS15ScenePropertyView*>(protected_node->properties);
    props->flags=0x1008;const auto count=guards.allocations.size();
    require(!fs15_scene_destruct(protected_node,sizeof(*protected_node),FS15_OBJECT_TRS,1,&guards.services)&&guards.allocations.size()==count&&guards.registry.size()==1,"Active-trigger teardown rejects before external frees");
    props->flags=0;auto* bytes=reinterpret_cast<uint8_t*>(protected_node);
    protected_node->attribute_count=1;
    require(!fs15_scene_destruct(protected_node,sizeof(*protected_node),FS15_OBJECT_TRS,1,&guards.services)&&guards.allocations.size()==count,"Nonempty attribute teardown rejects before frees");
    protected_node->attribute_count=0;
    const uint64_t original_property_table=props->unknown00;props->unknown00=123;
    require(!fs15_scene_destruct(protected_node,sizeof(*protected_node),FS15_OBJECT_TRS,1,&guards.services)&&guards.allocations.size()==count,"Unknown derived property destructor rejects before frees");
    props->unknown00=original_property_table;
    uintptr_t resource=123;std::memcpy(bytes+0x120,&resource,sizeof(resource));
    require(!fs15_scene_destruct(protected_node,sizeof(*protected_node),FS15_OBJECT_TRS,1,&guards.services)&&guards.allocations.size()==count,"Unrecovered shared resource rejects before frees");
    resource=0;std::memcpy(bytes+0x120,&resource,sizeof(resource));
    const auto snapshot=*protected_node;
    require(!fs15_scene_rename(protected_node,sizeof(*protected_node),"unterminated",5,&guards.services)&&std::memcmp(&snapshot,protected_node,sizeof(snapshot))==0,"Reject unterminated name before mutation");
    require(fs15_scene_destruct(protected_node,sizeof(*protected_node),FS15_OBJECT_TRS,1,&guards.services),"Delete after unsupported state cleared");

    for(int failure=0;failure<3;++failure) {
        Owner failing;FS15SceneStateRecord record;std::memset(&record,0xa5,sizeof(record));failing.fail_after=failure;
        require(!fs15_scene_construct(&record,sizeof(record),FS15_OBJECT_TRS,"name",5,0,1,&failing.services)&&failing.allocations.empty()&&failing.registry.empty(),"Allocation failure releases acquired members and registry entry");
    }
}
}
int main() {
    try {run();std::cout<<"Recovered scene object lifetime invariants passed; platform allocator/registry/dispatch are authored services.\n";return 0;}
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
