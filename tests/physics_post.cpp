#include "physics_post.h"
#include "wheel_record.h"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>
namespace {
void check(bool b){if(!b)throw std::runtime_error("Physics post-step fixture failed");}
template<class T>void put(void* p,size_t o,T v){std::memcpy(static_cast<uint8_t*>(p)+o,&v,sizeof(v));}
struct Fixture {
    FS15PhysicsDispatchState state{};FS15PhysicsTreeNode objects{},wheels{},object_node{},wheel_node{};
    fs15::WheelRecord wheel{};fs15::WheelSet set{};
    uint8_t endpoints[2][0x20]{},bodies[2][0x118]{},metadata[2][0x20]{};
    FS15PhysicsPair current{};FS15PhysicsPreviousPair previous[2]{};int32_t bucket=0;
    uintptr_t listeners[1]{55};std::vector<int> trace;
    uint8_t pending_item[0x20]{},pending_owner[0x18]{};uint8_t* pending_list[1]{pending_item};
    FS15PhysicsPostServices services{this,wait,object,frame,nullptr,allocate,release,observer,pending,cancel,32,nullptr,nullptr,4};
    Fixture(){
        for(auto* h:{&objects,&wheels}){h->nil=1;h->left=h->parent=h->right=h;}
        object_node.left=object_node.parent=object_node.right=&objects;object_node.value=99;objects.left=objects.parent=objects.right=&object_node;
        wheel_node.left=wheel_node.parent=wheel_node.right=&wheels;wheel_node.value=reinterpret_cast<uintptr_t>(&set);wheels.left=wheels.parent=wheels.right=&wheel_node;
        set.count=1;set.records=&wheel;put(&state,0x250,&wheels);put(&state,0x3a8,&objects);
        state.busy=1;state.asynchronous=1;state.completion_event=2;
        put(&wheel,0xa0,3.f);put(&wheel,0x108,12.f);put(&wheel,0x14c,8.f);put(&wheel,0x150,0.5f);
        for(size_t i=0;i<2;++i){put(endpoints[i],0,reinterpret_cast<uintptr_t>(bodies[i]));put(endpoints[i],0x18,uint32_t(i+1));put(bodies[i],0x110,reinterpret_cast<uintptr_t>(metadata[i]));put(metadata[i],0,i?uint32_t(0):uint32_t(0x40));put(metadata[i],8,uintptr_t(100+i));}
        current.first=reinterpret_cast<uintptr_t>(endpoints[0]);current.second=reinterpret_cast<uintptr_t>(endpoints[1]);current.mask=1;current.next=-1;
        auto* c=reinterpret_cast<FS15PhysicsContacts*>(reinterpret_cast<uint8_t*>(&state)+0x48);c->count=1;c->capacity=1;c->current=&current;c->previous=previous;c->previous_capacity=2;c->buckets=&bucket;
        put(&state,0x198,listeners);put(&state,0x1a0,listeners+1);
        put(pending_item,0x10,reinterpret_cast<uintptr_t>(pending_owner));put(pending_item,0x18,0.25f);put(pending_owner,4,uint32_t(7));put(pending_owner,0x10,uintptr_t(8));put(&state,0x320,pending_list);put(&state,0x328,uint32_t(1));
    }
    static uint32_t wait(void* p,uintptr_t token,uint32_t timeout,uint8_t alertable){auto& f=*static_cast<Fixture*>(p);check(token==2&&timeout==UINT32_MAX&&!alertable&&f.state.busy);f.trace.push_back(1);return 0;}
    static int object(void* p,uintptr_t token){auto& f=*static_cast<Fixture*>(p);check(token==99);f.trace.push_back(2);return 1;}
    static int frame(void* p,const void* set,int32_t index,float* out){auto& f=*static_cast<Fixture*>(p);check(set==&f.set&&!index&&f.wheel.axle_speed==12);f.trace.push_back(3);std::memset(out,0,64);out[0]=out[5]=out[10]=out[15]=1;return 1;}
    static void* allocate(void*,size_t bytes,size_t alignment){check(alignment==16);return std::malloc(bytes);}
    static void release(void*,void* p){std::free(p);}
    static int observer(void* p,uintptr_t listener,uintptr_t a,uintptr_t b,uintptr_t target,uint32_t event,uint32_t part){auto& f=*static_cast<Fixture*>(p);check(listener==55&&a==100&&b==101&&target==101&&part==UINT32_MAX&&f.wheel.position[0]==3);f.trace.push_back(10+int(event));return 0;}
    static int pending(void* p,uintptr_t callback,uint32_t owner,float value,uint8_t* cancel){auto& f=*static_cast<Fixture*>(p);check(callback==8&&owner==7&&value==0.25f);f.trace.push_back(4);*cancel=1;return 1;}
    static int cancel(void* p,void* state,uint32_t owner){auto& f=*static_cast<Fixture*>(p);check(state==&f.state&&owner==7);f.trace.push_back(5);return 1;}
};
}
int main(){try{
    Fixture f;check(fs15_physics_post_step(&f.state,sizeof(f.state),&f.services));
    check(f.trace==std::vector<int>({1,2,3,11,4,5})&&!f.state.busy&&f.wheel.position[0]==3&&f.wheel.contact_force==8);
    auto* c=reinterpret_cast<FS15PhysicsContacts*>(reinterpret_cast<uint8_t*>(&f.state)+0x48);check(c->previous_count==1&&c->previous[0].mask==1&&!f.current.mask);
    f.trace.clear();f.state.busy=1;f.current.mask=1;check(fs15_physics_post_step(&f.state,sizeof(f.state),&f.services));check(f.trace==std::vector<int>({1,2,3,14}));
    f.trace.clear();f.state.busy=1;f.current.mask=0;check(fs15_physics_post_step(&f.state,sizeof(f.state),&f.services));check(f.trace==std::vector<int>({1,2,3,12})&&c->previous_count==0);
    f.trace.clear();check(fs15_physics_post_step(&f.state,sizeof(f.state),&f.services)&&f.trace.empty());
    f.state.busy=1;f.state.asynchronous=0;check(fs15_physics_post_step(&f.state,sizeof(f.state),&f.services));check(f.trace==std::vector<int>({2,3}));
    FS15PhysicsContacts dynamic{};dynamic.count=dynamic.capacity=1;dynamic.current=&f.current;dynamic.buckets=&f.bucket;f.current.mask=1;
    check(fs15_physics_contacts_finish(&dynamic,sizeof(dynamic),f.listeners,1,&f.services)&&dynamic.previous_count==1&&dynamic.previous_capacity==1&&dynamic.previous[0].mask==1);
    std::free(dynamic.previous);dynamic.previous=nullptr;dynamic.previous_capacity=dynamic.previous_count=0;f.current.mask=1;
    auto unavailable=f.services;unavailable.allocate=[](void*,size_t,size_t)->void*{return nullptr;};
    check(!fs15_physics_contacts_finish(&dynamic,sizeof(dynamic),f.listeners,1,&unavailable)&&f.current.mask==1&&!dynamic.previous);
    // Cancellation can compact the remaining native pending queue itself.
    uint8_t extra[0x20]{};extra[0x1c]=1;uint8_t* queue[]={f.pending_item,extra};put(&f.state,0x320,queue);put(&f.state,0x328,uint32_t(2));f.state.busy=1;f.current.mask=0;
    auto compacting=f.services;compacting.cancel=[](void* p,void* state,uint32_t id){check(Fixture::cancel(p,state,id));put(state,0x328,uint32_t(0));return 1;};
    check(fs15_physics_post_step(&f.state,sizeof(f.state),&compacting)&&!f.state.busy);
    struct Groups {uint8_t wrapper[0x18]{},data[2][0x330]{};uintptr_t items[2]{};} groups;
    put(groups.wrapper,0x10,uintptr_t(99));for(size_t i=0;i<2;++i)groups.items[i]=reinterpret_cast<uintptr_t>(groups.data[i]);
    put(groups.data[0],0x310,reinterpret_cast<uintptr_t>(f.bodies[1]));put(groups.data[0],0x320,uint32_t(2));put(groups.data[0],0x78,uint32_t(35));put(groups.data[0],0x138,uint32_t(255));
    put(groups.data[1],0x320,uint32_t(1));put(groups.data[1],0x7c,uint32_t(64));
    FS15PhysicsPostServices collector{};collector.user=&groups;collector.contact_row_limit=4;
    collector.lookup=[](void* p,uintptr_t,uintptr_t,uintptr_t){return reinterpret_cast<uintptr_t>(static_cast<Groups*>(p)->wrapper);};
    collector.groups=[](void* p,uintptr_t token,void* vector){check(token==99);auto& g=*static_cast<Groups*>(p);put(vector,4,uint32_t(2));put(vector,8,uint32_t(2));put(vector,0x10,g.items);return 1;};
    put(f.metadata[1],0,uint32_t(0x80));check(fs15_physics_contacts_collect(c,sizeof(*c),1,&collector)&&f.current.mask==0x80000009u);
    uint32_t scratch=1;std::memcpy(&scratch,reinterpret_cast<uint8_t*>(c)+0x44,4);check(scratch==0);
    put(f.metadata[1],0,uint32_t(0));check(fs15_physics_contacts_collect(c,sizeof(*c),1,&collector)&&f.current.mask==1);
    collector.lookup=[](void*,uintptr_t,uintptr_t,uintptr_t)->uintptr_t{return 0;};check(fs15_physics_contacts_collect(c,sizeof(*c),1,&collector)&&f.current.mask==0);
    f.wheels.left=&f.wheel_node;f.wheel_node.right=&f.wheel_node;check(!fs15_physics_wheels(&f.state,sizeof(f.state),&f.services));
    check(!fs15_physics_post_step(&f.state,sizeof(f.state)-1,&f.services));
    std::cout<<"Native post-step wait/object/wheel/contact/pending chain and cancellation seam passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
