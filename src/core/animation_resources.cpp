// Native animation resource construction/decoding; guarded host interfaces authored.
#include "animation_resources.h"
#include "scene_math.h"
#include "scene_trs.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <new>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
template<class T>T read(const void* p,size_t offset){T v;std::memcpy(&v,static_cast<const uint8_t*>(p)+offset,sizeof(v));return v;}
template<class T>void write(void* p,size_t offset,T v){std::memcpy(static_cast<uint8_t*>(p)+offset,&v,sizeof(v));}
bool allocator(const FS15SceneLifecycleServices* s){return s&&s->allocate&&s->deallocate;}
bool valid(const FS15SceneVector& v) {
    return v.end>=v.begin&&v.capacity>=v.end&&(v.end-v.begin)%8==0&&(v.capacity-v.begin)%8==0&&
        (v.begin||(!v.end&&!v.capacity))&&(v.end-v.begin)/8<=65535;
}
bool append(FS15SceneVector& v,void* pointer,const FS15SceneLifecycleServices& s) {
    if(!valid(v))return false;
    const size_t count=(v.end-v.begin)/8,capacity=(v.capacity-v.begin)/8;
    if(count==65535)return false;
    if(count==capacity) {
        const size_t grown=std::max(count+1,capacity+capacity/2);
        void* next=s.allocate(s.user,grown*8);if(!next)return false;
        if(count)std::memcpy(next,reinterpret_cast<void*>(v.begin),count*8);
        if(v.begin)s.deallocate(s.user,reinterpret_cast<void*>(v.begin));
        v.begin=reinterpret_cast<uintptr_t>(next);v.end=v.begin+count*8;v.capacity=v.begin+grown*8;
    }
    write(reinterpret_cast<void*>(v.end),0,reinterpret_cast<uintptr_t>(pointer));v.end+=8;return true;
}
void finite(float f){if(!std::isfinite(f))throw std::runtime_error("Nonfinite animation resource");}
void euler_quaternion(const float* rotation,float* out) {
    const float scale[3]={1,1,1},position[3]={0,0,0};float matrix[16];
    if(!fs15_compose_trs(rotation,scale,position,matrix,16)||!fs15_matrix_to_quaternion(matrix,16,out,4))
        throw std::runtime_error("Invalid Euler quaternion");
}
void free_channel(FS15AnimationChannel* channel,const FS15SceneLifecycleServices& s) {
    if(!channel)return;
    for(uintptr_t p=channel->keys.begin;p<channel->keys.end;p+=8)s.deallocate(s.user,reinterpret_cast<void*>(read<uintptr_t>(reinterpret_cast<void*>(p),0)));
    if(channel->keys.begin)s.deallocate(s.user,reinterpret_cast<void*>(channel->keys.begin));
    s.deallocate(s.user,channel);
}
void free_clip(FS15AnimationClip* clip,const FS15SceneLifecycleServices& s) {
    if(!clip)return;
    for(uintptr_t p=clip->channels.begin;p<clip->channels.end;p+=8)free_channel(reinterpret_cast<FS15AnimationChannel*>(read<uintptr_t>(reinterpret_cast<void*>(p),0)),s);
    if(clip->channels.begin)s.deallocate(s.user,reinterpret_cast<void*>(clip->channels.begin));
    if(read<size_t>(clip->name,24)>15)s.deallocate(s.user,reinterpret_cast<void*>(read<uintptr_t>(clip->name,0)));
    s.deallocate(s.user,clip);
}
struct Key {float time;uint32_t flags;float translation[3]{},rotation[3]{},scale[3]{};};
struct Channel {uint32_t node;std::vector<Key> keys;};
struct Clip {std::string name;float duration;std::vector<Channel> channels;};
struct Group {std::string name;std::vector<Clip> clips;};
struct Reader {
    const uint8_t* data;size_t size,offset=0,total_keys=0;
    void need(size_t bytes)const{if(bytes>size-offset)throw std::runtime_error("Truncated animation file");}
    uint32_t integer(){need(4);uint32_t v=uint32_t(data[offset])<<24|uint32_t(data[offset+1])<<16|uint32_t(data[offset+2])<<8|data[offset+3];offset+=4;return v;}
    uint32_t count(){uint32_t n=integer();if(n>65535)throw std::runtime_error("Animation count exceeds bounded layout");return n;}
    float number(){uint32_t u=integer();float f;std::memcpy(&f,&u,4);finite(f);return f;}
    std::string text() {
        const size_t n=integer();if(n>1048576)throw std::runtime_error("Animation string too long");
        const size_t padded=(n+3)&~size_t(3);need(padded);
        std::string result(reinterpret_cast<const char*>(data+offset),n);offset+=padded;
        // The original temporary counted string is passed through a C-string
        // constructor; retain that first-NUL behavior for stored names.
        result.resize(std::strlen(result.c_str()));return result;
    }
    std::vector<Group> parse(bool version) {
        if(version){uint32_t v=integer();if(v!=2&&v!=3)throw std::runtime_error("Unsupported animation version");}
        const uint32_t n=count();std::vector<Group> groups;
        for(uint32_t i=0;i<n;++i) {
            Group group;group.name=text();const uint32_t clips=count();std::set<uint32_t> nodes;
            for(uint32_t j=0;j<clips;++j) {
                Clip clip;clip.name=text();clip.duration=number();const uint32_t channels=count();
                for(uint32_t k=0;k<channels;++k) {
                    Channel channel;channel.node=integer();const uint32_t keys=count();
                    if(total_keys+keys>1048576)throw std::runtime_error("Animation key budget exceeded");
                    total_keys+=keys;
                    for(uint32_t m=0;m<keys;++m) {
                        Key key{};key.time=number();key.flags=integer();
                        if(key.flags&1u)for(float& f:key.translation)f=number();
                        if(key.flags&2u)for(float& f:key.rotation)f=number();
                        if(key.flags&4u)for(float& f:key.scale)f=number();
                        if(m&&(key.time<=channel.keys.back().time||((key.flags^channel.keys.back().flags)&15u)))
                            throw std::runtime_error("Inconsistent animation keys");
                        channel.keys.push_back(key);
                    }
                    if(keys){nodes.insert(channel.node);if(nodes.size()>65535)throw std::runtime_error("Too many animated nodes");}
                    clip.channels.push_back(std::move(channel));
                }
                group.clips.push_back(std::move(clip));
            }
            groups.push_back(std::move(group));
        }
        return groups;
    }
};
FS15AnimationChannel* channel_construct(const FS15SceneLifecycleServices& s) {
    auto* c=static_cast<FS15AnimationChannel*>(s.allocate(s.user,sizeof(FS15AnimationChannel)));
    if(!c)throw std::bad_alloc();
    c->keys={0,0,0};write<uint32_t>(c,0,0);for(float& w:c->weights)w=0;c->first=c->second=nullptr;return c;
}
FS15AnimationClip* clip_construct(const Clip& source,const FS15SceneLifecycleServices& s) {
    auto* c=static_cast<FS15AnimationClip*>(s.allocate(s.user,0x50));if(!c)throw std::bad_alloc();
    c->channels={0,0,0};write<size_t>(c->name,16,0);write<size_t>(c->name,24,15);c->name[0]=0;
    const size_t size=source.name.size();char* storage=reinterpret_cast<char*>(c->name);
    if(size>15) {
        const size_t capacity=size|15u;storage=static_cast<char*>(s.allocate(s.user,capacity+1));
        if(!storage){s.deallocate(s.user,c);throw std::bad_alloc();}
        write(c->name,0,reinterpret_cast<uintptr_t>(storage));write(c->name,24,capacity);
    }
    std::memcpy(storage,source.name.c_str(),size+1);write(c->name,16,size);c->duration=source.duration;return c;
}
}
extern "C" int fs15_animation_euler_quaternion(const float* r,float* out,size_t count) {
    if(!r||!out||count<4)return 0;
    try{float q[4];euler_quaternion(r,q);std::memcpy(out,q,16);return 1;}catch(...){return 0;}
}
extern "C" int fs15_animation_key_create(float time,uint32_t flags,const float* t,const float* q,const float* scale,int visible,
    const FS15SceneLifecycleServices* s,FS15AnimationKey** out) {
    if(!allocator(s)||!out||((flags&1u)&&!t)||((flags&2u)&&!q)||((flags&4u)&&!scale))return 0;
    try {
        finite(time);uint8_t packed[40];const uint16_t header=static_cast<uint16_t>(flags)|(visible?uint16_t(0x8000):0);
        write(packed,0,time);write(packed,4,header);packed[6]=(flags&1u)?12:0;packed[7]=static_cast<uint8_t>(packed[6]+((flags&2u)?8:0));
        size_t used=8;
        if(flags&1u){for(unsigned i=0;i<3;++i)finite(t[i]);std::memcpy(packed+used,t,12);used+=12;}
        if(flags&2u)for(unsigned i:{1u,2u,3u,0u}) {
            finite(q[i]);const float v=q[i]*32767.0f;const float rounded=(v<0?-0.5f:0.5f)+v;
            if(!std::isfinite(rounded)||double(rounded)<-2147483648.0||double(rounded)>=2147483648.0)return 0;
            const uint16_t quantized=static_cast<uint16_t>(static_cast<uint32_t>(static_cast<int32_t>(rounded)));
            write(packed,used,quantized);used+=2;
        }
        if(flags&4u){for(unsigned i=0;i<3;++i)finite(scale[i]);std::memcpy(packed+used,scale,12);used+=12;}
        auto* key=static_cast<FS15AnimationKey*>(s->allocate(s->user,used));if(!key)return 0;
        std::memcpy(key,packed,used);*out=key;return 1;
    }catch(...){return 0;}
}
extern "C" int fs15_animation_channel_add_key(FS15AnimationChannel* c,FS15AnimationKey* key,const FS15SceneLifecycleServices* s,int* diagnostic) {
    if(!c||!key||!allocator(s)||!diagnostic||!valid(c->keys)||!std::isfinite(key->time))return 0;
    *diagnostic=0;const size_t count=(c->keys.end-c->keys.begin)/8;
    if(count) {
        const auto* last=reinterpret_cast<const FS15AnimationKey*>(read<uintptr_t>(reinterpret_cast<void*>(c->keys.end-8),0));if(!last)return 0;
        if(key->time<=last->time){*diagnostic=1;return 1;}
        if((key->flags^last->flags)&15u){*diagnostic=2;return 1;}
    }else c->flags=key->flags&15u;
    if(!append(c->keys,key,*s))return 0;
    c->last_time=key->time;return 1;
}
extern "C" int fs15_animation_channel_finalize(FS15AnimationChannel* c) {
    if(!c||!valid(c->keys))return 0;
    const size_t count=(c->keys.end-c->keys.begin)/8;const auto* keys=reinterpret_cast<const FS15AnimationKey* const*>(c->keys.begin);
    for(size_t i=0;i<count;++i)if(!keys[i]||!std::isfinite(keys[i]->time))return 0;
    const float span=count?keys[count-1]->time-keys[0]->time:0;if(!std::isfinite(span))return 0;
    c->lookup[0]=0;c->index_scale=0;
    if(count&&span>0) {
        const float step=span*0.015625f;c->index_scale=64.0f/span;size_t next=0;
        for(unsigned i=0;i<64;++i){const float time=static_cast<float>(i)*step+keys[0]->time;while(next<count&&keys[next]->time<=time)++next;c->lookup[i]=static_cast<uint16_t>(next);}
    }
    return 1;
}
extern "C" int fs15_animation_clip_add_channel(FS15AnimationClip* c,FS15AnimationChannel* channel,const FS15SceneLifecycleServices* s) {
    return c&&allocator(s)&&append(c->channels,channel,*s);
}
extern "C" int fs15_animation_set_add_clip(FS15CharacterSet* set,FS15AnimationClip* clip,const FS15SceneLifecycleServices* s) {
    if(!set||!set->data||!clip||!allocator(s)||!valid(clip->channels))return 0;
    uint32_t mask=0;for(uintptr_t p=clip->channels.begin;p<clip->channels.end;p+=8) {
        auto* channel=reinterpret_cast<FS15AnimationChannel*>(read<uintptr_t>(reinterpret_cast<void*>(p),0));if(!channel)return 0;mask|=channel->flags;
    }
    set->data->flags|=mask;return append(set->data->clips,clip,*s);
}
extern "C" int fs15_animation_load_binary(const void* input,size_t bytes,int has_version,const FS15AnimationResourceServices* services,FS15AnimationLoadStats* stats) {
    if(!input||!bytes||!services||!allocator(services->memory)||!services->context||!services->resolve||!services->loaded||!stats)return 0;
    Reader reader{static_cast<const uint8_t*>(input),bytes};std::vector<Group> groups;
    try{groups=reader.parse(has_version!=0);}catch(...){return 0;}
    FS15AnimationLoadStats result{reader.offset,static_cast<uint32_t>(groups.size()),0,0,0};const auto& s=*services->memory;
    for(const auto& group:groups) {
        FS15CharacterSet* set=nullptr;FS15AnimationClip* clip=nullptr;FS15AnimationChannel* channel=nullptr;FS15AnimationKey* key=nullptr;
        bool constructed=false,binding=false;
        try {
            set=static_cast<FS15CharacterSet*>(s.allocate(s.user,sizeof(FS15CharacterSet)));if(!set)throw std::bad_alloc();
            if(!fs15_character_set_construct(set,sizeof(*set),group.name.c_str(),group.name.size()+1,nullptr,services->context,&s))throw std::bad_alloc();
            constructed=true;
            std::vector<uint32_t> node_ids;std::map<uint32_t,size_t> ordinals;
            for(const auto& source:group.clips) {
                clip=clip_construct(source,s);
                for(const auto& track:source.channels)if(!track.keys.empty()) {
                    channel=channel_construct(s);
                    for(const auto& frame:track.keys) {
                        float q[4]{};if(frame.flags&2u)euler_quaternion(frame.rotation,q);
                        if(!fs15_animation_key_create(frame.time,frame.flags,frame.translation,q,frame.scale,(frame.flags&0x8000u)!=0,&s,&key))throw std::bad_alloc();
                        int diagnostic=0;if(!fs15_animation_channel_add_key(channel,key,&s,&diagnostic)||diagnostic)throw std::runtime_error("Key insertion failed");key=nullptr;
                    }
                    if(!fs15_animation_channel_finalize(channel))throw std::runtime_error("Lookup generation failed");
                    const auto inserted=ordinals.emplace(track.node,node_ids.size());
                    if(inserted.second)node_ids.push_back(track.node);
                    channel->node_index=static_cast<uint16_t>(inserted.first->second);
                    if(!fs15_animation_clip_add_channel(clip,channel,&s))throw std::bad_alloc();
                    channel=nullptr;
                }
                if(!fs15_animation_set_add_clip(set,clip,&s))throw std::bad_alloc();
                clip=nullptr;
            }
            std::vector<FS15SceneStateRecord*> nodes;bool missing=false;
            for(uint32_t id:node_ids){auto* node=services->resolve(services->user,id);if(!node){missing=true;break;}nodes.push_back(node);}
            if(missing) {
                if(!fs15_character_set_destruct(set,sizeof(*set),1,services->context,&s))throw std::runtime_error("Skipped group cleanup failed");
                set=nullptr;++result.skipped;services->loaded(services->user,nullptr,3);
            }else {
                binding=true;int diagnostic=0;
                if(!fs15_character_set_bind(set,nodes.data(),nodes.size(),services->context,&s,&diagnostic))throw std::runtime_error("Binding failed");
                ++result.loaded;if(diagnostic)++result.binding_warnings;services->loaded(services->user,set,static_cast<uint32_t>(diagnostic));set=nullptr;
            }
        }catch(...) {
            if(key)s.deallocate(s.user,key);
            free_channel(channel,s);free_clip(clip,s);
            if(set) {
                if(binding)services->loaded(services->user,set,4);
                else if(constructed)fs15_character_set_destruct(set,sizeof(*set),1,services->context,&s);
                else s.deallocate(s.user,set);
            }
            *stats=result;return 0;
        }
    }
    *stats=result;return 1;
}
