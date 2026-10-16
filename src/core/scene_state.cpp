// Recovered scene state propagation; authored bounded arena/event transport.
#include "scene_state.h"
#include "scene_math.h"
#include "scene_graph.h"
#include "scene_trs.h"
#include "scene_lod.h"
#include "scene_derived.h"
#include <cmath>
#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
constexpr size_t none=std::numeric_limits<size_t>::max();
bool range(const void* data,size_t count,size_t step) {
    return count==0||(data&&step&&count<=std::numeric_limits<uintptr_t>::max()/step&&
        reinterpret_cast<uintptr_t>(data)<=std::numeric_limits<uintptr_t>::max()-count*step);
}
size_t offset(uintptr_t pointer,const void* base,size_t count,size_t stride) {
    const auto start=reinterpret_cast<uintptr_t>(base);
    if(pointer<start||pointer-start>=count*stride||(pointer-start)%stride)throw std::runtime_error("Out-of-view scene pointer");
    return (pointer-start)/stride;
}
struct State {
    const FS15SceneStateArena& view;
    size_t count;
    std::vector<FS15SceneStateRecord> nodes;
    std::vector<FS15SceneNotification> events;
    std::vector<uintptr_t> child_storage;
    std::vector<size_t> lod_requests;
    std::vector<size_t> light_requests;std::vector<uint32_t> light_tokens;FS15SceneDerivedState* derived;
    State(const FS15SceneStateArena& v,size_t index,bool lod=false,FS15SceneDerivedState* d=nullptr):view(v),count(v.node_stride?v.node_bytes/v.node_stride:0),derived(d) {
        if(v.node_stride<sizeof(FS15SceneStateRecord)||index>=count||
            !range(v.nodes,v.node_bytes,1)||!range(v.children,v.child_count,sizeof(uintptr_t))||
            !range(v.observers,v.observer_count,sizeof(uintptr_t))||
            !range(v.properties,v.property_count,sizeof(FS15ScenePropertyView))||
            !v.kinds||v.kind_count<count||!range(v.kinds,v.kind_count,1)||
            (v.special_root!=none&&v.special_root>=count))throw std::runtime_error("Invalid scene views");
        nodes.resize(count);
        if(d){if(d->count<count||!range(d->light_tokens,d->count,4))throw std::runtime_error("Invalid staged Light tokens");light_tokens.assign(d->light_tokens,d->light_tokens+count);}
        for(size_t i=0;i<count;++i) {
            if(v.kinds[i]>(d?3:lod?2:1))throw std::runtime_error("Unknown virtual dispatch kind");
            std::memcpy(&nodes[i],static_cast<const uint8_t*>(v.nodes)+i*v.node_stride,sizeof(nodes[i]));
        }
        for(auto& node:nodes)if(node.parent)node.parent=reinterpret_cast<uintptr_t>(&nodes[source_index(node.parent)]);
    }
    size_t source_index(uintptr_t p)const{return offset(p,view.nodes,count,view.node_stride);}
    size_t parent(size_t i)const{return nodes[i].parent?offset(nodes[i].parent,nodes.data(),count,sizeof(nodes[0])):none;}
    std::vector<uintptr_t> span(uintptr_t begin,uintptr_t end,const uintptr_t* data,size_t size,const uintptr_t* staged=nullptr)const {
        if(begin==end)return {};
        if(end<begin||(end-begin)%sizeof(uintptr_t))throw std::runtime_error("Invalid scene vector");
        const size_t first=offset(begin,data,size,sizeof(uintptr_t)),length=(end-begin)/sizeof(uintptr_t);
        if(length>size-first||length>0x7fffffff)throw std::runtime_error("Scene vector outside supplied view");
        const auto* source=staged?staged:data;
        return {source+first,source+first+length};
    }
    std::vector<size_t> children(size_t i)const {
        const auto& n=nodes[i];std::vector<size_t> result;
        for(uintptr_t p:span(n.children_begin,n.children_end,view.children,view.child_count,child_storage.empty()?nullptr:child_storage.data())) {
            const size_t child=source_index(p);
            if(parent(child)!=i)throw std::runtime_error("Inconsistent parent/child link");
            result.push_back(child);
        }
        return result;
    }
    uint32_t properties(size_t i)const {
        return view.properties[offset(nodes[i].properties,view.properties,view.property_count,sizeof(FS15ScenePropertyView))].flags;
    }
    void set_matrix(size_t i,const float* matrix) {
        for(unsigned lane=0;lane<16;++lane)if(!std::isfinite(matrix[lane]))throw std::runtime_error("Nonfinite matrix");
        std::memcpy(nodes[i].local,matrix,64);
        if(view.kinds[i]>=FS15_SCENE_TRS_NODE)nodes[i].flags|=0x80;
    }
    void world(size_t i) {
        if(!fs15_scene_update_world(nodes.data(),nodes.size()*sizeof(nodes[0]),sizeof(nodes[0]),i))
            throw std::runtime_error("Invalid world chain");
    }
    void refresh_trs(size_t i) {
        auto& node=nodes[i];float scale[3],rotation[3];
        if(!fs15_matrix_scale(node.local,16,scale,3)||!fs15_matrix_euler(node.local,16,scale,rotation,3))
            throw std::runtime_error("Invalid native TRS cache input");
        std::memcpy(node.translation,node.local+12,12);
        std::memcpy(node.rotation,rotation,12);std::memcpy(node.scale,scale,12);
        node.flags&=~uint32_t(0x80);
    }
    void rebuild_trs(size_t i) {
        auto& node=nodes[i];if(node.flags&0x80)refresh_trs(i);
        float local[16];
        if(!fs15_compose_trs(node.rotation,node.scale,node.translation,local,16))throw std::runtime_error("Invalid TRS composition");
        set_matrix(i,local);
        std::vector<bool> visited(count);propagate(i,1,0,visited);
        node.flags&=~uint32_t(0x80);
    }
    void commit() {
        for(size_t i=0;i<count;++i) {
            const size_t p=parent(i);
            nodes[i].parent=p==none?0:reinterpret_cast<uintptr_t>(view.nodes)+p*view.node_stride;
        }
        for(size_t i=0;i<count;++i)std::memcpy(static_cast<uint8_t*>(view.nodes)+i*view.node_stride,&nodes[i],sizeof(nodes[i]));
        if(derived)for(size_t i=0;i<count;++i)if(view.kinds[i]==3)derived->light_tokens[i]=light_tokens[i];
    }
    void propagate(size_t i,uint32_t mask,int32_t origin,std::vector<bool>& visited,size_t depth=0) {
        if(visited[i]||depth>=256)throw std::runtime_error("Repeated/cyclic/deep scene child");
        visited[i]=true;
        auto& n=nodes[i];const uint32_t initial=n.flags,input_mask=mask;const size_t p=parent(i);
        if((initial&0x50)==0x40)mask&=~uint32_t(1);
        if(mask&2) {
            bool visible=false;
            if(initial&1)visible=i==view.special_root||(p!=none&&(nodes[p].type_flags&0x20)&&(nodes[p].flags&2));
            if(((initial&2)!=0)==visible)mask&=~uint32_t(2);
            else if(visible)n.flags|=2;else n.flags&=~uint32_t(2);
        }
        const bool inherited=p!=none&&(nodes[p].type_flags&0x20);
        if((mask&4)&&!(n.type_flags&0x80)) {
            if(inherited&&(nodes[p].flags&8))n.flags|=8;else n.flags&=~uint32_t(8);
        }
        if(mask&8) {
            float start=n.local_clip[0],end=n.local_clip[1];
            if(!std::isfinite(start)||!std::isfinite(end))throw std::runtime_error("Nonfinite clip distance");
            if(inherited) {
                start=nodes[p].world_clip[0];end=nodes[p].world_clip[1];
                if(!std::isfinite(start)||!std::isfinite(end))throw std::runtime_error("Nonfinite inherited distance");
                if(n.local_clip[0]<start)start=n.local_clip[0];
                if(end<n.local_clip[1])end=n.local_clip[1];
            }
            if(n.world_clip[0]==start&&n.world_clip[1]==end)mask&=~uint32_t(8);
            else {n.world_clip[0]=start;n.world_clip[1]=end;}
        }
        if(mask&0x30) {
            const uint32_t a=inherited?n.local_masks[0]&nodes[p].world_masks[0]:n.local_masks[0];
            const uint32_t b=inherited?n.local_masks[1]&nodes[p].world_masks[1]:n.local_masks[1];
            if(n.world_masks[0]==a&&n.world_masks[1]==b)mask&=~uint32_t(0x30);
            else {n.world_masks[0]=a;n.world_masks[1]=b;}
        }
        if(!mask){derived_post(i,input_mask);return;}
        if(mask&1) {
            n.flags|=0x40;
            for(uintptr_t observer:span(n.observers_begin,n.observers_end,view.observers,view.observer_count)) {
                if(!observer)throw std::runtime_error("Null observer");
                events.push_back({i,observer,origin,0});
            }
        }
        for(size_t child:children(i))if(nodes[child].type_flags&0x20) {
            uint32_t child_mask=mask;
            if((properties(i)&0x10)&&(properties(child)&0x88)==0x88&&origin==1) {
                world(i);float inverse[16],relative[16];
                if(!fs15_affine_inverse(n.world,16,inverse,16))throw std::runtime_error("Singular parent in world-preserving path");
                world(child);
                if(!fs15_affine_multiply(inverse,16,nodes[child].world,16,relative,16))throw std::runtime_error("Invalid relative transform");
                set_matrix(child,relative);
                child_mask&=~uint32_t(1);
                if(!child_mask)continue;
            }
            propagate(child,child_mask,origin,visited,depth+1);
        }
        derived_post(i,input_mask);
    }
    void derived_post(size_t i,uint32_t mask) {
        if(view.kinds[i]==3) {
            auto& n=nodes[i];
            if(mask&0xcu){if(mask&4u)light_tokens[i]=0xffffffff;n.flags|=0x400;light_pending(i);}
            if(mask&3u){n.flags|=0x500;light_pending(i);}return;
        }
        if(view.kinds[i]!=2||!(mask&3u))return;
        auto& n=nodes[i];n.flags|=0x100;
        if(!(n.flags&0x400u)&&(n.flags&0x202u)){n.flags|=0x400;lod_requests.push_back(i);}
    }
    bool queue_capacity(FS15SceneLodQueue* queue)const {
        const bool lod_ok=capacity(queue,lod_requests);
        const bool light_ok=capacity(derived?derived->lights:nullptr,light_requests);return lod_ok&&light_ok;
    }
    static bool capacity(FS15SceneLodQueue* q,const std::vector<size_t>& requests){if(!q)return requests.empty();q->count=requests.size();return q->count<=q->capacity&&range(q->indices,q->capacity,sizeof(size_t));}
    void light_pending(size_t i){auto& n=nodes[i];if(!(n.flags&0x800u)&&(n.flags&0x202u)){n.flags|=0x800;light_requests.push_back(i);}}
    void queue_output(FS15SceneLodQueue* queue)const {
        if(queue&&!lod_requests.empty())std::memcpy(queue->indices,lod_requests.data(),lod_requests.size()*sizeof(size_t));
        if(derived&&derived->lights&&!light_requests.empty())std::memcpy(derived->lights->indices,light_requests.data(),light_requests.size()*sizeof(size_t));
    }
    void rescan(size_t i,uint32_t mask) {
        std::vector<bool> visited(count);
        do {
            if(visited[i])throw std::runtime_error("Cyclic parent chain");
            visited[i]=true;
            auto& n=nodes[i];
            if(mask&1) {
                bool present=!span(n.observers_begin,n.observers_end,view.observers,view.observer_count).empty();
                if(!present)for(size_t child:children(i))if(nodes[child].flags&0x10){present=true;break;}
                if(((n.flags&0x10)!=0)==present)mask&=~uint32_t(1);
                else if(present)n.flags|=0x10;else n.flags&=~uint32_t(0x10);
            }
            i=parent(i);
        }while(i!=none&&mask);
    }
    void mark(size_t i,std::vector<bool>& visited,size_t depth=0,uint32_t token=0) {
        if(visited[i]||depth>=256)throw std::runtime_error("Repeated/cyclic/deep scene child");
        visited[i]=true;
        if(view.kinds[i]==3&&light_tokens[i]!=token){nodes[i].flags|=0x400;light_tokens[i]=token;light_pending(i);}
        nodes[i].flags|=8;
        for(size_t child:children(i))mark(child,visited,depth+1,token);
    }
    struct ChildWindow {size_t first,size,capacity;};
    ChildWindow window(size_t i)const {
        const auto& n=nodes[i];
        if(!n.children_begin&&!n.children_end&&!n.children_capacity)return {0,0,0};
        const uintptr_t base=reinterpret_cast<uintptr_t>(view.children);
        if(n.children_begin<base||n.children_end<n.children_begin||n.children_capacity<n.children_end||
            (n.children_begin-base)%sizeof(uintptr_t)||(n.children_end-base)%sizeof(uintptr_t)||
            (n.children_capacity-base)%sizeof(uintptr_t)||
            n.children_capacity-base>view.child_count*sizeof(uintptr_t))throw std::runtime_error("Invalid reserved child vector");
        const size_t length=(n.children_end-n.children_begin)/sizeof(uintptr_t);
        if(length>0x7fffffff)throw std::runtime_error("Child count exceeds native integer range");
        return {(n.children_begin-base)/sizeof(uintptr_t),length,(n.children_capacity-n.children_begin)/sizeof(uintptr_t)};
    }
    void validate_hierarchy()const {
        std::vector<size_t> owners(view.child_count,none),incoming(count,none);
        for(size_t i=0;i<count;++i) {
            const auto w=window(i);
            for(size_t j=0;j<w.capacity;++j) {
                if(owners[w.first+j]!=none)throw std::runtime_error("Overlapping child capacities");
                owners[w.first+j]=i;
            }
            for(size_t child:children(i)) {
                if(incoming[child]!=none)throw std::runtime_error("Duplicate child reference");
                incoming[child]=i;
            }
        }
        for(size_t i=0;i<count;++i) {
            if(incoming[i]!=parent(i))throw std::runtime_error("Missing parent/child reference");
            size_t cursor=i,depth=0;
            while(cursor!=none) {
                if(++depth>256)throw std::runtime_error("Cyclic or excessive hierarchy depth");
                cursor=parent(cursor);
            }
        }
    }
    void stage_children(uintptr_t* writable) {
        if(writable!=view.children)throw std::runtime_error("Mismatched mutable child storage");
        if(view.child_count)child_storage.assign(view.children,view.children+view.child_count);
        validate_hierarchy();
    }
    void unlink(size_t i,uint32_t slot,bool suppress) {
        const auto w=window(i);
        if(slot>=w.size)throw std::runtime_error("Invalid child slot");
        const size_t child=source_index(child_storage[w.first+slot]);
        nodes[child].parent=0;
        for(size_t j=slot;j+1<w.size;++j)child_storage[w.first+j]=child_storage[w.first+j+1];
        // Native erase keeps the now-unused trailing slot and vector capacity.
        nodes[i].children_end-=sizeof(uintptr_t);
        if(nodes[child].flags&0x10)rescan(i,1);
        if(!suppress&&(nodes[child].type_flags&0x20)) {
            std::vector<bool> visited(count);propagate(child,0x3f,0,visited);
        }
    }
    uint32_t link(size_t i,size_t child,uint32_t position,bool matrix_link) {
        if(child>=count||child==i)throw std::runtime_error("Invalid linked child");
        const size_t old=parent(child);
        // Reject new cycles before any staged propagation can traverse them.
        for(size_t cursor=i;cursor!=none;cursor=parent(cursor))if(cursor==child)throw std::runtime_error("Link would create cycle");
        if(old!=none) {
            const auto previous=children(old);
            const auto found=std::find(previous.begin(),previous.end(),child);
            if(found==previous.end())throw std::runtime_error("Old parent lacks child");
            unlink(old,static_cast<uint32_t>(found-previous.begin()),false);
        }
        const auto w=window(i);
        if(w.size>=w.capacity)throw std::runtime_error("Child storage exhausted; native allocator not recovered");
        nodes[child].parent=reinterpret_cast<uintptr_t>(&nodes[i]);
        const uint32_t result=position<w.size?position:static_cast<uint32_t>(w.size);
        child_storage[w.first+w.size]=reinterpret_cast<uintptr_t>(view.nodes)+child*view.node_stride;
        // Equivalent to the native pointer-vector rotate after appending.
        std::rotate(child_storage.begin()+w.first+result,child_storage.begin()+w.first+w.size,
            child_storage.begin()+w.first+w.size+1);
        nodes[i].children_end+=sizeof(uintptr_t);
        if(matrix_link) {
            if(nodes[child].flags&0x10)rescan(i,1);
            if(nodes[child].type_flags&0x20) {
                bool transformed=true;
                if(old==none&&parent(i)==none) {
                    world(i);transformed=false;
                    // The original tests these 12 lanes, excluding affine padding.
                    constexpr unsigned lanes[]={0,1,2,4,5,6,8,9,10,12,13,14};
                    for(unsigned lane:lanes) {
                        const float expected=(lane==0||lane==5||lane==10)?1.0f:0.0f;
                        if(!(std::fabs(nodes[i].world[lane]-expected)<1e-4f)){transformed=true;break;}
                    }
                }
                const uint32_t flags=nodes[child].flags;
                uint32_t mask=0x38;
                if(transformed)mask|=1;
                if(((nodes[i].flags^flags)&2)&&(flags&1))mask|=2;
                if((nodes[i].flags|flags)&8)mask|=4;
                std::vector<bool> visited(count);propagate(child,mask,0,visited);
            }
        }
        return result;
    }
    void commit_children(uintptr_t* writable) {
        validate_hierarchy();commit();
        if(!child_storage.empty())std::memcpy(writable,child_storage.data(),child_storage.size()*sizeof(uintptr_t));
    }
};
}

static int state_derived_impl(const FS15SceneStateArena* arena,
    size_t index,unsigned operation,uint32_t mask,int32_t origin,FS15SceneNotification* events,
    size_t capacity,size_t* notification_count,FS15SceneLodQueue* queue,FS15SceneDerivedState* derived) {
    if(!arena||!notification_count||operation>FS15_SCENE_MARK_INHERITED||!range(events,capacity,sizeof(*events)))return 0;
    try {
        State state(*arena,index,queue!=nullptr,derived);std::vector<bool> visited(state.count);
        if(operation==FS15_SCENE_PROPAGATE)state.propagate(index,mask,origin,visited);
        else if(operation==FS15_SCENE_RESCAN_OBSERVERS)state.rescan(index,mask);
        else state.mark(index,visited,0,mask);
        *notification_count=state.events.size();
        if(!state.queue_capacity(queue)||state.events.size()>capacity)return 0;
        state.commit();state.queue_output(queue);
        if(!state.events.empty())std::memcpy(events,state.events.data(),state.events.size()*sizeof(*events));
        return 1;
    }catch(...){return 0;}
}

extern "C" FS15_CORE_API int fs15_scene_set_local_matrix(const FS15SceneStateArena* arena,
    size_t index,const float* matrix,size_t count) {
    if(!arena||!matrix||count<16)return 0;
    try {State state(*arena,index);state.set_matrix(index,matrix);state.commit();return 1;}
    catch(...){return 0;}
}

static int trs_derived_impl(const FS15SceneStateArena* arena,
    size_t index,unsigned operation,void* frame,size_t frame_bytes,FS15SceneNotification* events,
    size_t capacity,size_t* notification_count,int* scale_warning,FS15SceneLodQueue* queue,FS15SceneDerivedState* derived) {
    if(!arena||!notification_count||!scale_warning||operation>FS15_SCENE_GET_SCALE||
        !range(events,capacity,sizeof(*events))||(operation>=2&&(!frame||frame_bytes<(operation>=5?0x11a:0x24))))return 0;
    try {
        State state(*arena,index,queue!=nullptr,derived);auto& node=state.nodes[index];int warning=0;
        float values[3]{};
        if(operation>=2&&operation<=4) {
            for(unsigned i=0;i<3;++i)std::memcpy(values+i,static_cast<const uint8_t*>(frame)+i*16,4);
            for(float value:values)if(!std::isfinite(value))throw std::runtime_error("Nonfinite Lua TRS input");
            if(operation==FS15_SCENE_SET_SCALE&&(state.properties(index)&0x88)==0x88&&
                (values[0]!=1||values[1]!=1||values[2]!=1))warning=1;
        }
        if(operation==FS15_SCENE_TRS_REFRESH)state.refresh_trs(index);
        else if(operation==FS15_SCENE_TRS_REBUILD)state.rebuild_trs(index);
        else {
            if(node.flags&0x80)state.refresh_trs(index);
            if(operation<=4) {
                float* target=operation==2?node.translation:operation==3?node.rotation:node.scale;
                std::memcpy(target,values,12);state.rebuild_trs(index);
            }else {
                const float* source=operation==5?node.translation:operation==6?node.rotation:node.scale;
                for(unsigned i=0;i<3;++i)if(!std::isfinite(source[i]))throw std::runtime_error("Nonfinite cached TRS output");
                std::memcpy(values,source,12);
            }
        }
        *notification_count=state.events.size();
        if(!state.queue_capacity(queue)||state.events.size()>capacity)return 0;
        state.commit();state.queue_output(queue);
        if(operation>=5)for(unsigned i=0;i<3;++i) {
            const uint16_t type=3;
            std::memcpy(static_cast<uint8_t*>(frame)+0xf0+i*16,values+i,4);
            std::memcpy(static_cast<uint8_t*>(frame)+0xf8+i*16,&type,2);
        }
        if(!state.events.empty())std::memcpy(events,state.events.data(),state.events.size()*sizeof(*events));
        *scale_warning=warning;return 1;
    }catch(...){return 0;}
}

static int link_derived_impl(const FS15SceneStateArena* arena,uintptr_t* writable,
    size_t parent,size_t child,uint32_t position,int matrix_link,FS15SceneNotification* events,
    size_t capacity,size_t* used,uint32_t* attached,FS15SceneLodQueue* queue,FS15SceneDerivedState* derived) {
    if(!arena||!used||!attached||(matrix_link!=0&&matrix_link!=1)||!range(events,capacity,sizeof(*events)))return 0;
    try {
        State state(*arena,parent,queue!=nullptr,derived);state.stage_children(writable);
        const uint32_t result=state.link(parent,child,position,matrix_link!=0);
        *used=state.events.size();if(!state.queue_capacity(queue)||state.events.size()>capacity)return 0;
        state.commit_children(writable);state.queue_output(queue);
        if(!state.events.empty())std::memcpy(events,state.events.data(),state.events.size()*sizeof(*events));
        *attached=result;return 1;
    }catch(...){return 0;}
}

static int unlink_derived_impl(const FS15SceneStateArena* arena,uintptr_t* writable,
    size_t parent,uint32_t slot,int suppress,FS15SceneNotification* events,size_t capacity,size_t* used,FS15SceneLodQueue* queue,FS15SceneDerivedState* derived) {
    if(!arena||!used||!range(events,capacity,sizeof(*events)))return 0;
    try {
        State state(*arena,parent,queue!=nullptr,derived);state.stage_children(writable);state.unlink(parent,slot,suppress!=0);
        *used=state.events.size();if(!state.queue_capacity(queue)||state.events.size()>capacity)return 0;
        state.commit_children(writable);state.queue_output(queue);
        if(!state.events.empty())std::memcpy(events,state.events.data(),state.events.size()*sizeof(*events));
        return 1;
    }catch(...){return 0;}
}

extern "C" int fs15_scene_state_update(const FS15SceneStateArena* a,size_t i,unsigned op,uint32_t mask,int32_t origin,FS15SceneNotification* e,size_t c,size_t* n) {
    return fs15_scene_state_lod(a,i,op,mask,origin,e,c,n,nullptr);
}
extern "C" int fs15_scene_trs_update(const FS15SceneStateArena* a,size_t i,unsigned op,void* frame,size_t bytes,FS15SceneNotification* e,size_t c,size_t* n,int* warning) {
    return fs15_scene_trs_lod(a,i,op,frame,bytes,e,c,n,warning,nullptr);
}
extern "C" int fs15_scene_link(const FS15SceneStateArena* a,uintptr_t* w,size_t p,size_t child,uint32_t at,int matrix,FS15SceneNotification* e,size_t c,size_t* n,uint32_t* result) {
    return fs15_scene_link_lod(a,w,p,child,at,matrix,e,c,n,result,nullptr);
}
extern "C" int fs15_scene_unlink(const FS15SceneStateArena* a,uintptr_t* w,size_t p,uint32_t at,int suppress,FS15SceneNotification* e,size_t c,size_t* n) {
    return fs15_scene_unlink_lod(a,w,p,at,suppress,e,c,n,nullptr);
}

extern "C" int fs15_scene_state_lod(const FS15SceneStateArena* a,size_t i,unsigned op,uint32_t mask,int32_t origin,FS15SceneNotification* e,size_t c,size_t* n,FS15SceneLodQueue* q){return state_derived_impl(a,i,op,mask,origin,e,c,n,q,nullptr);}
extern "C" int fs15_scene_state_derived(const FS15SceneStateArena* a,size_t i,unsigned op,uint32_t mask,int32_t origin,FS15SceneNotification* e,size_t c,size_t* n,FS15SceneDerivedState* d){return d?state_derived_impl(a,i,op,mask,origin,e,c,n,d->lod,d):0;}
extern "C" int fs15_scene_trs_lod(const FS15SceneStateArena* a,size_t i,unsigned op,void* f,size_t b,FS15SceneNotification* e,size_t c,size_t* n,int* w,FS15SceneLodQueue* q){return trs_derived_impl(a,i,op,f,b,e,c,n,w,q,nullptr);}
extern "C" int fs15_scene_trs_derived(const FS15SceneStateArena* a,size_t i,unsigned op,void* f,size_t b,FS15SceneNotification* e,size_t c,size_t* n,int* w,FS15SceneDerivedState* d){return d?trs_derived_impl(a,i,op,f,b,e,c,n,w,d->lod,d):0;}
extern "C" int fs15_scene_link_lod(const FS15SceneStateArena* a,uintptr_t* children,size_t p,size_t child,uint32_t at,int matrix,FS15SceneNotification* e,size_t c,size_t* n,uint32_t* out,FS15SceneLodQueue* q){return link_derived_impl(a,children,p,child,at,matrix,e,c,n,out,q,nullptr);}
extern "C" int fs15_scene_link_derived(const FS15SceneStateArena* a,uintptr_t* children,size_t p,size_t child,uint32_t at,int matrix,FS15SceneNotification* e,size_t c,size_t* n,uint32_t* out,FS15SceneDerivedState* d){return d?link_derived_impl(a,children,p,child,at,matrix,e,c,n,out,d->lod,d):0;}
extern "C" int fs15_scene_unlink_lod(const FS15SceneStateArena* a,uintptr_t* children,size_t p,uint32_t slot,int suppress,FS15SceneNotification* e,size_t c,size_t* n,FS15SceneLodQueue* q){return unlink_derived_impl(a,children,p,slot,suppress,e,c,n,q,nullptr);}
extern "C" int fs15_scene_unlink_derived(const FS15SceneStateArena* a,uintptr_t* children,size_t p,uint32_t slot,int suppress,FS15SceneNotification* e,size_t c,size_t* n,FS15SceneDerivedState* d){return d?unlink_derived_impl(a,children,p,slot,suppress,e,c,n,d->lod,d):0;}
