#include "scene_group_fixture.h"
#include "scene_clone.h"
#include "scene_attributes.h"
#include <iostream>
namespace {
using fs15_group_test::Fixture;
using fs15_group_test::require;
void text_attribute(Fixture& f,FS15SceneStateRecord* n,const char* key,const char* text,unsigned kind=3) {
    FS15SceneAttributeValue* v=nullptr;require(fs15_scene_attribute_ensure(n,sizeof(*n),key,std::strlen(key)+1,kind,&f.memory,&v),"Create clone attribute");
    auto* copied=f.memory.allocate(f.memory.user,std::strlen(text)+1);require(copied!=nullptr,"Allocate attribute text");std::memcpy(copied,text,std::strlen(text)+1);v->bits=reinterpret_cast<uintptr_t>(copied);
}
void run() {
    Fixture f;require(f.begin({{"name","source"},{"lodDistance","0 10 20 30"},{"translation","4 5 6"},{"rotation","0 45 0"},{"scale","2 1 3"},{"dynamic","true"},{"density","0.4"},{"visibility","false"},{"clipDistance","300"},{"nodeId","12"}}),"Load clone source through recovered group callback");
    auto* source=f.current;text_attribute(f,source,"very-long-source-attribute-key","payload");
    require(f.begin({{"name","child"}}),"Load source child");f.end();f.end();
    FS15SceneCloneServices services{f.group_services.common,Fixture::refresh,Fixture::constructed};FS15SceneGroupResult result{};
    require(fs15_scene_clone_node(source,&services,&result)&&result.kind==2&&result.bytes==0x188,"Clone native LOD wrapper");
    auto* copied=result.node;auto* lod=reinterpret_cast<FS15SceneLodRecord*>(copied);
    require(copied!=source&&copied->identifier!=source->identifier&&!copied->parent&&copied->children_begin==copied->children_end,"Clone gets fresh identity without copying child ownership");
    require(lod->distances[0]==0&&lod->distances[1]==10&&lod->distances[2]==20&&lod->distances[3]==20,"Retain original LOD clone truncation");
    auto* body=reinterpret_cast<FS15RigidBodyProperty*>(copied->properties);require(body->owner==copied&&!body->actor&&!body->manager&&body->density==0.4f&&!body->changes,"Clone detached body metadata");
    FS15SceneAttributeValue *a=nullptr,*b=nullptr;
    require(fs15_scene_attribute_find(source,sizeof(*source),"very-long-source-attribute-key",31,&a)&&fs15_scene_attribute_find(copied,sizeof(*copied),"very-long-source-attribute-key",31,&b)&&a&&b&&a!=b&&a->bits!=b->bits&&std::strcmp(reinterpret_cast<const char*>(b->bits),"payload")==0,"Attribute map and text are independent allocations");
    reinterpret_cast<char*>(a->bits)[0]='P';require(std::strcmp(reinterpret_cast<const char*>(b->bits),"payload")==0,"Changing source text leaves clone independent");
    f.flush();require(copied->translation[0]==4&&copied->local_clip[0]==300,"Clone common node values");
    require(fs15_scene_attributes_copy(copied,sizeof(*copied),copied,sizeof(*copied),&f.memory)&&!copied->attribute_count,"Native self-assignment clears attribute map");
    FS15SceneXmlAttribute camera[]={{"name",5,"view",5},{"fov",4,"75",3}};
    require(fs15_scene_camera_begin(&f.current,camera,2,&f.group_services,&result),"Load Camera clone source");auto* view=f.current;f.end();
    require(fs15_scene_clone_node(view,&services,&result)&&result.bytes==0x190&&reinterpret_cast<FS15SceneCameraRecord*>(result.node)->fov==75,"Clone Camera parameters through native wrapper");
    require(fs15_scene_trs_copy(copied,view,&services)&&copied->attribute_count==0&&copied->vtable==0x1404c1c48,"TRS copy preserves destination derived identity");f.close();
    Fixture retention;auto* source_node=retention.node;text_attribute(retention,source_node,"retained","kind-four",4);
    require(fs15_scene_attribute_find(source_node,sizeof(*source_node),"retained",9,&a)&&a,"Find retained source");
    FS15SceneAttributeValue* cloned=nullptr;require(fs15_scene_attribute_value_clone(a,sizeof(*a),&retention.memory,&cloned)&&cloned->kind==4&&cloned->bits!=a->bits,"Clone kind-four text independently");
    retention.memory.deallocate(retention.memory.user,reinterpret_cast<void*>(cloned->bits));retention.memory.deallocate(retention.memory.user,cloned);
    const auto retained=a->bits;require(fs15_scene_attributes_copy(source_node,sizeof(*source_node),source_node,sizeof(*source_node),&retention.memory),"Clear kind-four self-copy");
    require(retention.blocks.count(reinterpret_cast<void*>(retained))==1,"Native clear retains kind-four allocation");retention.memory.deallocate(retention.memory.user,reinterpret_cast<void*>(retained));retention.close();
    unsigned failed=0,succeeded=0;
    for(int limit=0;limit<24;++limit) {
        Fixture attempt;require(attempt.begin({{"name","source"}}),"Create failure source");auto* n=attempt.current;text_attribute(attempt,n,"key","value");attempt.end();
        FS15SceneCloneServices callbacks{attempt.group_services.common,Fixture::refresh,Fixture::constructed};attempt.fail_after=limit;
        if(fs15_scene_clone_node(n,&callbacks,&result))++succeeded;else ++failed;
        attempt.fail_after=-1;attempt.close();
    }
    require(failed>=7&&succeeded>0,"Allocation failures preserve cleanable clone prefixes");
}
}
int main(){try{run();std::cout<<"Scene clone checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
