// Native child-vector allocation with authored noncontiguous record transport.
#include "scene_owned.h"
#include "scene_derived.h"
#include "scene_graph.h"
#include <algorithm>
#include <cstring>
#include <limits>
#include <map>
#include <stdexcept>
#include <vector>
namespace {
constexpr size_t limit=1048576;
bool allocator(const FS15SceneLifecycleServices* s){return s&&s->allocate&&s->deallocate;}
size_t slots(uintptr_t begin,uintptr_t end,uintptr_t capacity) {
    if(end<begin||capacity<end||(end-begin)%8||(capacity-begin)%8||(!begin&&(end||capacity))||(capacity-begin)/8>limit)
        throw std::runtime_error("Invalid owned vector");
    return (capacity-begin)/8;
}
size_t checked(const FS15SceneVector& v){return slots(v.begin,v.end,v.capacity);}
size_t growth(size_t count,size_t capacity) {
    if(count>=limit)throw std::runtime_error("Owned vector limit");
    return std::max(count+1,capacity+capacity/2);
}
struct Staged {
    const FS15SceneOwnedView& source;
    std::map<uintptr_t,size_t> indices;
    std::vector<FS15SceneStateRecord> nodes;
    std::vector<FS15ScenePropertyView> properties;
    std::vector<std::vector<uintptr_t>> windows;
    std::vector<size_t> sizes,firsts,observer_firsts;
    std::vector<uintptr_t> children,observers;
    FS15SceneStateArena view{};std::vector<uint32_t> light_tokens;bool has_lights;
    explicit Staged(const FS15SceneOwnedView& v,bool lod=false,bool lights=false):source(v),has_lights(lights) {
        if(!v.nodes||!v.kinds||!v.count||v.count>65535||(v.special_root!=SIZE_MAX&&v.special_root>=v.count))throw std::runtime_error("Invalid owned forest");
        nodes.resize(v.count);properties.resize(v.count);windows.resize(v.count);sizes.resize(v.count);firsts.resize(v.count);observer_firsts.resize(v.count);
        if(lights)light_tokens.assign(v.count,0xffffffff);
        for(size_t i=0;i<v.count;++i) {
            if(!v.nodes[i]||v.kinds[i]>(lights?3:lod?2:1)||!indices.emplace(reinterpret_cast<uintptr_t>(v.nodes[i]),i).second)throw std::runtime_error("Duplicate/unsupported owned node");
            if(v.kinds[i]==3){if(v.nodes[i]->vtable!=0x1404c1d98)throw std::runtime_error("Invalid Light dispatch");std::memcpy(&light_tokens[i],reinterpret_cast<const uint8_t*>(v.nodes[i])+0x1d4,4);}
            nodes[i]=*v.nodes[i];if(!nodes[i].properties)throw std::runtime_error("Missing owned property");
            std::memcpy(&properties[i],reinterpret_cast<void*>(nodes[i].properties),sizeof(properties[i]));
        }
        size_t total=0;
        for(size_t i=0;i<v.count;++i) {
            auto& n=nodes[i];const size_t capacity=slots(n.children_begin,n.children_end,n.children_capacity);
            if(capacity>limit-total)throw std::runtime_error("Owned children limit");
            total+=capacity;sizes[i]=(n.children_end-n.children_begin)/8;
            if(capacity)windows[i].assign(reinterpret_cast<const uintptr_t*>(n.children_begin),reinterpret_cast<const uintptr_t*>(n.children_capacity));
            for(size_t j=0;j<sizes[i];++j)index(windows[i][j]);
            if(n.parent)n.parent=pointer(index(n.parent));
            const size_t observer_capacity=slots(n.observers_begin,n.observers_end,n.observers_capacity),observer_size=(n.observers_end-n.observers_begin)/8;
            if(observer_capacity>limit-observers.size())throw std::runtime_error("Owned observers limit");
            observer_firsts[i]=observers.size();
            if(observer_size)observers.insert(observers.end(),reinterpret_cast<const uintptr_t*>(n.observers_begin),reinterpret_cast<const uintptr_t*>(n.observers_end));
            n.properties=reinterpret_cast<uintptr_t>(&properties[i]);
        }
        flatten();
        // Validate full membership before asking an allocator to do anything.
        std::vector<size_t> parents(v.count,SIZE_MAX);
        for(size_t i=0;i<v.count;++i)for(size_t j=0;j<sizes[i];++j) {
            const size_t child=index(windows[i][j]);
            if(parents[child]!=SIZE_MAX||nodes[child].parent!=pointer(i))throw std::runtime_error("Inconsistent owned membership");
            parents[child]=i;
        }
        for(size_t i=0;i<v.count;++i) {
            if((nodes[i].parent?staged_index(nodes[i].parent):SIZE_MAX)!=parents[i])throw std::runtime_error("Missing owned child");
            size_t depth=0;
            for(size_t p=i;p!=SIZE_MAX;p=parents[p])if(++depth>256)throw std::runtime_error("Cyclic/deep owned forest");
        }
    }
    size_t index(uintptr_t p)const{const auto found=indices.find(p);if(found==indices.end())throw std::runtime_error("Node outside owned forest");return found->second;}
    uintptr_t pointer(size_t i)const{return reinterpret_cast<uintptr_t>(&nodes[i]);}
    size_t staged_index(uintptr_t p)const {
        const uintptr_t base=reinterpret_cast<uintptr_t>(nodes.data());
        if(p<base||(p-base)%sizeof(nodes[0])||(p-base)/sizeof(nodes[0])>=nodes.size())throw std::runtime_error("Invalid staged node");
        return (p-base)/sizeof(nodes[0]);
    }
    uintptr_t encode(uintptr_t p)const{const auto found=indices.find(p);return found==indices.end()?p:pointer(found->second);}
    uintptr_t decode(uintptr_t p)const {
        const uintptr_t base=reinterpret_cast<uintptr_t>(nodes.data());
        if(p>=base&&p-base<nodes.size()*sizeof(nodes[0])&&(p-base)%sizeof(nodes[0])==0)return reinterpret_cast<uintptr_t>(source.nodes[(p-base)/sizeof(nodes[0])]);
        return p;
    }
    void flatten() {
        children.clear();
        for(size_t i=0;i<nodes.size();++i){
            if(windows[i].size()>limit-children.size())throw std::runtime_error("Owned children limit");
            firsts[i]=children.size();
            for(size_t j=0;j<windows[i].size();++j)children.push_back(j<sizes[i]?encode(windows[i][j]):windows[i][j]);
        }
        for(size_t i=0;i<nodes.size();++i) {
            auto& n=nodes[i];const uintptr_t base=children.empty()?0:reinterpret_cast<uintptr_t>(children.data()+firsts[i]);
            n.children_begin=base;n.children_end=base+sizes[i]*8;n.children_capacity=base+windows[i].size()*8;
            const auto& original=*source.nodes[i];const size_t size=(original.observers_end-original.observers_begin)/8;
            const uintptr_t obs=observers.empty()?0:reinterpret_cast<uintptr_t>(observers.data()+observer_firsts[i]);
            n.observers_begin=obs;n.observers_end=obs+size*8;n.observers_capacity=n.observers_end;
        }
        view={nodes.data(),nodes.size()*sizeof(nodes[0]),sizeof(nodes[0]),children.data(),children.size(),observers.data(),observers.size(),properties.data(),properties.size(),source.kinds,source.count,source.special_root};
    }
    void commit(size_t replaced=SIZE_MAX,uintptr_t replacement=0) {
        for(size_t i=0;i<nodes.size();++i) {
            auto& n=nodes[i];const auto& original=*source.nodes[i];
            const size_t size=(n.children_end-n.children_begin)/8,capacity=windows[i].size();
            const uintptr_t destination=i==replaced?replacement:original.children_begin;
            for(size_t j=0;j<capacity;++j) {
                const uintptr_t value=children[firsts[i]+j];
                reinterpret_cast<uintptr_t*>(destination)[j]=j<std::max(size,sizes[i])?decode(value):value;
            }
            n.parent=decode(n.parent);n.children_begin=destination;n.children_end=destination+size*8;n.children_capacity=destination+capacity*8;
            n.properties=original.properties;n.observers_begin=original.observers_begin;n.observers_end=original.observers_end;n.observers_capacity=original.observers_capacity;
        }
        for(size_t i=0;i<nodes.size();++i){*source.nodes[i]=nodes[i];if(has_lights&&source.kinds[i]==3)std::memcpy(reinterpret_cast<uint8_t*>(source.nodes[i])+0x1d4,&light_tokens[i],4);}
    }
};
}
extern "C" int fs15_scene_pointer_reserve(FS15SceneVector* vector,size_t capacity,const FS15SceneLifecycleServices* s) {
    if(!vector||!allocator(s)||capacity>limit)return 0;
    try {
        const size_t old=checked(*vector),count=(vector->end-vector->begin)/8;if(capacity<=old)return 1;
        void* memory=s->allocate(s->user,capacity*8);if(!memory)return 0;
        if(count)std::memcpy(memory,reinterpret_cast<void*>(vector->begin),count*8);
        if(vector->begin)s->deallocate(s->user,reinterpret_cast<void*>(vector->begin));
        vector->begin=reinterpret_cast<uintptr_t>(memory);vector->end=vector->begin+count*8;vector->capacity=vector->begin+capacity*8;return 1;
    }catch(...){return 0;}
}
extern "C" int fs15_scene_pointer_grow(FS15SceneVector* vector,const FS15SceneLifecycleServices* s) {
    if(!vector||!allocator(s))return 0;
    try {const size_t capacity=checked(*vector),count=(vector->end-vector->begin)/8;return count<capacity?1:fs15_scene_pointer_reserve(vector,growth(count,capacity),s);}catch(...){return 0;}
}
extern "C" int fs15_scene_pointer_append(FS15SceneVector* vector,const uintptr_t* value,const FS15SceneLifecycleServices* s) {
    if(!vector||!value)return 0;
    try {
        checked(*vector);const uintptr_t input=reinterpret_cast<uintptr_t>(value);size_t alias=SIZE_MAX;
        if(input>=vector->begin&&input<vector->end)alias=(input-vector->begin)/8;
        // Original alias handling re-reads the corresponding relocated element.
        uintptr_t copied=0;
        if(input>=vector->end&&input<vector->capacity)return 0;
        if(!fs15_scene_pointer_grow(vector,s))return 0;
        if(alias!=SIZE_MAX)std::memcpy(&copied,reinterpret_cast<void*>(vector->begin+alias*8),8);
        else std::memcpy(&copied,value,8);
        std::memcpy(reinterpret_cast<void*>(vector->end),&copied,8);vector->end+=8;return 1;
    }catch(...){return 0;}
}
static int owned_state_impl(const FS15SceneOwnedView* view,size_t index,unsigned op,uint32_t mask,int32_t origin,FS15SceneNotification* events,size_t capacity,size_t* used,FS15SceneLodQueue* queue,FS15SceneLodQueue* lights,bool derived) {
    if(!view)return 0;
    try {Staged s(*view,queue!=nullptr,derived);FS15SceneDerivedState d{queue,lights,s.light_tokens.data(),s.light_tokens.size()};if(!(derived?fs15_scene_state_derived(&s.view,index,op,mask,origin,events,capacity,used,&d):fs15_scene_state_lod(&s.view,index,op,mask,origin,events,capacity,used,queue)))return 0;s.commit();return 1;}catch(...){return 0;}
}
static int owned_trs_impl(const FS15SceneOwnedView* view,size_t index,unsigned op,void* frame,size_t bytes,FS15SceneNotification* events,size_t capacity,size_t* used,int* warning,FS15SceneLodQueue* queue,FS15SceneLodQueue* lights,bool derived) {
    if(!view)return 0;
    try {Staged s(*view,queue!=nullptr,derived);FS15SceneDerivedState d{queue,lights,s.light_tokens.data(),s.light_tokens.size()};if(!(derived?fs15_scene_trs_derived(&s.view,index,op,frame,bytes,events,capacity,used,warning,&d):fs15_scene_trs_lod(&s.view,index,op,frame,bytes,events,capacity,used,warning,queue)))return 0;s.commit();return 1;}catch(...){return 0;}
}
static int owned_link_impl(const FS15SceneOwnedView* view,size_t parent,size_t child,uint32_t position,int matrix,const FS15SceneLifecycleServices* memory,FS15SceneNotification* events,size_t capacity,size_t* used,uint32_t* attached,FS15SceneLodQueue* queue,FS15SceneLodQueue* lights,bool derived) {
    if(!view||parent>=view->count||child>=view->count||parent==child||!allocator(memory))return 0;
    FS15SceneVector replacement{};
    try {
        Staged s(*view,queue!=nullptr,derived);FS15SceneDerivedState d{queue,lights,s.light_tokens.data(),s.light_tokens.size()};size_t changed=SIZE_MAX;
        if(view->nodes[child]->parent!=reinterpret_cast<uintptr_t>(view->nodes[parent])&&s.sizes[parent]==s.windows[parent].size()) {
            const size_t grown=growth(s.sizes[parent],s.windows[parent].size());
            if(!fs15_scene_pointer_reserve(&replacement,grown,memory))return 0;
            if(s.sizes[parent])std::memcpy(reinterpret_cast<void*>(replacement.begin),reinterpret_cast<void*>(view->nodes[parent]->children_begin),s.sizes[parent]*8);
            s.windows[parent].assign(reinterpret_cast<const uintptr_t*>(replacement.begin),reinterpret_cast<const uintptr_t*>(replacement.capacity));s.flatten();changed=parent;
        }
        if(!(derived?fs15_scene_link_derived(&s.view,s.children.data(),parent,child,position,matrix,events,capacity,used,attached,&d):fs15_scene_link_lod(&s.view,s.children.data(),parent,child,position,matrix,events,capacity,used,attached,queue))) {
            if(replacement.begin)memory->deallocate(memory->user,reinterpret_cast<void*>(replacement.begin));
            return 0;
        }
        const uintptr_t old=changed==SIZE_MAX?0:view->nodes[parent]->children_begin;
        s.commit(changed,replacement.begin);replacement.begin=0;
        if(old)memory->deallocate(memory->user,reinterpret_cast<void*>(old));
        return 1;
    }catch(...){if(replacement.begin)memory->deallocate(memory->user,reinterpret_cast<void*>(replacement.begin));return 0;}
}
static int owned_unlink_impl(const FS15SceneOwnedView* view,size_t parent,uint32_t slot,int suppress,FS15SceneNotification* events,size_t capacity,size_t* used,FS15SceneLodQueue* queue,FS15SceneLodQueue* lights,bool derived) {
    if(!view)return 0;
    try {Staged s(*view,queue!=nullptr,derived);FS15SceneDerivedState d{queue,lights,s.light_tokens.data(),s.light_tokens.size()};if(!(derived?fs15_scene_unlink_derived(&s.view,s.children.data(),parent,slot,suppress,events,capacity,used,&d):fs15_scene_unlink_lod(&s.view,s.children.data(),parent,slot,suppress,events,capacity,used,queue)))return 0;s.commit();return 1;}catch(...){return 0;}
}

extern "C" int fs15_scene_owned_state(const FS15SceneOwnedView* v,size_t i,unsigned op,uint32_t mask,int32_t origin,FS15SceneNotification* e,size_t cap,size_t* used) {
    return fs15_scene_owned_state_lod(v,i,op,mask,origin,e,cap,used,nullptr);
}
extern "C" int fs15_scene_owned_trs(const FS15SceneOwnedView* v,size_t i,unsigned op,void* frame,size_t bytes,FS15SceneNotification* e,size_t cap,size_t* used,int* warning) {
    return fs15_scene_owned_trs_lod(v,i,op,frame,bytes,e,cap,used,warning,nullptr);
}
extern "C" int fs15_scene_owned_link(const FS15SceneOwnedView* v,size_t parent,size_t child,uint32_t at,int matrix,const FS15SceneLifecycleServices* s,FS15SceneNotification* e,size_t cap,size_t* used,uint32_t* result) {
    return fs15_scene_owned_link_lod(v,parent,child,at,matrix,s,e,cap,used,result,nullptr);
}
extern "C" int fs15_scene_owned_unlink(const FS15SceneOwnedView* v,size_t parent,uint32_t at,int suppress,FS15SceneNotification* e,size_t cap,size_t* used) {
    return fs15_scene_owned_unlink_lod(v,parent,at,suppress,e,cap,used,nullptr);
}
extern "C" int fs15_scene_owned_world(const FS15SceneOwnedView* view,size_t index,float* position,size_t count) {
    if(!view||!position||count<3||index>=view->count)return 0;
    try {Staged s(*view,true,true);if(!fs15_scene_update_world(s.nodes.data(),s.nodes.size()*sizeof(s.nodes[0]),sizeof(s.nodes[0]),index))return 0;s.commit();std::memcpy(position,view->nodes[index]->world+12,12);return 1;}catch(...){return 0;}
}
extern "C" int fs15_scene_owned_mark_branch(FS15SceneStateRecord* root) {
    if(!root)return 0;
    try {
        std::vector<std::pair<FS15SceneStateRecord*,unsigned>> pending{{root,0}};
        std::map<FS15SceneStateRecord*,bool> seen;
        while(!pending.empty()) {
            const auto item=pending.back();pending.pop_back();auto* n=item.first;
            if(!n||item.second>=256||seen.size()>=limit||!seen.emplace(n,true).second||
                (n->vtable!=0x1404c0cd8&&n->vtable!=0x1404c5d98&&n->vtable!=0x1404c1c48&&n->vtable!=0x1404c1078))return 0;
            slots(n->children_begin,n->children_end,n->children_capacity);
            for(uintptr_t p=n->children_begin;p<n->children_end;p+=8)pending.emplace_back(reinterpret_cast<FS15SceneStateRecord*>(*reinterpret_cast<const uintptr_t*>(p)),item.second+1);
        }
        for(const auto& item:seen)item.first->flags|=8;
        return 1;
    }catch(...){return 0;}
}
extern "C" int fs15_scene_owned_matrix(const FS15SceneOwnedView* view,size_t index,const float* matrix,size_t count) {
    if(!view||index>=view->count)return 0;
    try {
        Staged s(*view,true,true);std::vector<uint8_t> kinds(view->kinds,view->kinds+view->count);
        // All inspected LOD tables inherit the exact TRS matrix setter at +0x38.
        for(auto& kind:kinds)if(kind>=2)kind=1;
        s.view.kinds=kinds.data();
        if(!fs15_scene_set_local_matrix(&s.view,index,matrix,count))return 0;
        s.commit();return 1;
    }catch(...){return 0;}
}

extern "C" int fs15_scene_owned_state_lod(const FS15SceneOwnedView* v,size_t i,unsigned op,uint32_t mask,int32_t origin,FS15SceneNotification* e,size_t c,size_t* n,FS15SceneLodQueue* q){return owned_state_impl(v,i,op,mask,origin,e,c,n,q,nullptr,false);}
extern "C" int fs15_scene_owned_state_derived(const FS15SceneOwnedView* v,size_t i,unsigned op,uint32_t mask,int32_t origin,FS15SceneNotification* e,size_t c,size_t* n,FS15SceneLodQueue* q,FS15SceneLodQueue* l){return owned_state_impl(v,i,op,mask,origin,e,c,n,q,l,true);}
extern "C" int fs15_scene_owned_trs_lod(const FS15SceneOwnedView* v,size_t i,unsigned op,void* f,size_t b,FS15SceneNotification* e,size_t c,size_t* n,int* w,FS15SceneLodQueue* q){return owned_trs_impl(v,i,op,f,b,e,c,n,w,q,nullptr,false);}
extern "C" int fs15_scene_owned_trs_derived(const FS15SceneOwnedView* v,size_t i,unsigned op,void* f,size_t b,FS15SceneNotification* e,size_t c,size_t* n,int* w,FS15SceneLodQueue* q,FS15SceneLodQueue* l){return owned_trs_impl(v,i,op,f,b,e,c,n,w,q,l,true);}
extern "C" int fs15_scene_owned_link_lod(const FS15SceneOwnedView* v,size_t p,size_t child,uint32_t at,int matrix,const FS15SceneLifecycleServices* m,FS15SceneNotification* e,size_t c,size_t* n,uint32_t* out,FS15SceneLodQueue* q){return owned_link_impl(v,p,child,at,matrix,m,e,c,n,out,q,nullptr,false);}
extern "C" int fs15_scene_owned_link_derived(const FS15SceneOwnedView* v,size_t p,size_t child,uint32_t at,int matrix,const FS15SceneLifecycleServices* m,FS15SceneNotification* e,size_t c,size_t* n,uint32_t* out,FS15SceneLodQueue* q,FS15SceneLodQueue* l){return owned_link_impl(v,p,child,at,matrix,m,e,c,n,out,q,l,true);}
extern "C" int fs15_scene_owned_unlink_lod(const FS15SceneOwnedView* v,size_t p,uint32_t slot,int suppress,FS15SceneNotification* e,size_t c,size_t* n,FS15SceneLodQueue* q){return owned_unlink_impl(v,p,slot,suppress,e,c,n,q,nullptr,false);}
extern "C" int fs15_scene_owned_unlink_derived(const FS15SceneOwnedView* v,size_t p,uint32_t slot,int suppress,FS15SceneNotification* e,size_t c,size_t* n,FS15SceneLodQueue* q,FS15SceneLodQueue* l){return owned_unlink_impl(v,p,slot,suppress,e,c,n,q,l,true);}
