// Recovered scene state propagation; authored bounded arena/event transport.
#include "scene_state.h"
#include "scene_math.h"
#include "scene_graph.h"
#include <cmath>
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
    State(const FS15SceneStateArena& v,size_t index):view(v),count(v.node_stride?v.node_bytes/v.node_stride:0) {
        if(v.node_stride<sizeof(FS15SceneStateRecord)||index>=count||
            !range(v.nodes,v.node_bytes,1)||!range(v.children,v.child_count,sizeof(uintptr_t))||
            !range(v.observers,v.observer_count,sizeof(uintptr_t))||
            !range(v.properties,v.property_count,sizeof(FS15ScenePropertyView))||
            !v.kinds||v.kind_count<count||!range(v.kinds,v.kind_count,1)||
            (v.special_root!=none&&v.special_root>=count))throw std::runtime_error("Invalid scene views");
        nodes.resize(count);
        for(size_t i=0;i<count;++i) {
            if(v.kinds[i]>FS15_SCENE_TRS_NODE)throw std::runtime_error("Unknown virtual dispatch kind");
            std::memcpy(&nodes[i],static_cast<const uint8_t*>(v.nodes)+i*v.node_stride,sizeof(nodes[i]));
        }
        for(auto& node:nodes)if(node.parent)node.parent=reinterpret_cast<uintptr_t>(&nodes[source_index(node.parent)]);
    }
    size_t source_index(uintptr_t p)const{return offset(p,view.nodes,count,view.node_stride);}
    size_t parent(size_t i)const{return nodes[i].parent?offset(nodes[i].parent,nodes.data(),count,sizeof(nodes[0])):none;}
    std::vector<uintptr_t> span(uintptr_t begin,uintptr_t end,const uintptr_t* data,size_t size)const {
        if(begin==end)return {};
        if(end<begin||(end-begin)%sizeof(uintptr_t))throw std::runtime_error("Invalid scene vector");
        const size_t first=offset(begin,data,size,sizeof(uintptr_t)),length=(end-begin)/sizeof(uintptr_t);
        if(length>size-first||length>0x7fffffff)throw std::runtime_error("Scene vector outside supplied view");
        return {data+first,data+first+length};
    }
    std::vector<size_t> children(size_t i)const {
        const auto& n=nodes[i];std::vector<size_t> result;
        for(uintptr_t p:span(n.children_begin,n.children_end,view.children,view.child_count)) {
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
        if(view.kinds[i]==FS15_SCENE_TRS_NODE)nodes[i].flags|=0x80;
    }
    void world(size_t i) {
        if(!fs15_scene_update_world(nodes.data(),nodes.size()*sizeof(nodes[0]),sizeof(nodes[0]),i))
            throw std::runtime_error("Invalid world chain");
    }
    void commit() {
        for(size_t i=0;i<count;++i) {
            const size_t p=parent(i);
            nodes[i].parent=p==none?0:reinterpret_cast<uintptr_t>(view.nodes)+p*view.node_stride;
        }
        for(size_t i=0;i<count;++i)std::memcpy(static_cast<uint8_t*>(view.nodes)+i*view.node_stride,&nodes[i],sizeof(nodes[i]));
    }
    void propagate(size_t i,uint32_t mask,int32_t origin,std::vector<bool>& visited,size_t depth=0) {
        if(visited[i]||depth>=256)throw std::runtime_error("Repeated/cyclic/deep scene child");
        visited[i]=true;
        auto& n=nodes[i];const uint32_t initial=n.flags;const size_t p=parent(i);
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
        if(!mask)return;
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
    void mark(size_t i,std::vector<bool>& visited,size_t depth=0) {
        if(visited[i]||depth>=256)throw std::runtime_error("Repeated/cyclic/deep scene child");
        visited[i]=true;
        nodes[i].flags|=8;
        for(size_t child:children(i))mark(child,visited,depth+1);
    }
};
}

extern "C" FS15_CORE_API int fs15_scene_state_update(const FS15SceneStateArena* arena,
    size_t index,unsigned operation,uint32_t mask,int32_t origin,FS15SceneNotification* events,
    size_t capacity,size_t* notification_count) {
    if(!arena||!notification_count||operation>FS15_SCENE_MARK_INHERITED||!range(events,capacity,sizeof(*events)))return 0;
    try {
        State state(*arena,index);std::vector<bool> visited(state.count);
        if(operation==FS15_SCENE_PROPAGATE)state.propagate(index,mask,origin,visited);
        else if(operation==FS15_SCENE_RESCAN_OBSERVERS)state.rescan(index,mask);
        else state.mark(index,visited);
        *notification_count=state.events.size();
        if(state.events.size()>capacity)return 0;
        state.commit();
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
