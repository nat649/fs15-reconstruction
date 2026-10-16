// Native pointer remap RB map and recursive supported scene-clone dispatcher.
#include "scene_clone_tree.h"
#include "scene_owned.h"
#include <set>
#include <stdexcept>
#include <vector>
namespace {
using Map=FS15SceneCloneMap;using Node=FS15SceneCloneMapNode;
constexpr size_t limit=1048576;
bool memory(const FS15SceneLifecycleServices* s){return s&&s->allocate&&s->deallocate;}
unsigned visit(const Map& m,Node* n,Node* parent,std::set<Node*>& seen,std::vector<Node*>& ordered,unsigned depth) {
    if(n==m.head)return 1;
    if(!n||n->nil||n->color>1||n->parent!=parent||!n->left||!n->right||depth>=256||seen.size()>=limit||!seen.insert(n).second)throw std::runtime_error("Invalid clone map node");
    if(!n->color&&(!n->left->color||!n->right->color))throw std::runtime_error("Invalid clone map red edge");
    const unsigned left=visit(m,n->left,n,seen,ordered,depth+1);
    if(!ordered.empty()&&ordered.back()->source>=n->source)throw std::runtime_error("Unordered clone map");
    ordered.push_back(n);const unsigned right=visit(m,n->right,n,seen,ordered,depth+1);
    if(left!=right)throw std::runtime_error("Invalid clone map black height");
    return left+(n->color==1);
}
void validate(const Map& m) {
    if(!m.head||!m.head->nil||m.head->color!=1||!m.head->left||!m.head->parent||!m.head->right||m.count>limit||m.head->parent->color!=1)throw std::runtime_error("Invalid clone map header");
    std::set<Node*> seen;std::vector<Node*> ordered;visit(m,m.head->parent,m.head,seen,ordered,0);
    if(ordered.size()!=m.count||m.head->left!=(ordered.empty()?m.head:ordered.front())||m.head->right!=(ordered.empty()?m.head:ordered.back()))throw std::runtime_error("Invalid clone map count/extrema");
}
Node* find(const Map& m,uintptr_t source) {
    Node* n=m.head->parent;while(n!=m.head){if(n->source==source)return n;n=source<n->source?n->left:n->right;}return nullptr;
}
void rotate(Map& m,Node* n,bool left) {
    Node* p=left?n->right:n->left;Node* inner=left?p->left:p->right;
    if(left)n->right=inner;else n->left=inner;
    if(!inner->nil)inner->parent=n;
    p->parent=n->parent;
    if(n==m.head->parent)m.head->parent=p;
    else if(n==n->parent->left)n->parent->left=p;else n->parent->right=p;
    if(left)p->left=n;else p->right=n;
    n->parent=p;
}
void insert(Map& m,Node* n) {
    Node* parent=m.head;Node* cursor=parent->parent;bool left=true;
    while(cursor!=m.head){parent=cursor;left=n->source<cursor->source;cursor=left?cursor->left:cursor->right;}
    n->parent=parent;++m.count;
    if(parent==m.head)m.head->parent=m.head->left=m.head->right=n;
    else if(left){parent->left=n;if(parent==m.head->left)m.head->left=n;}
    else {parent->right=n;if(parent==m.head->right)m.head->right=n;}
    while(n->parent->color==0) {
        Node* p=n->parent;Node* grand=p->parent;const bool parent_left=p==grand->left;Node* uncle=parent_left?grand->right:grand->left;
        if(uncle->color==0){p->color=uncle->color=1;grand->color=0;n=grand;}
        else {if(n==(parent_left?p->right:p->left)){n=p;rotate(m,n,parent_left);}n->parent->color=1;n->parent->parent->color=0;rotate(m,n->parent->parent,!parent_left);}
    }
    m.head->parent->color=1;
}
void release_nodes(Node* n,Node* head,const FS15SceneLifecycleServices& s) {
    while(n!=head){release_nodes(n->right,head,s);Node* left=n->left;s.deallocate(s.user,n);n=left;}
}
struct Walk {
    Map& map;FS15SceneVector& created;const FS15SceneCloneTreeServices& services;FS15SceneGroupResult& unfinished;size_t calls=0;
    int run(FS15SceneStateRecord* source,unsigned depth,FS15SceneGroupResult& result) {
        result={nullptr,0,0};if(!source||++calls>65535)return 0;
        if(find(map,reinterpret_cast<uintptr_t>(source)))return 1;
        if(depth>=256)return 0;
        const uint32_t flags=source->type_flags;
        if(flags&0x8000u){if(source->vtable!=0x1404c1078)return 0;}
        else if(flags&0x800u)return 0; // Shape clone dispatch remains open.
        else if(flags&0x2000u){if(source->vtable!=0x1404c1d98)return 0;}
        else if(flags&0x4000u)return 0; // External/reference resource dispatch remains open.
        else if(flags&0x80u){if(source->vtable!=0x1404c1c48)return 0;}
        else if(!(flags&0x40u))return 1;
        else if(source->vtable!=0x1404c5d98)return 0;
        const uintptr_t begin=source->children_begin,end=source->children_end,cap=source->children_capacity;
        if(end<begin||cap<end||(end-begin)%8||(cap-begin)%8||(!begin&&(end||cap))||(cap-begin)/8>limit)return 0;
        if(!fs15_scene_clone_node(source,&services.clone,&result)){unfinished=result;return 0;}
        Node* candidate=nullptr;const auto* m=services.clone.scene.memory;
        if(!fs15_scene_clone_map_node(&map,reinterpret_cast<uintptr_t>(source),reinterpret_cast<uintptr_t>(result.node),m,&candidate)){unfinished=result;return 0;}
        Node* entry=nullptr;int inserted=0;
        if(!fs15_scene_clone_map_insert(&map,candidate,m,&entry,&inserted)){m->deallocate(m->user,candidate);unfinished=result;return 0;}
        if(!fs15_scene_clone_vector_grow(&created,m)){unfinished=result;return 0;}
        *reinterpret_cast<uintptr_t*>(created.end)=reinterpret_cast<uintptr_t>(result.node);created.end+=8;
        const size_t count=(end-begin)/8;
        for(size_t i=0;i<count;++i) {
            auto* child=reinterpret_cast<FS15SceneStateRecord*>(reinterpret_cast<const uintptr_t*>(begin)[i]);
            if(!child||!(child->type_flags&0x40u))continue;
            FS15SceneGroupResult cloned{};if(!run(child,depth+1,cloned))return 0;
            if(cloned.node&&!services.link(services.clone.scene.user,result.node,cloned.node,UINT32_MAX)){unfinished=cloned;return 0;}
        }
        return 1;
    }
};
}
extern "C" int fs15_scene_clone_map_init(Map* map,const FS15SceneLifecycleServices* s) {
    if(!map||map->head||map->count||!memory(s))return 0;
    auto* head=static_cast<Node*>(s->allocate(s->user,sizeof(Node)));if(!head)return 0;
    head->left=head->parent=head->right=head;head->color=head->nil=1;map->head=head;return 1;
}
extern "C" int fs15_scene_clone_map_clear(Map* map,int release_header,const FS15SceneLifecycleServices* s) {
    if(!map||!memory(s))return 0;
    try {validate(*map);release_nodes(map->head->parent,map->head,*s);map->head->left=map->head->parent=map->head->right=map->head;map->count=0;if(release_header){s->deallocate(s->user,map->head);map->head=nullptr;}return 1;}catch(...){return 0;}
}
extern "C" int fs15_scene_clone_map_find(const Map* map,uintptr_t source,Node** out) {
    if(!map||!out)return 0;
    try {validate(*map);*out=find(*map,source);return 1;}catch(...){return 0;}
}
extern "C" int fs15_scene_clone_map_node(Map* map,uintptr_t source,uintptr_t target,const FS15SceneLifecycleServices* s,Node** out) {
    if(!map||!out||!memory(s))return 0;
    try {validate(*map);auto* n=static_cast<Node*>(s->allocate(s->user,sizeof(Node)));if(!n)return 0;n->left=n->parent=n->right=map->head;n->color=n->nil=0;n->source=source;n->target=target;*out=n;return 1;}catch(...){return 0;}
}
extern "C" int fs15_scene_clone_map_insert(Map* map,Node* node,const FS15SceneLifecycleServices* s,Node** out,int* inserted) {
    if(!map||!node||!memory(s)||!out||!inserted)return 0;
    try {
        validate(*map);if(node==map->head||node->nil||node->color||node->left!=map->head||node->right!=map->head||node->parent!=map->head)return 0;
        if(Node* found=find(*map,node->source)){s->deallocate(s->user,node);*out=found;*inserted=0;return 1;}
        if(map->count>=limit)return 0;
        insert(*map,node);*out=node;*inserted=1;return 1;
    }catch(...){return 0;}
}
extern "C" int fs15_scene_clone_vector_reserve(FS15SceneVector* v,size_t capacity,const FS15SceneLifecycleServices* s){return fs15_scene_pointer_reserve(v,capacity,s);}
extern "C" int fs15_scene_clone_vector_grow(FS15SceneVector* v,const FS15SceneLifecycleServices* s){return fs15_scene_pointer_grow(v,s);}
extern "C" int fs15_scene_clone_tree(FS15SceneStateRecord* source,Map* map,FS15SceneVector* created,const FS15SceneCloneTreeServices* s,FS15SceneGroupResult* result,FS15SceneGroupResult* unfinished) {
    if(!map||!created||!s||!s->link||!result||!unfinished||result==unfinished||!memory(s->clone.scene.memory))return 0;
    *result={nullptr,0,0};*unfinished={nullptr,0,0};
    try {validate(*map);if(!fs15_scene_clone_vector_reserve(created,0,s->clone.scene.memory))return 0;Walk work{*map,*created,*s,*unfinished};return work.run(source,0,*result);}catch(...){return 0;}
}
