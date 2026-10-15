// Authored invariant/ownership checks; native differential proof is separate.
#include "scene_attributes.h"
#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
namespace {
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
template<class T>void put(void* p,size_t at,T value){std::memcpy(static_cast<uint8_t*>(p)+at,&value,sizeof(value));}
struct Fixture {
    std::map<void*,size_t> blocks;
    uint32_t id=0;size_t registrations=0;int fail_after=-1;
    FS15SceneStateRecord node{};FS15SceneLifecycleServices services{};
    Fixture() {
        services={this,allocate,release,next,registered,unregistered,nullptr,nullptr};
        require(fs15_scene_construct(&node,sizeof(node),FS15_OBJECT_TRS,"fixture",8,0,1,&services),"Construct attribute fixture");
    }
    ~Fixture(){for(const auto& block:blocks)std::free(block.first);}
    static void* allocate(void* u,size_t size) {
        auto& f=*static_cast<Fixture*>(u);if(f.fail_after==0)return nullptr;if(f.fail_after>0)--f.fail_after;
        void* p=std::malloc(size);if(p){std::memset(p,0xa5,size);f.blocks.emplace(p,size);}return p;
    }
    static void release(void* u,void* p){if(p){auto& f=*static_cast<Fixture*>(u);if(!f.blocks.erase(p))std::abort();std::free(p);}}
    static uint32_t next(void* u){return ++static_cast<Fixture*>(u)->id;}
    static void registered(void* u,uint32_t,void*){++static_cast<Fixture*>(u)->registrations;}
    static void unregistered(void* u,uint32_t,void*){--static_cast<Fixture*>(u)->registrations;}
    FS15SceneAttributeValue* find(const std::string& key) {
        FS15SceneAttributeValue* value=nullptr;
        require(fs15_scene_attribute_find(&node,sizeof(node),key.c_str(),key.size()+1,&value),"Find attribute");return value;
    }
    void set(const std::string& key,const char* type,uint64_t bits,uint16_t tag=1,const char* text=nullptr) {
        std::array<uint8_t,0x110> frame{};put(frame.data(),0,reinterpret_cast<uintptr_t>(key.c_str()));
        put(frame.data(),0x10,reinterpret_cast<uintptr_t>(type));put(frame.data(),0x20,text?reinterpret_cast<uintptr_t>(text):bits);put(frame.data(),0x28,tag);
        FS15SceneAttributeStrings strings{key.c_str(),key.size()+1,type,std::strlen(type)+1,text,text?std::strlen(text)+1:0};
        require(fs15_scene_attribute_frame(&node,sizeof(node),1,&strings,frame.data(),frame.size(),&services),"Set attribute frame");
    }
};
void run() {
    Fixture f;f.set("value","Boolean",0x7f);require(f.find("value")->kind==0&&(f.find("value")->bits&255)==127,"Native Boolean preserves its byte");
    f.set("value","Integer",0x7fc00000,3);require(uint32_t(f.find("value")->bits)==0x80000000,"NaN CVTTSS2SI yields INT_MIN");
    f.set("value","Integer",0x40600000,3);require(uint32_t(f.find("value")->bits)==3,"Float-to-integer truncates toward zero");
    f.set("text","String",0,4,"harvester");const auto old=f.find("text")->bits;
    f.set("text","Integer",9);require(f.blocks.count(reinterpret_cast<void*>(old))&&uint32_t(f.find("text")->bits)==9,"Preserve native retained String-to-scalar allocation");
    f.set("value","Unsupported",3);require(uint32_t(f.find("value")->bits)==3,"Unknown type leaves value unchanged");
    f.set("Value","Float",0x80000000);require(f.find("Value")!=f.find("value")&&uint32_t(f.find("Value")->bits)==0x80000000,"Case-sensitive keys and signed-zero float bits");
    for(unsigned i=0;i<500;++i)f.set("ordered-key-"+std::to_string(i),"Integer",i);
    require(f.node.attribute_count==503&&fs15_scene_attributes_validate(&f.node,sizeof(f.node)),"Ascending key insertion yields a valid balanced tree");
    FS15SceneAttributeValue* found=nullptr;
    require(fs15_scene_attribute_ensure(&f.node,sizeof(f.node),"value",6,99,&f.services,&found)&&found->kind==1,"Ensure existing retains original kind");
    std::array<uint8_t,0x110> frame;frame.fill(0xa5);const char* name="absent";put(frame.data(),0,reinterpret_cast<uintptr_t>(name));
    const auto before=frame;FS15SceneAttributeStrings view{name,7,nullptr,0,nullptr,0};
    require(fs15_scene_attribute_frame(&f.node,sizeof(f.node),0,&view,frame.data(),frame.size(),&f.services),"Missing getter");
    auto expected=before;expected[0xf8]=expected[0xf9]=0;
    require(frame==expected,"Nil getter clears only result tag");
    name="value";put(frame.data(),0,reinterpret_cast<uintptr_t>(name));view.key=name;view.key_bytes=6;
    require(fs15_scene_attribute_frame(&f.node,sizeof(f.node),0,&view,frame.data(),frame.size(),&f.services)&&frame[0xf0]==3&&frame[0xf8]==1&&frame[0xf4]==0xa5,"Integer getter writes only four value bytes and tag");
    require(!fs15_scene_attribute_frame(&f.node,sizeof(f.node),0,&view,frame.data(),0xf9,&f.services),"Reject truncated getter output");
    auto* head=reinterpret_cast<FS15SceneAttributeNode*>(f.node.attribute_head);const auto count=f.node.attribute_count;
    ++f.node.attribute_count;const size_t allocations=f.blocks.size();
    require(!fs15_scene_attributes_clear(&f.node,sizeof(f.node),0,&f.services)&&f.blocks.size()==allocations,"Malformed map rejected before freeing members");
    f.node.attribute_count=count;
    require(fs15_scene_attributes_clear(&f.node,sizeof(f.node),0,&f.services)&&f.node.attribute_count==0&&head->left==head&&head->parent==head&&head->right==head,"Clear retains native empty sentinel");
    f.set("after-clear","String",0,4,"again");
    require(fs15_scene_destruct(&f.node,sizeof(f.node),FS15_OBJECT_TRS,0,&f.services)&&f.registrations==0&&f.blocks.size()==1&&f.blocks.count(reinterpret_cast<void*>(old)),"Nonempty lifetime chain releases all members except observed orphan text");
    Fixture invalid;FS15SceneAttributeValue* output=nullptr;
    require(!fs15_scene_attribute_ensure(&invalid.node,sizeof(invalid.node),"bad",2,0,&invalid.services,&output),"Reject unterminated key");
    for(int failure=0;failure<3;++failure) {
        invalid.fail_after=failure;const auto initial=invalid.blocks.size();
        require(!fs15_scene_attribute_ensure(&invalid.node,sizeof(invalid.node),"a-long-key-needing-heap",23,0,&invalid.services,&output)&&invalid.node.attribute_count==0&&invalid.blocks.size()==initial,"Failed insertion releases staged allocations and preserves map");
    }
}
}
int main(){try{run();std::cout<<"Recovered native-layout attributes, frame and ownership invariants passed.\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
