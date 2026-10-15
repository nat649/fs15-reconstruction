// Recovered native attribute storage, RB insertion, Lua frames and cleanup.
#include "scene_attributes.h"
#include <cmath>
#include <cstring>
#include <limits>
#include <set>
#include <stdexcept>
#include <vector>

namespace {
using Node=FS15SceneAttributeNode;using Value=FS15SceneAttributeValue;
template<class T>T read(const void* p,size_t at){T value;std::memcpy(&value,static_cast<const uint8_t*>(p)+at,sizeof(value));return value;}
template<class T>void write(void* p,size_t at,T value){std::memcpy(static_cast<uint8_t*>(p)+at,&value,sizeof(value));}
size_t length(const char* text,size_t bytes) {
    if(!text||!bytes)throw std::runtime_error("Missing bounded string");
    const auto* end=static_cast<const char*>(std::memchr(text,0,bytes));
    if(!end)throw std::runtime_error("Unterminated attribute string");
    return static_cast<size_t>(end-text);
}
const char* key(const Node* n) {
    if(n->key_capacity<=15)return reinterpret_cast<const char*>(n->key_storage);
    const char* text;std::memcpy(&text,n->key_storage,sizeof(text));return text;
}
int compare(const char* a,size_t na,const char* b,size_t nb) {
    const int order=std::memcmp(a,b,na<nb?na:nb);
    return order?order:na<nb?-1:na>nb?1:0;
}
struct Map {
    FS15SceneStateRecord& record;Node* head;
    static FS15SceneStateRecord& owner(void* object,size_t bytes) {
        if(!object||bytes<0x90)throw std::runtime_error("Truncated attribute owner");
        return *static_cast<FS15SceneStateRecord*>(object);
    }
    Map(void* object,size_t bytes):record(owner(object,bytes)),head(nullptr) {
        head=reinterpret_cast<Node*>(record.attribute_head);
        if(!head||!head->nil||head->color!=1||!head->left||!head->parent||!head->right)throw std::runtime_error("Invalid attribute sentinel");
    }
    Node* find(const char* text,size_t size)const {
        Node* cursor=head->parent;size_t depth=0;
        while(cursor!=head) {
            if(!cursor||cursor->nil||++depth>256)throw std::runtime_error("Invalid attribute search path");
            const int order=compare(text,size,key(cursor),cursor->key_length);
            if(!order)return cursor;
            cursor=order<0?cursor->left:cursor->right;
        }
        return nullptr;
    }
    void rotate(Node* node,bool left) {
        Node* pivot=left?node->right:node->left;
        Node* inner=left?pivot->left:pivot->right;
        if(left)node->right=inner;else node->left=inner;
        if(!inner->nil)inner->parent=node;
        pivot->parent=node->parent;
        if(node==head->parent)head->parent=pivot;
        else if(node==node->parent->left)node->parent->left=pivot;
        else node->parent->right=pivot;
        if(left)pivot->left=node;else pivot->right=node;
        node->parent=pivot;
    }
    void insert(Node* node) {
        Node* parent=head;Node* cursor=head->parent;bool left=true;
        while(cursor!=head) {
            parent=cursor;left=compare(key(node),node->key_length,key(cursor),cursor->key_length)<0;
            cursor=left?cursor->left:cursor->right;
        }
        node->parent=parent;++record.attribute_count;
        if(parent==head)head->parent=head->left=head->right=node;
        else if(left){parent->left=node;if(parent==head->left)head->left=node;}
        else {parent->right=node;if(parent==head->right)head->right=node;}
        while(node->parent->color==0) {
            Node* p=node->parent;Node* grand=p->parent;const bool parent_left=p==grand->left;
            Node* uncle=parent_left?grand->right:grand->left;
            if(uncle->color==0) {p->color=uncle->color=1;grand->color=0;node=grand;}
            else {
                if(node==(parent_left?p->right:p->left)){node=p;rotate(node,parent_left);}
                node->parent->color=1;node->parent->parent->color=0;rotate(node->parent->parent,!parent_left);
            }
        }
        head->parent->color=1;
    }
    unsigned validate(Node* cursor,Node* parent,std::set<Node*>& seen,std::vector<Node*>& ordered,size_t depth)const {
        if(cursor==head)return 1;
        if(!cursor||cursor->nil||cursor->color>1||cursor->parent!=parent||depth>=256||!seen.insert(cursor).second||
            cursor->key_length>cursor->key_capacity||!key(cursor)||key(cursor)[cursor->key_length]!=0||!cursor->left||!cursor->right)
            throw std::runtime_error("Invalid owned attribute tree");
        if(cursor->color==0&&(cursor->left->color==0||cursor->right->color==0))throw std::runtime_error("Invalid RB colors");
        const unsigned a=validate(cursor->left,cursor,seen,ordered,depth+1);
        if(!ordered.empty()&&compare(key(ordered.back()),ordered.back()->key_length,key(cursor),cursor->key_length)>=0)
            throw std::runtime_error("Unordered attribute keys");
        ordered.push_back(cursor);
        const unsigned b=validate(cursor->right,cursor,seen,ordered,depth+1);
        if(a!=b)throw std::runtime_error("Invalid RB height");
        return a+(cursor->color==1);
    }
    std::vector<Node*> ordered()const {
        std::set<Node*> seen;std::vector<Node*> result;
        if(head->parent->color!=1)throw std::runtime_error("Nonblack root");
        validate(head->parent,head,seen,result,0);
        if(result.size()!=record.attribute_count||head->left!=(result.empty()?head:result.front())||head->right!=(result.empty()?head:result.back()))
            throw std::runtime_error("Attribute count/extrema mismatch");
        return result;
    }
};
bool allocator(const FS15SceneLifecycleServices* s){return s&&s->allocate&&s->deallocate;}
Value* ensure(Map& map,const char* text,size_t size,uint32_t kind,const FS15SceneLifecycleServices& s) {
    if(Node* found=map.find(text,size))return found->value;
    auto* value=static_cast<Value*>(s.allocate(s.user,sizeof(Value)));if(!value)return nullptr;
    value->bits=0;value->kind=kind;
    auto* node=static_cast<Node*>(s.allocate(s.user,sizeof(Node)));
    if(!node){s.deallocate(s.user,value);return nullptr;}
    node->left=node->parent=node->right=map.head;node->color=node->nil=0;
    node->key_storage[0]=0;node->key_length=0;node->key_capacity=15;
    if(size>15) {
        if(size>std::numeric_limits<size_t>::max()-16){s.deallocate(s.user,node);s.deallocate(s.user,value);return nullptr;}
        node->key_capacity=size|size_t(15);
        auto* buffer=static_cast<char*>(s.allocate(s.user,node->key_capacity+1));
        if(!buffer){s.deallocate(s.user,node);s.deallocate(s.user,value);return nullptr;}
        std::memcpy(node->key_storage,&buffer,sizeof(buffer));
    }
    std::memcpy(const_cast<char*>(key(node)),text,size+1);node->key_length=size;node->value=value;
    map.insert(node);return value;
}
void free_nodes(Node* cursor,Node* head,const FS15SceneLifecycleServices& s) {
    while(cursor!=head) {
        free_nodes(cursor->right,head,s);Node* next=cursor->left;
        if(cursor->key_capacity>15)s.deallocate(s.user,const_cast<char*>(key(cursor)));
        cursor->key_capacity=15;cursor->key_length=0;cursor->key_storage[0]=0;
        s.deallocate(s.user,cursor);cursor=next;
    }
}
}
extern "C" FS15_CORE_API int fs15_scene_attribute_find(const void* object,size_t bytes,
    const char* text,size_t text_bytes,Value** out) {
    if(!out)return 0;
    try {Map map(const_cast<void*>(object),bytes);Node* node=map.find(text,length(text,text_bytes));*out=node?node->value:nullptr;return 1;}catch(...){return 0;}
}
extern "C" FS15_CORE_API int fs15_scene_attribute_ensure(void* object,size_t bytes,
    const char* text,size_t text_bytes,uint32_t kind,const FS15SceneLifecycleServices* s,Value** out) {
    if(!out||!allocator(s))return 0;
    try {Map map(object,bytes);map.ordered();Value* value=ensure(map,text,length(text,text_bytes),kind,*s);if(!value)return 0;*out=value;return 1;}catch(...){return 0;}
}
extern "C" FS15_CORE_API int fs15_scene_attributes_validate(const void* object,size_t bytes) {
    try {Map map(const_cast<void*>(object),bytes);map.ordered();return 1;}catch(...){return 0;}
}
extern "C" FS15_CORE_API int fs15_scene_attributes_clear(void* object,size_t bytes,int release_header,const FS15SceneLifecycleServices* s) {
    if(!allocator(s))return 0;
    try {
        Map map(object,bytes);const auto ordered=map.ordered();
        for(Node* node:ordered)if(Value* value=node->value) {
            if(value->kind==3){s->deallocate(s->user,reinterpret_cast<void*>(static_cast<uintptr_t>(value->bits)));value->bits=0;}
            s->deallocate(s->user,value);
        }
        free_nodes(map.head->parent,map.head,*s);
        map.head->parent=map.head->left=map.head->right=map.head;map.record.attribute_count=0;
        if(release_header)s->deallocate(s->user,map.head);
        return 1;
    }catch(...){return 0;}
}
extern "C" FS15_CORE_API int fs15_scene_attribute_frame(void* object,size_t bytes,unsigned operation,
    const FS15SceneAttributeStrings* strings,void* frame,size_t frame_bytes,const FS15SceneLifecycleServices* s) {
    if(operation>1)return 0;
    if(operation==1&&!object)return 1;
    if(!frame||frame_bytes<(operation?0x2a:0xfa))return 0;
    if(!object){write<uint16_t>(frame,0xf8,0);return 1;}
    if(!strings||read<uintptr_t>(frame,0)!=reinterpret_cast<uintptr_t>(strings->key))return 0;
    try {
        Map map(object,bytes);const size_t key_length=length(strings->key,strings->key_bytes);
        if(!operation) {
            Node* found=map.find(strings->key,key_length);Value* value=found?found->value:nullptr;
            uint16_t tag=0;size_t size=0;
            if(value) {
                if(value->kind==0){tag=2;size=1;}else if(value->kind==1){tag=1;size=4;}
                else if(value->kind==2){tag=3;size=4;}else if(value->kind==3||value->kind==4){tag=4;size=8;}
            }
            if(size)std::memcpy(static_cast<uint8_t*>(frame)+0xf0,&value->bits,size);
            write(frame,0xf8,tag);return 1;
        }
        if(!allocator(s)||read<uintptr_t>(frame,0x10)!=reinterpret_cast<uintptr_t>(strings->type))return 0;
        const size_t type_length=length(strings->type,strings->type_bytes);
        uint32_t kind=99;
        const char* names[]={"Boolean","Integer","Float","String"};
        for(uint32_t i=0;i<4;++i)if(type_length==std::strlen(names[i])&&!std::memcmp(strings->type,names[i],type_length)){kind=i;break;}
        if(kind==99)return 1;
        size_t string_length=0;
        if(kind==3) {
            if(read<uintptr_t>(frame,0x20)!=reinterpret_cast<uintptr_t>(strings->value))return 0;
            string_length=length(strings->value,strings->value_bytes);
            if(Node* previous=map.find(strings->key,key_length))if(previous->value&&(previous->value->kind==3||previous->value->kind==4)) {
                const auto old=static_cast<uintptr_t>(previous->value->bits),input=reinterpret_cast<uintptr_t>(strings->value);
                if(old&&input>=old&&input-old<=std::strlen(reinterpret_cast<const char*>(old)))return 0;
            }
        }
        map.ordered();Value* value=ensure(map,strings->key,key_length,kind,*s);if(!value)return 0;
        if(kind==3) {
            if(value->kind==3||value->kind==4)s->deallocate(s->user,reinterpret_cast<void*>(static_cast<uintptr_t>(value->bits)));
            auto* copied=s->allocate(s->user,string_length+1);
            if(!copied){value->bits=0;value->kind=3;return 0;}
            std::memcpy(copied,strings->value,string_length+1);value->bits=reinterpret_cast<uintptr_t>(copied);
        }else if(kind==0)std::memcpy(&value->bits,static_cast<uint8_t*>(frame)+0x20,1);
        else if(kind==1&&read<uint16_t>(frame,0x28)!=1) {
            const float input=read<float>(frame,0x20);
            const int32_t result=std::isfinite(input)&&input>=-2147483648.0f&&input<2147483648.0f?static_cast<int32_t>(input):std::numeric_limits<int32_t>::min();
            std::memcpy(&value->bits,&result,4);
        }else std::memcpy(&value->bits,static_cast<uint8_t*>(frame)+0x20,4);
        value->kind=kind;return 1;
    }catch(...){return 0;}
}
