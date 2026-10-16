#include "scene_group_fixture.h"
#include "scene_clone_tree.h"
#include <iostream>
namespace {
using fs15_group_test::Fixture;
using fs15_group_test::require;
FS15SceneCloneTreeServices callbacks(Fixture& f){return {{f.group_services.common,Fixture::refresh,Fixture::constructed},Fixture::link};}
void release(Fixture& f,FS15SceneCloneMap& map,FS15SceneVector& created){require(fs15_scene_clone_map_clear(&map,1,&f.memory),"Release clone remap entries");f.memory.deallocate(f.memory.user,reinterpret_cast<void*>(created.begin));created={};}
void run() {
    Fixture f;require(f.begin({{"name","source"},{"lodDistance","0 10 20 30"},{"translation","5 0 0"}}),"Load recursive source");auto* source=f.current;
    require(f.begin({{"name","branch"},{"translation","2 0 0"}}),"Load recursive branch");auto* branch=f.current;
    FS15SceneXmlAttribute attrs[]={{"name",5,"camera",7},{"fov",4,"70",3}};FS15SceneGroupResult single{};
    require(fs15_scene_camera_begin(&f.current,attrs,2,&f.group_services,&single),"Load recursive Camera");f.end();f.end();
    require(f.begin({{"name","skip"}}),"Load excluded branch");auto* skipped=f.current;skipped->type_flags&=~uint32_t(0x40);
    require(f.begin({{"name","skip-child"}}),"Load excluded descendant");f.end();f.end();f.end();
    FS15SceneCloneMap map{0xabcdef,nullptr,0};FS15SceneVector created{};auto services=callbacks(f);FS15SceneGroupResult result{},unfinished{};
    require(fs15_scene_clone_map_init(&map,&f.memory)&&map.unknown00==0xabcdef,"Initialize native-layout remap header");
    require(fs15_scene_clone_tree(source,&map,&created,&services,&result,&unfinished)&&result.node&&result.kind==2&&!unfinished.node&&map.count==3&&(created.end-created.begin)/8==3,"Recursively clone exactly the eligible subtree");
    auto** copies=reinterpret_cast<FS15SceneStateRecord**>(created.begin);
    require(copies[0]==result.node&&copies[1]->parent==reinterpret_cast<uintptr_t>(copies[0])&&copies[2]->parent==reinterpret_cast<uintptr_t>(copies[1])&&copies[2]->vtable==0x1404c1078,"Native pre-order and ordered child attachment");
    require(reinterpret_cast<FS15SceneCameraRecord*>(copies[2])->fov==70&&copies[0]->children_end-copies[0]->children_begin==8,"Preserve Camera tail and skip excluded subtree");
    FS15SceneCloneMapNode* entry=nullptr;require(fs15_scene_clone_map_find(&map,reinterpret_cast<uintptr_t>(branch),&entry)&&entry&&entry->target==reinterpret_cast<uintptr_t>(copies[1]),"Resolve original-to-clone reference");
    const auto blocks=f.blocks.size();require(fs15_scene_clone_tree(source,&map,&created,&services,&single,&unfinished)&&!single.node&&f.blocks.size()==blocks&&map.count==3,"Already-mapped input is null without allocation");
    f.flush();float position[3];require(Fixture::world(&f,copies[2],position)&&position[0]==7,"Recursive copy preserves hierarchical world position");
    release(f,map,created);
    // Unknown derived dispatch stops at its encounter; retained earlier clones
    // remain owned and can be cleaned instead of silently reinterpreting Shape.
    branch->type_flags|=0x800;require(fs15_scene_clone_map_init(&map,&f.memory),"Reinitialize remap");
    require(!fs15_scene_clone_tree(source,&map,&created,&services,&result,&unfinished)&&result.node&&map.count==1,"Unsupported derived branch retains the native-order root prefix");branch->type_flags&=~uint32_t(0x800);
    release(f,map,created);f.close();
    Fixture keys;FS15SceneCloneMap tree{};require(fs15_scene_clone_map_init(&tree,&keys.memory),"Initialize pointer-key map");
    for(uintptr_t key:{uintptr_t(0),UINTPTR_MAX,uintptr_t(1)<<63,uintptr_t(17),uintptr_t(3)}) {
        FS15SceneCloneMapNode* candidate=nullptr;int inserted=0;
        require(fs15_scene_clone_map_node(&tree,key,key^9,&keys.memory,&candidate)&&fs15_scene_clone_map_insert(&tree,candidate,&keys.memory,&entry,&inserted)&&inserted&&entry->source==key,"Insert unsigned pointer key");
        const auto allocations=keys.blocks.size();require(fs15_scene_clone_map_node(&tree,key,99,&keys.memory,&candidate)&&fs15_scene_clone_map_insert(&tree,candidate,&keys.memory,&entry,&inserted)&&!inserted&&entry->target==(key^9)&&keys.blocks.size()==allocations,"Duplicate consumes candidate without replacing target");
    }
    require(fs15_scene_clone_map_clear(&tree,0,&keys.memory)&&!tree.count&&tree.head&&tree.head->parent==tree.head,"Retain reusable map sentinel");
    tree.head->nil=0;require(!fs15_scene_clone_map_find(&tree,0,&entry),"Reject malformed map before lookup");tree.head->nil=1;
    require(fs15_scene_clone_map_clear(&tree,1,&keys.memory),"Release map sentinel");keys.close();
    unsigned failed=0,succeeded=0;
    for(int limit=0;limit<55;++limit) {
        Fixture attempt;require(attempt.begin({{"name","source"}})&&attempt.begin({{"name","child"},{"lodDistance","0 10"}}),"Build allocation-failure subtree");attempt.end();auto* root=attempt.current;attempt.end();
        FS15SceneCloneMap remap{};FS15SceneVector list{};require(fs15_scene_clone_map_init(&remap,&attempt.memory),"Create failure remap");auto seam=callbacks(attempt);attempt.fail_after=limit;
        if(fs15_scene_clone_tree(root,&remap,&list,&seam,&result,&unfinished))++succeeded;else ++failed;
        attempt.fail_after=-1;release(attempt,remap,list);attempt.close();
    }
    require(failed>12&&succeeded>0,"Cleanable prefixes across recursive allocation failures");
}
}
int main(){try{run();std::cout<<"Recursive scene clone checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
