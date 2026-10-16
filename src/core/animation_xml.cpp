// Recovered GE6 XML animation callbacks and numeric grammar; host transport authored.
#include "animation_xml.h"
#include "animation_storage.h"
#include <cmath>
#include <cstring>
#include <map>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
size_t text_length(const char* text,size_t bytes) {
    if(!text||!bytes||bytes>1048577)throw std::runtime_error("Invalid bounded animation attribute");
    const auto* end=static_cast<const char*>(std::memchr(text,0,bytes));if(!end)throw std::runtime_error("Unterminated animation attribute");
    return static_cast<size_t>(end-text);
}
bool digit(char c){return c>='0'&&c<='9';}
float parse(const char* text,size_t length,size_t& cursor) {
    size_t p=0;while(p<length&&text[p]==' ')++p;
    const bool negative=p<length&&text[p]=='-';if(negative)++p;
    float value=0;
    while(p<length&&digit(text[p])){value=static_cast<float>(double(text[p]-'0')+double(value)*10.0);++p;}
    if(p<length&&text[p]=='.') {
        ++p;double place=0.1;
        while(p<length&&digit(text[p])){const double part=double(text[p]-'0')*place;place=place*0.1;value=static_cast<float>(part+double(value));++p;}
    }
    if(p<length&&text[p]=='e') {
        ++p;const bool inverse=p<length&&text[p]=='-';if(p<length&&(text[p]=='-'||text[p]=='+'))++p;
        uint32_t exponent=0;
        while(p<length&&digit(text[p])){if(exponent>4096)throw std::runtime_error("Exponent work limit");exponent=exponent*10+uint32_t(text[p]-'0');++p;}
        if(exponent>4096)throw std::runtime_error("Exponent work limit");
        float power=1;for(uint32_t i=0;i<exponent;++i)power=static_cast<float>(double(power)*10.0);
        value=inverse?value/power:value*power;
    }
    if(negative){uint32_t bits;std::memcpy(&bits,&value,4);bits^=0x80000000u;std::memcpy(&value,&bits,4);}
    cursor=p;return value;
}
std::vector<float> parse_vector(const char* text,size_t length,size_t capacity) {
    std::vector<float> values;if(!length)return values;size_t start=0,unused=0;
    for(size_t i=0;i<length;++i)if(text[i]==' ') {
        if(values.size()==capacity)return values;
        values.push_back(parse(text+start,length-start,unused));start=i+1;
    }
    if(values.size()<capacity)values.push_back(parse(text+start,length-start,unused));
    return values;
}
struct Attributes {
    const FS15AnimationXmlAttribute* items;size_t count;
    Attributes(const FS15AnimationXmlAttribute* a,size_t n):items(a),count(n) {
        if((n&&!a)||n>4096)throw std::runtime_error("Invalid animation attributes");
        for(size_t i=0;i<n;++i){text_length(a[i].name,a[i].name_bytes);if(a[i].value)text_length(a[i].value,a[i].value_bytes);}
    }
    const FS15AnimationXmlAttribute* find(const char* key)const {for(size_t i=0;i<count;++i)if(std::strcmp(items[i].name,key)==0)return items+i;return nullptr;}
    const char* value(const char* key)const{const auto* a=find(key);return a?a->value:nullptr;}
    bool triple(const char* key,float* out)const {
        const auto* a=find(key);if(!a||!a->value)return false;
        const auto values=parse_vector(a->value,text_length(a->value,a->value_bytes),3);
        if(values.size()!=3)throw std::runtime_error("Incomplete native numeric vector");
        for(size_t i=0;i<3;++i){if(!std::isfinite(values[i]))throw std::runtime_error("Nonfinite animation vector");out[i]=values[i];}return true;
    }
    bool linear(const char* key)const{const char* v=value(key);return v&&std::strcmp(v,"linear")==0;}
};
bool is_false(const char* value) {
    const char* expected="false";
    for(size_t i=0;i<6;++i){unsigned char c=static_cast<unsigned char>(value[i]);if(c>='A'&&c<='Z')c=static_cast<unsigned char>(c+32);if(c!=static_cast<unsigned char>(expected[i]))return false;if(!c)return true;}return true;
}
struct State {
    FS15AnimationXmlServices services;FS15CharacterSet* set=nullptr;FS15AnimationClip* clip=nullptr;FS15AnimationChannel* channel=nullptr;
    uint32_t node_id=0;std::vector<uint32_t> ids;std::map<uint32_t,size_t> ordinals;std::vector<FS15AnimationKey*> rejected;
    explicit State(const FS15AnimationXmlServices& s):services(s){}
    const FS15SceneLifecycleServices& memory()const{return *services.resources.memory;}
    void pending() {
        fs15_animation_storage::destroy_channel(channel,memory());channel=nullptr;
        fs15_animation_storage::destroy_clip(clip,memory());clip=nullptr;
        if(set){if(!fs15_character_set_destruct(set,sizeof(*set),1,services.resources.context,&memory()))throw std::runtime_error("Pending XML set cleanup failed");set=nullptr;}
    }
    void event(unsigned operation,const Attributes& a,int& diagnostic) {
        const auto& m=memory();const auto& resources=services.resources;
        if(operation==FS15_ANIMATION_XML_BEGIN_SET) {
            if(clip||channel)throw std::runtime_error("Nested XML set");
            pending();const char* name=a.value("name");if(!name)name="";
            auto* created=static_cast<FS15CharacterSet*>(m.allocate(m.user,sizeof(FS15CharacterSet)));if(!created)throw std::bad_alloc();
            if(!fs15_character_set_construct(created,sizeof(*created),name,std::strlen(name)+1,nullptr,resources.context,&m)){m.deallocate(m.user,created);throw std::bad_alloc();}
            set=created;ids.clear();ordinals.clear();return;
        }
        if(operation==FS15_ANIMATION_XML_END_SET) {
            if(!set||clip||channel)throw std::runtime_error("Unclosed XML animation members");
            std::vector<FS15SceneStateRecord*> nodes;
            for(uint32_t id:ids) {
                auto* node=resources.resolve(resources.user,id);
                if(!node){pending();resources.loaded(resources.user,nullptr,3);return;}
                nodes.push_back(node);
            }
            if(!fs15_character_set_bind(set,nodes.data(),nodes.size(),resources.context,&m,&diagnostic)) {
                auto* partial=set;set=nullptr;resources.loaded(resources.user,partial,4);
                throw std::runtime_error("XML animation bind failed");
            }
            auto* completed=set;set=nullptr;resources.loaded(resources.user,completed,static_cast<uint32_t>(diagnostic));return;
        }
        if(operation==FS15_ANIMATION_XML_BEGIN_CLIP) {
            if(!set||clip||channel)throw std::runtime_error("Invalid clip nesting");
            const char* name=a.value("name"),*duration=a.value("duration");float value=0;
            if(!duration||!services.duration||!services.duration(resources.user,duration,&value)||!std::isfinite(value))throw std::runtime_error("Invalid clip duration service");
            if(!name)name="";
            clip=fs15_animation_storage::clip(name,std::strlen(name),value,m);return;
        }
        if(operation==FS15_ANIMATION_XML_END_CLIP) {
            if(!set||!clip||channel)throw std::runtime_error("Invalid clip closure");
            if(!fs15_animation_set_add_clip(set,clip,&m))throw std::bad_alloc();
            clip=nullptr;return;
        }
        if(operation==FS15_ANIMATION_XML_BEGIN_CHANNEL) {
            if(!set||!clip||channel)throw std::runtime_error("Invalid channel nesting");
            const char* id=a.value("nodeId");uint32_t next=node_id;
            if(id&&(!services.node_id||!services.node_id(resources.user,id,&next)))throw std::runtime_error("Invalid node ID service");
            node_id=next;channel=fs15_animation_storage::channel(m);return;
        }
        if(operation==FS15_ANIMATION_XML_END_CHANNEL) {
            if(!set||!clip||!channel)throw std::runtime_error("Invalid channel closure");
            if(channel->keys.begin==channel->keys.end){fs15_animation_storage::destroy_channel(channel,m);channel=nullptr;return;}
            if(!fs15_animation_channel_finalize(channel))throw std::runtime_error("XML lookup finalization failed");
            if(ids.size()>=65535&&!ordinals.count(node_id))throw std::runtime_error("Too many XML animation nodes");
            const auto inserted=ordinals.emplace(node_id,ids.size());if(inserted.second)ids.push_back(node_id);
            channel->node_index=static_cast<uint16_t>(inserted.first->second);
            if(!fs15_animation_clip_add_channel(clip,channel,&m))throw std::bad_alloc();
            channel=nullptr;return;
        }
        if(operation!=FS15_ANIMATION_XML_KEY||!channel||!clip||!set)throw std::runtime_error("Invalid key event");
        const auto* time=a.find("time");if(!time||!time->value)return;
        size_t consumed=0;const float at=parse(time->value,text_length(time->value,time->value_bytes),consumed);
        if(!std::isfinite(at))throw std::runtime_error("Nonfinite key time");
        uint32_t flags=0;float translation[3]{},rotation[3]{},scale[3]{},quaternion[4]{};
        if(a.triple("translation",translation)){flags|=1;if(!a.linear("iptin"))flags|=0x10;if(!a.linear("iptout"))flags|=0x80;}
        if(a.triple("rotation",rotation)) {
            for(float& value:rotation)value=(value/180.0f)*3.1415927410125732421875f;
            if(!fs15_animation_euler_quaternion(rotation,quaternion,4))throw std::runtime_error("Invalid XML key rotation");
            flags|=2;if(!a.linear("iprin"))flags|=0x20;if(!a.linear("iprout"))flags|=0x100;
        }
        if(a.triple("scale",scale)){flags|=4;if(!a.linear("ipsin"))flags|=0x40;if(!a.linear("ipsout"))flags|=0x200;}
        bool visible=true;if(const char* v=a.value("visibility")){flags|=8;visible=!is_false(v);}
        FS15AnimationKey* key=nullptr;
        if(!fs15_animation_key_create(at,flags,translation,quaternion,scale,visible,&m,&key))throw std::bad_alloc();
        if(!fs15_animation_channel_add_key(channel,key,&m,&diagnostic)){m.deallocate(m.user,key);throw std::bad_alloc();}
        if(diagnostic){try{rejected.push_back(key);}catch(...){m.deallocate(m.user,key);throw;}}
    }
};
}
extern "C" int fs15_parse_scene_float(const char* text,size_t bytes,float* out,size_t* consumed) {
    if(!out||!consumed)return 0;
    try{const size_t length=text_length(text,bytes);size_t cursor=0;const float value=parse(text,length,cursor);*out=value;*consumed=cursor;return 1;}catch(...){return 0;}
}
extern "C" int fs15_parse_scene_float_vector(const char* text,size_t bytes,float* out,size_t capacity,size_t* written) {
    if(!written||(capacity&&!out)||capacity>1048576)return 0;
    try{const auto values=parse_vector(text,text_length(text,bytes),capacity);if(!values.empty())std::memcpy(out,values.data(),values.size()*4);*written=values.size();return 1;}catch(...){return 0;}
}
extern "C" void* fs15_animation_xml_create(const FS15AnimationXmlServices* s) {
    if(!s||!s->resources.memory||!s->resources.memory->allocate||!s->resources.memory->deallocate||!s->resources.context||!s->resources.resolve||!s->resources.loaded)return nullptr;
    void* storage=s->resources.memory->allocate(s->resources.memory->user,sizeof(State));if(!storage)return nullptr;
    try{return new(storage)State(*s);}catch(...){s->resources.memory->deallocate(s->resources.memory->user,storage);return nullptr;}
}
extern "C" int fs15_animation_xml_event(void* context,unsigned operation,const FS15AnimationXmlAttribute* attributes,size_t count,int* diagnostic) {
    if(!context||!diagnostic||operation>FS15_ANIMATION_XML_KEY)return 0;
    try{Attributes a(attributes,count);int result=0;static_cast<State*>(context)->event(operation,a,result);*diagnostic=result;return 1;}catch(...){return 0;}
}
extern "C" int fs15_animation_xml_view(const void* context,FS15AnimationXmlView* out) {
    if(!context||!out)return 0;
    const auto& s=*static_cast<const State*>(context);*out={s.set,s.clip,s.channel,s.node_id,0,s.ids.size(),s.rejected.size()};return 1;
}
extern "C" int fs15_animation_xml_destroy(void* context) {
    if(!context)return 0;
    auto& s=*static_cast<State*>(context);
    try{s.pending();const auto memory=s.memory();for(auto* key:s.rejected)memory.deallocate(memory.user,key);s.~State();memory.deallocate(memory.user,context);return 1;}catch(...){return 0;}
}
