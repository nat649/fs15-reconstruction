// Recovered GE6 animation algorithms; bounded guards/callback transport authored.
#include "animation.h"
#include "scene_math.h"
#include "scene_rsqrt.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>
namespace {
uint32_t bits(float f){uint32_t u;std::memcpy(&u,&f,4);return u;}
float value(uint32_t u){float f;std::memcpy(&f,&u,4);return f;}
float scalar(const void* p){float f;std::memcpy(&f,p,4);return f;}
void finite(float f){if(!std::isfinite(f))throw std::runtime_error("Nonfinite animation value");}
template<class T>size_t length(const FS15SceneVector& v) {
    if(v.end<v.begin||v.capacity<v.end||(v.end-v.begin)%sizeof(T)||
        (v.end!=v.begin&&!v.begin)||(v.end-v.begin)/sizeof(T)>1048576)
        throw std::runtime_error("Invalid animation vector");
    return (v.end-v.begin)/sizeof(T);
}
template<class T>T& at(const FS15SceneVector& v,size_t i) {
    if(i>=length<T>(v))throw std::runtime_error("Animation index outside vector");
    return reinterpret_cast<T*>(v.begin)[i];
}
void normalize(float* q) {
    for(unsigned i=0;i<4;++i)finite(q[i]);
    // Instruction grouping differs from decompiler's double-expression output.
    const float sum=((q[1]*q[1]+q[2]*q[2])+q[3]*q[3])+q[0]*q[0];finite(sum);
    const float inv=fs15_scene_math_detail::inverse_length(sum);
    for(unsigned i=0;i<4;++i){q[i]=q[i]*inv;finite(q[i]);}
}
void nlerp(const float* a,const float* b,float t,float* out) {
    finite(t);for(unsigned i=0;i<4;++i){finite(a[i]);finite(b[i]);}
    const float dot=((b[1]*a[1]+a[0]*b[0])+a[2]*b[2])+a[3]*b[3];finite(dot);
    for(unsigned i=0;i<4;++i)out[i]=t*((dot>=0?b[i]:value(bits(b[i])^0x80000000u))-a[i])+a[i];
    normalize(out);
}
void decode(const FS15AnimationKey* key,float* q) {
    if(!key)throw std::runtime_error("No selected key");
    const auto* p=reinterpret_cast<const uint8_t*>(key)+8+key->rotation_offset;
    int16_t packed[4];std::memcpy(packed,p,8);
    q[1]=static_cast<float>(packed[0])/32767.0f;q[2]=static_cast<float>(packed[1])/32767.0f;
    q[3]=static_cast<float>(packed[2])/32767.0f;q[0]=static_cast<float>(packed[3])/32767.0f;
    normalize(q);
}
void rotation(const FS15AnimationChannel& channel,float* out) {
    float a[4],b[4];decode(channel.first,a);decode(channel.second,b);
    nlerp(b,a,channel.weights[1],out);
}
struct Sample {FS15AnimationChannel* channel;const FS15AnimationKey* a;const FS15AnimationKey* b;float w[3];};
void sample(FS15AnimationClip& clip,float time,bool loop) {
    finite(time);finite(clip.duration);if(clip.duration<=0)throw std::runtime_error("Invalid clip duration");
    if(time>clip.duration)time=loop?std::fmod(time,clip.duration):clip.duration;
    else if(time<0){time=loop?std::fmod(time,clip.duration):0.0f;if(time<0)time=time+clip.duration;}
    std::vector<Sample> results;
    for(size_t i=0;i<length<FS15AnimationChannel*>(clip.channels);++i) {
        auto* c=at<FS15AnimationChannel*>(clip.channels,i);if(!c)throw std::runtime_error("Null channel");
        const size_t count=length<const FS15AnimationKey*>(c->keys);if(!count)throw std::runtime_error("Empty key vector");
        for(size_t j=0;j<count;++j) {
            auto* k=at<const FS15AnimationKey*>(c->keys,j);if(!k)throw std::runtime_error("Null key");
            finite(k->time);if(j&&k->time<at<const FS15AnimationKey*>(c->keys,j-1)->time)throw std::runtime_error("Unsorted keys");
        }
        const auto* first=at<const FS15AnimationKey*>(c->keys,0);Sample s{c,nullptr,nullptr,{1,1,1}};bool wrap=false;
        finite(c->last_time);finite(c->index_scale);
        if(time<first->time||count<2)s.a=s.b=first;
        else if(c->last_time<=time){s.a=at<const FS15AnimationKey*>(c->keys,count-1);s.b=loop?first:s.a;wrap=loop;}
        else {
            const float bucket=(time-first->time)*c->index_scale;
            if(!std::isfinite(bucket)||bucket<0||static_cast<double>(bucket)>=9223372036854775808.0)throw std::runtime_error("Invalid lookup coordinate");
            const uint32_t low=static_cast<uint32_t>(static_cast<uint64_t>(bucket));
            const size_t start=c->lookup[std::min(low,uint32_t(63))];
            if(!start||start>=count)throw std::runtime_error("Invalid acceleration index");
            for(size_t j=start;j<count;++j) {
                s.b=at<const FS15AnimationKey*>(c->keys,j);
                if(time<=s.b->time){s.a=at<const FS15AnimationKey*>(c->keys,j-1);break;}
            }
            if(!s.a)throw std::runtime_error("No bracketing key");
        }
        const float end=wrap?s.b->time+clip.duration:s.b->time;
        const float w=end<=s.a->time?1.0f:(end-time)/(end-s.a->time);finite(w);
        for(float& f:s.w)f=w;
        const uint16_t flags=(w<=0.5f?s.b:s.a)->flags;
        const unsigned shift=w<=0.5f?4:7;
        if(flags&(7u<<shift)) {
            const float eased=(3.0f-w*2.0f)*(w*w);finite(eased);
            for(unsigned lane=0;lane<3;++lane)if(flags&(1u<<(shift+lane)))s.w[lane]=eased;
        }
        results.push_back(s);
    }
    for(const auto& s:results){s.channel->first=s.a;s.channel->second=s.b;std::memcpy(s.channel->weights,s.w,12);}
}
uint32_t word(const FS15CharacterSet& set,unsigned i){return bits(set.state[i]);}
void word(FS15CharacterSet& set,unsigned i,uint32_t u){set.state[i]=value(u);}
uint8_t dirty(const FS15CharacterSet& set){return static_cast<uint8_t>(set.flags>>8);}
void dirty(FS15CharacterSet& set,uint8_t d){set.flags=static_cast<uint16_t>((set.flags&255u)|(uint16_t(d)<<8));}
FS15AnimationClip& clip(const FS15CharacterSet& set,uint32_t index) {
    if(!set.data)throw std::runtime_error("No shared animation data");
    auto* p=at<FS15AnimationClip*>(set.data->clips,index);if(!p)throw std::runtime_error("Null clip");
    finite(p->duration);if(p->duration<=0)throw std::runtime_error("Invalid duration");return *p;
}
void blend_vector(const FS15AnimationChannel& c,unsigned lane,unsigned offset,float weight,const float* base,float* out) {
    if(!c.first||!c.second)throw std::runtime_error("Missing selected keys");
    const auto* a=reinterpret_cast<const uint8_t*>(c.first)+8+(offset?c.first->scale_offset:0);
    const auto* b=reinterpret_cast<const uint8_t*>(c.second)+8+(offset?c.second->scale_offset:0);
    for(unsigned i=0;i<3;++i) {
        const float av=scalar(a+i*4),bv=scalar(b+i*4);finite(av);finite(bv);
        out[i]=out[i]+(((av-bv)*c.weights[lane]+bv)-base[i])*weight;finite(out[i]);
    }
}
void update(FS15CharacterSet& set,float dt,const FS15AnimationServices& services) {
    finite(dt);if(!(set.flags&255u))return;
    for(unsigned i=0;i<4;++i)if((word(set,i)&3u)==3) {
        const float speed=set.state[8+i];finite(speed);
        if(std::fabs(speed)>0) {
            const float old=set.state[12+i];finite(old);const float next=old+speed*dt;finite(next);
            set.state[12+i]=next;const float duration=clip(set,word(set,4+i)).duration;
            if((word(set,i)&4u)||(speed>0&&old<duration&&next>0)||(speed<0&&old>0&&next<duration))dirty(set,1);
        }
    }
    if(!dirty(set))return;
    for(unsigned i=0;i<4;++i)if((word(set,i)&3u)==3)sample(clip(set,word(set,4+i)),set.state[12+i],(word(set,i)&4u)!=0);
    if(!set.data)throw std::runtime_error("No shared data");
    const size_t count=length<FS15SceneStateRecord*>(set.transforms);
    if(length<float>(set.data->translation)<count*3||length<float>(set.data->scale)<count*3||
        set.data->visibility_count<count||length<uint32_t>(set.data->visibility_words)<(count+31)/32)
        throw std::runtime_error("Incomplete default pose");
    const auto* translation=reinterpret_cast<const float*>(set.data->translation.begin);
    const auto* scale=reinterpret_cast<const float*>(set.data->scale.begin);
    for(size_t n=0;n<count;++n) {
        float t[3],s[3],q[4]{};std::memcpy(t,translation+n*3,12);std::memcpy(s,scale+n*3,12);
        bool visible=(at<uint32_t>(set.data->visibility_words,n/32)&(1u<<(n%32)))!=0;
        uint16_t changed=0;
        for(unsigned i=0;i<4;++i)if((word(set,i)&3u)==3) {
            auto& current=clip(set,word(set,4+i));auto* c=at<FS15AnimationChannel*>(current.channels,n);
            const float w=set.state[16+i];finite(w);if(!c)throw std::runtime_error("Null channel");changed|=c->flags;
            if(c->flags&1u)blend_vector(*c,0,0,w,translation+n*3,t);
            if(c->flags&2u) {
                float r[4];rotation(*c,r);
                const float dot=((r[1]*q[1]+r[0]*q[0])+r[2]*q[2])+r[3]*q[3];
                for(unsigned j=0;j<4;++j){if(dot<0)r[j]=value(bits(r[j])^0x80000000u);q[j]=q[j]+r[j]*w;}
            }
            if(c->flags&4u)blend_vector(*c,2,1,w,scale+n*3,s);
            if(c->flags&8u){if(!c->first)throw std::runtime_error("No visibility key");if(w>0.5f)visible=(c->first->flags&0x8000u)!=0;}
        }
        auto* node=at<FS15SceneStateRecord*>(set.transforms,n);if(!node)throw std::runtime_error("Null transform");
        if(changed&7u) {
            float matrix[16];std::memcpy(matrix,node->local,64);
            if(changed&1u)std::memcpy(matrix+12,t,12);
            if(changed&2u){normalize(q);if(!fs15_quaternion_to_matrix(q,4,matrix,16))throw std::runtime_error("Invalid rotation matrix");}
            if(changed&4u) {
                if(!(changed&2u)) {
                    if(node->flags&0x80u)if(!services.refresh||!services.refresh(services.user,node))throw std::runtime_error("TRS refresh failed");
                    for(unsigned j=0;j<3;++j)if(node->scale[j]!=0)s[j]=s[j]/node->scale[j];
                }
                for(unsigned col=0;col<3;++col)for(unsigned row=0;row<3;++row)matrix[col*4+row]=matrix[col*4+row]*s[col];
            }
            for(float f:matrix)finite(f);
            if(!services.set_matrix||!services.set_matrix(services.user,node,matrix)||
                !services.propagate||!services.propagate(services.user,node,1,0))throw std::runtime_error("Matrix update failed");
        }
        if((changed&8u)&&bool(node->flags&1u)!=visible) {
            node->flags=(node->flags&~1u)|uint32_t(visible);
            if(!services.propagate||!services.propagate(services.user,node,2,0))throw std::runtime_error("Visibility update failed");
        }
    }
    dirty(set,0);
}
}
extern "C" int fs15_animation_rsqrt_table(uint32_t* out,size_t count){if(!out||count<512)return 0;std::memcpy(out,fs15_scene_math_detail::rsqrt_table().data(),2048);return 1;}
extern "C" int fs15_animation_normalize(float* q,size_t count){if(!q||count<4)return 0;try{float a[4];std::memcpy(a,q,16);normalize(a);std::memcpy(q,a,16);return 1;}catch(...){return 0;}}
extern "C" int fs15_animation_nlerp(const float* a,const float* b,float t,float* out,size_t count){if(!a||!b||!out||count<4)return 0;try{float q[4];nlerp(a,b,t,q);std::memcpy(out,q,16);return 1;}catch(...){return 0;}}
extern "C" int fs15_animation_rotation(const FS15AnimationChannel* c,float* out,size_t count){if(!c||!out||count<4)return 0;try{float q[4];rotation(*c,q);std::memcpy(out,q,16);return 1;}catch(...){return 0;}}
extern "C" int fs15_animation_sample(FS15AnimationClip* c,float time,int loop){if(!c)return 0;try{sample(*c,time,loop!=0);return 1;}catch(...){return 0;}}
extern "C" int fs15_animation_set_time(FS15CharacterSet* set,uint32_t i,float time) {
    if(!set||i>=4||!std::isfinite(time))return 0;
    try {
        if((word(*set,i)&3u)==3) {
            const float duration=clip(*set,word(*set,4+i)).duration,old=set->state[12+i];finite(old);
            if(std::clamp(old,0.0f,duration)!=std::clamp(time,0.0f,duration))dirty(*set,dirty(*set)|1u);
        }
        set->state[12+i]=time;return 1;
    }catch(...){return 0;}
}
extern "C" int fs15_animation_set_weight(FS15CharacterSet* set,uint32_t i,float weight) {
    if(!set||i>=4||!std::isfinite(weight))return 0;
    dirty(*set,dirty(*set)==0&&set->state[16+i]==weight?0:1);set->state[16+i]=weight;return 1;
}
extern "C" int fs15_animation_set_clip(FS15CharacterSet* set,uint32_t i,uint32_t index) {
    if(!set||i>=4)return 0;
    if(!(word(*set,i)&1u)||word(*set,4+i)!=index)dirty(*set,dirty(*set)|1u);
    word(*set,i,word(*set,i)|1u);word(*set,4+i,index);return 1;
}
extern "C" int fs15_animation_find_clip(const FS15CharacterSet* set,const char* name,size_t bytes,uint32_t* result) {
    if(!set||!set->data||!name||!bytes||!result||!std::memchr(name,0,bytes))return 0;
    try {
        uint32_t found=UINT32_MAX;
        for(size_t i=0;i<length<FS15AnimationClip*>(set->data->clips);++i) {
            auto* c=at<FS15AnimationClip*>(set->data->clips,i);if(!c)throw std::runtime_error("Null named clip");
            size_t size,capacity;std::memcpy(&size,c->name+16,8);std::memcpy(&capacity,c->name+24,8);
            if(size>capacity||size>1048576)throw std::runtime_error("Invalid clip string");
            const char* text=reinterpret_cast<const char*>(c->name);if(capacity>15)std::memcpy(&text,c->name,8);
            if(!text||!std::memchr(text,0,size+1))throw std::runtime_error("Unterminated clip name");
            if(std::strcmp(text,name)==0){found=static_cast<uint32_t>(i);break;}
        }
        *result=found;return 1;
    }catch(...){return 0;}
}
extern "C" int fs15_animation_update(FS15CharacterSet* set,float dt,const FS15AnimationServices* services) {
    if(!set||!services)return 0;
    try{update(*set,dt,*services);return 1;}catch(...){return 0;}
}
