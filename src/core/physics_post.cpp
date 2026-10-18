#include "physics_post.h"
#include "wheel_record.h"
#include <cmath>
#include <cstring>
namespace {
template<class T>T get(const void* p,size_t o){T value;std::memcpy(&value,static_cast<const uint8_t*>(p)+o,sizeof(value));return value;}
template<class T>void put(void* p,size_t o,T value){std::memcpy(static_cast<uint8_t*>(p)+o,&value,sizeof(value));}
bool finite(const float* p,size_t n){for(size_t i=0;i<n;++i)if(!std::isfinite(p[i]))return false;return true;}
constexpr uint32_t bound=1048576;
FS15PhysicsTreeNode* tree(void* p,size_t offset){return get<FS15PhysicsTreeNode*>(p,offset);}
template<class F>bool visit(FS15PhysicsTreeNode* head,F callback){
    if(!head||!head->left)return false;
    auto* node=head->left;uint32_t visited=0;
    while(node!=head){
        if(!node||node->nil||++visited>bound||!callback(node->value))return false;
        if(!node->right||node->right==node||node->left==node||node->parent==node)return false;
        if(!node->right->nil){node=node->right;while(node->left&&!node->left->nil){node=node->left;if(++visited>bound)return false;}if(!node->left)return false;}
        else {auto* parent=node->parent;while(parent&&!parent->nil&&node==parent->right){node=parent;parent=parent->parent;if(++visited>bound)return false;}node=parent;}
    }
    return true;
}
uint32_t identity(uintptr_t endpoint){return get<uint32_t>(reinterpret_cast<const void*>(endpoint),0x18);}
}
extern "C" int fs15_physics_wheel_refresh(void* record,size_t bytes,const void* set,const FS15PhysicsPostServices* s){
    if(!record||bytes<0x160||!set||!s||!s->frame)return 0;
    float origin[3],axis[3];for(size_t i=0;i<3;++i){origin[i]=get<float>(record,0xa0+i*4);axis[i]=get<float>(record,0xb0+i*4);}
    const float distance=get<float>(record,0x148),angle=get<float>(record,0x150);
    if(!finite(origin,3)||!finite(axis,3)||!std::isfinite(distance)||!std::isfinite(angle))return 0;
    put(record,0x30,get<uint32_t>(record,0x108));
    const auto contact=get<uintptr_t>(record,0x28);put(record,0x38,uint32_t(0));put(record,0x34,uint8_t(contact!=0));
    if(contact){const auto metadata=get<uintptr_t>(reinterpret_cast<void*>(contact),0x110);if(metadata){const uint32_t flags=get<uint32_t>(reinterpret_cast<void*>(metadata),0);if((flags&1)&&!(flags&0x100)){const auto owner=get<uintptr_t>(reinterpret_cast<void*>(metadata),8);if(!owner)return 0;put(record,0x38,get<uint32_t>(reinterpret_cast<void*>(owner),0x18));}}}
    put(record,0x70,get<uint32_t>(record,0x14c));put(record,0x50,uint32_t(0));
    uint8_t point[16],normal[16];std::memcpy(point,static_cast<uint8_t*>(record)+0x10,16);std::memcpy(normal,record,16);
    std::memcpy(static_cast<uint8_t*>(record)+0x40,point,16);std::memcpy(static_cast<uint8_t*>(record)+0x60,normal,16);
    float p[4];for(size_t i=0;i<3;++i)p[i]=distance*axis[i]+origin[i];p[3]=0;
    float matrix[16];const int32_t index=get<int32_t>(record,0x138);
    if(index<0||!s->frame(s->user,set,index,matrix)||!finite(matrix,16)||!finite(p,3))return 0;
    const float x=p[0]-matrix[12],y=p[1]-matrix[13],z=p[2]-matrix[14];
    for(size_t i=0;i<3;++i)p[i]=(matrix[i]*x+matrix[4+i]*y)+matrix[8+i]*z;
    if(!finite(p,3))return 0;
    put(record,0x90,get<uint32_t>(record,0x10c));std::memcpy(static_cast<uint8_t*>(record)+0x80,p,16);
    put(record,0x94,get<uint32_t>(record,0x154));const float result=s->arctangent?s->arctangent(s->user,angle):std::atan(angle);
    if(!std::isfinite(result))return 0;
    put(record,0x98,result);return 1;
}
extern "C" int fs15_physics_wheels(FS15PhysicsDispatchState* p,size_t bytes,const FS15PhysicsPostServices* s){
    if(!p||bytes<sizeof(*p)||!s)return 0;
    return visit(tree(p,0x250),[&](uintptr_t value){auto* set=reinterpret_cast<fs15::WheelSet*>(value);if(!set||set->count<0||static_cast<uint32_t>(set->count)>bound||(set->count&&!set->records))return false;for(int32_t i=0;i<set->count;++i)if(!fs15_physics_wheel_refresh(set->records+i,sizeof(fs15::WheelRecord),set,s))return false;return true;})?1:0;
}
extern "C" int fs15_physics_contact_notify(const void* pair,uint32_t mask,uint32_t event,const uintptr_t* listeners,size_t count,const FS15PhysicsPostServices* s){
    if(!pair||!s||count>bound||(count&&(!listeners||!s->observer)))return 0;
    const auto a=get<uintptr_t>(pair,0),b=get<uintptr_t>(pair,8);if(!a||!b)return 0;
    const auto ba=get<uintptr_t>(reinterpret_cast<void*>(a),0),bb=get<uintptr_t>(reinterpret_cast<void*>(b),0);if(!ba||!bb)return 0;
    const auto ma=get<uintptr_t>(reinterpret_cast<void*>(ba),0x110),mb=get<uintptr_t>(reinterpret_cast<void*>(bb),0x110);if(!ma||!mb)return 0;
    const uint32_t fa=get<uint32_t>(reinterpret_cast<void*>(ma),0),fb=get<uint32_t>(reinterpret_cast<void*>(mb),0);
    if((fa&0x100)||(fb&0x100)||(event==4&&!(fa&0x40)))return 1;
    const auto first=get<uintptr_t>(reinterpret_cast<void*>(ma),8),second=get<uintptr_t>(reinterpret_cast<void*>(mb),8);
    if(!(fb&0x80)){const uint32_t part=(fb&8)?get<uint32_t>(reinterpret_cast<void*>(mb),0x18):UINT32_MAX;for(size_t i=0;i<count;++i)if(s->observer(s->user,listeners[i],first,second,second,event,part))break;}
    else {const auto parts=get<uintptr_t>(reinterpret_cast<void*>(mb),0x10);for(size_t bit=0;mask;++bit,mask>>=1){if(!(mask&1))continue;if(bit>=s->part_limit||bit>=32||!parts)return 0;const auto target=get<uintptr_t>(reinterpret_cast<void*>(parts),bit*16);for(size_t i=0;i<count;++i)if(s->observer(s->user,listeners[i],first,second,target,event,UINT32_MAX))break;}}
    return 1;
}
extern "C" int fs15_physics_contacts_finish(FS15PhysicsContacts* c,size_t bytes,const uintptr_t* listeners,size_t count,const FS15PhysicsPostServices* s){
    if(!c||bytes<sizeof(*c)||!s||c->count>bound||c->previous_count>bound||c->count>c->capacity||c->previous_count>c->previous_capacity||(c->count&&(!c->current||!c->buckets||!c->capacity||(c->capacity&(c->capacity-1))))||(c->previous_count&&!c->previous))return 0;
    uint32_t i=0;
    while(i<c->previous_count){auto& old=c->previous[i];if(!old.first||!old.second)return 0;const uint32_t a=identity(old.first),b=identity(old.second);uint32_t h=(b<<16)|a;h+=~(h<<15);h=(h^(h>>10))*9;h^=h>>6;h+=~(h<<11);h^=h>>16;
        int32_t index=c->capacity&&c->buckets?c->buckets[h&(c->capacity-1)]:-1;FS15PhysicsPair* found=nullptr;uint32_t steps=0;
        while(index!=-1){if(index<0||static_cast<uint32_t>(index)>=c->count||++steps>c->count)return 0;auto& row=c->current[index];if(!row.first||!row.second)return 0;if(identity(row.first)==a&&identity(row.second)==b){if(row.mask)found=&row;break;}index=row.next;}
        if(found){const uint32_t now=found->mask,previous=old.mask;
            if((~previous&now)&&!fs15_physics_contact_notify(&old,~previous&now,1,listeners,count,s))return 0;
            if((~now&previous)&&!fs15_physics_contact_notify(&old,~now&previous,2,listeners,count,s))return 0;
            if((now&previous)&&!fs15_physics_contact_notify(&old,now&previous,4,listeners,count,s))return 0;
            old.mask=now;found->mask=0;++i;
        }else {if(!fs15_physics_contact_notify(&old,old.mask,2,listeners,count,s))return 0;--c->previous_count;if(i<c->previous_count)c->previous[i]=c->previous[c->previous_count];}
    }
    const uint32_t current_count=c->count;
    if(c->previous_capacity<current_count){if(!s->allocate||!s->release)return 0;auto* replacement=static_cast<FS15PhysicsPreviousPair*>(s->allocate(s->user,size_t(c->capacity)*sizeof(FS15PhysicsPreviousPair),16));if(!replacement)return 0;if(c->previous_count)std::memcpy(replacement,c->previous,size_t(c->previous_count)*sizeof(*replacement));if(c->previous)s->release(s->user,c->previous);c->previous=replacement;c->previous_capacity=c->capacity;}
    for(uint32_t n=0;n<current_count;++n){auto& row=c->current[n];if(!row.mask)continue;if(c->previous_count>=c->previous_capacity||!c->previous)return 0;if(!fs15_physics_contact_notify(&row,row.mask,1,listeners,count,s))return 0;std::memcpy(c->previous+c->previous_count,&row,sizeof(FS15PhysicsPreviousPair));++c->previous_count;row.mask=0;}
    return 1;
}
extern "C" int fs15_physics_contacts_collect(FS15PhysicsContacts* c,size_t bytes,uintptr_t world,const FS15PhysicsPostServices* s){
    if(!c||bytes<sizeof(*c)||!s||!s->lookup||!s->groups||c->count>bound||(c->count&&!c->current))return 0;
    for(uint32_t i=0;i<c->count;++i){auto& row=c->current[i];row.mask=0;
        const auto found=s->lookup(s->user,world,row.first,row.second);if(!found)continue;
        const auto interface_token=get<uintptr_t>(reinterpret_cast<void*>(found),0x10);if(!interface_token)continue;
        auto* vector=reinterpret_cast<uint8_t*>(c)+0x40;if(!s->groups(s->user,interface_token,vector))return 0;
        const int32_t count=get<int32_t>(vector,4);if(count<0||static_cast<uint32_t>(count)>bound)return 0;if(!count)continue;
        const auto groups=get<uintptr_t*>(vector,0x10);if(!groups||!row.second)return 0;
        const auto body=get<uintptr_t>(reinterpret_cast<void*>(row.second),0);if(!body)return 0;
        const auto metadata=get<uintptr_t>(reinterpret_cast<void*>(body),0x110);if(!metadata)return 0;
        if(!(get<uint32_t>(reinterpret_cast<void*>(metadata),0)&0x80)){
            for(int32_t j=0;j<count;++j){if(!groups[j])return 0;if(get<int32_t>(reinterpret_cast<void*>(groups[j]),0x320)>0){row.mask=1;break;}}
        }else {
            for(int32_t j=0;j<count;++j){const auto group=groups[j];if(!group)return 0;const uint32_t rows=get<uint32_t>(reinterpret_cast<void*>(group),0x320);if(rows>s->contact_row_limit)return 0;
                const size_t selector=get<uintptr_t>(reinterpret_cast<void*>(group),0x310)==body?0x78:0x7c;
                for(uint32_t k=0;k<rows;++k){const uint8_t bit=get<uint8_t>(reinterpret_cast<void*>(group),selector+size_t(k)*0xc0);row.mask|=uint32_t(1)<<(bit&31);}
            }
        }
        put(vector,4,uint32_t(0));
    }
    return 1;
}
extern "C" int fs15_physics_post_step(FS15PhysicsDispatchState* p,size_t bytes,const FS15PhysicsPostServices* s){
    if(!p||bytes<sizeof(*p)||!s)return 0;
    if(!p->busy)return 1;
    if(!p->asynchronous)p->busy=0;else if(!s->wait||s->wait(s->user,p->completion_event,UINT32_MAX,0)!=0)return 0;
    if(!visit(tree(p,0x3a8),[&](uintptr_t value){return s->object&&s->object(s->user,value);})||!fs15_physics_wheels(p,bytes,s))return 0;
    auto* raw=reinterpret_cast<uint8_t*>(p);auto* contacts=reinterpret_cast<FS15PhysicsContacts*>(raw+0x48);
    const auto begin=get<uintptr_t>(p,0x198),end=get<uintptr_t>(p,0x1a0);if(end<begin||(end-begin)%sizeof(uintptr_t))return 0;
    if(!fs15_physics_contacts_finish(contacts,sizeof(*contacts),reinterpret_cast<uintptr_t*>(begin),(end-begin)/sizeof(uintptr_t),s))return 0;
    uint32_t i=0,operations=0;
    for(;;){
        uint32_t count=get<uint32_t>(p,0x328);auto** pending=get<uint8_t**>(p,0x320);
        if(count>bound||(count&&!pending)||++operations>bound)return 0;
        if(i>=count)break;
        auto* item=pending[i];if(!item)return 0;
        if(item[0x1c]){++i;continue;}
        --count;put(p,0x328,count);if(i<count)pending[i]=pending[count];
        const auto owner=get<uintptr_t>(item,0x10);if(!owner)return 0;
        const auto callback=get<uintptr_t>(reinterpret_cast<void*>(owner),0x10);if(!callback)continue;
        if(!s->pending)return 0;
        uint8_t cancel=0;
        if(!s->pending(s->user,callback,get<uint32_t>(reinterpret_cast<void*>(owner),4),get<float>(item,0x18),&cancel))return 0;
        // Native cancellation can remove other pending entries or replace the
        // array. Reload count/storage at the next iteration, and owner ID after
        // the callback, rather than retaining stale queue or identifier state.
        if(cancel&&(!s->cancel||!s->cancel(s->user,p,get<uint32_t>(reinterpret_cast<void*>(owner),4))))return 0;
    }
    p->busy=0;return 1;
}
