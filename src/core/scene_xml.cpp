// Recovered common I3D node attributes and pending body/joint property metadata.
#include "scene_xml.h"
#include "shared_scene.h"
#include <cmath>
#include <cstring>
#include <new>
#include <stdexcept>
namespace {
size_t length(const char* value,size_t bytes) {
    if(!value||!bytes||bytes>1048577)throw std::runtime_error("Invalid scene XML string");
    const auto* end=static_cast<const char*>(std::memchr(value,0,bytes));if(!end)throw std::runtime_error("Unterminated scene XML string");return static_cast<size_t>(end-value);
}
struct Attributes {
    const FS15SceneXmlAttribute* values;size_t count;
    Attributes(const FS15SceneXmlAttribute* p,size_t n):values(p),count(n) {
        if((n&&!p)||n>4096)throw std::runtime_error("Invalid scene XML attributes");
        for(size_t i=0;i<n;++i){length(p[i].name,p[i].name_bytes);if(p[i].value)length(p[i].value,p[i].value_bytes);}
    }
    const FS15SceneXmlAttribute* find(const char* name)const{for(size_t i=0;i<count;++i)if(std::strcmp(values[i].name,name)==0)return values+i;return nullptr;}
    const char* text(const char* name)const{const auto* a=find(name);return a?a->value:nullptr;}
    bool boolean(const char* name,bool fallback=false)const {
        const char* s=text(name);if(!s)return fallback;const char* f="false";
        for(size_t i=0;i<6;++i){char c=s[i];if(c>='A'&&c<='Z')c=static_cast<char>(c+32);if(c!=f[i])return true;if(!c)return false;}return false;
    }
    bool number(const char* name,float& out)const {
        const auto* a=find(name);if(!a||!a->value)return false;size_t consumed=0;
        if(!fs15_parse_scene_float(a->value,a->value_bytes,&out,&consumed)||!std::isfinite(out))throw std::runtime_error("Invalid scene number");
        return true;
    }
    void triple(const char* name,float* out)const {
        const auto* a=find(name);if(!a||!a->value)return;const size_t bytes=length(a->value,a->value_bytes)+1;size_t offset=0;
        for(unsigned i=0;i<3;++i) {
            if(offset>=bytes)throw std::runtime_error("Incomplete scene TRS triple");
            size_t consumed=0;
            if(!fs15_parse_scene_float(a->value+offset,bytes-offset,out+i,&consumed)||!std::isfinite(out[i]))throw std::runtime_error("Invalid scene TRS value");
            offset+=consumed+1;
        }
    }
};
uint32_t integer(const Attributes& a,const char* name,const FS15SceneXmlServices& s) {
    uint32_t value=0;if(!s.integer||!s.integer(s.user,a.text(name),&value))throw std::runtime_error("Scene integer service failed");return value;
}
void propagate(const FS15SceneXmlServices& s,FS15SceneStateRecord& node,uint32_t mask) {
    if(!s.propagate||!s.propagate(s.user,&node,mask,0))throw std::runtime_error("Scene propagation failed");
}
void replace(FS15SceneStateRecord& node,void* property,const FS15SceneXmlServices& s) {
    if(!fs15_scene_property_replace(&node,sizeof(node),static_cast<FS15ScenePropertyView*>(property),s.memory)) {
        fs15_scene_property_destruct_detached(property,1,s.memory);throw std::runtime_error("Scene property replacement failed");
    }
}
void apply(FS15SceneStateRecord& node,const Attributes& a,bool physics,bool register_id,bool joint,const char* fallback,size_t bytes,const FS15SceneXmlServices& s) {
    const char* name=a.text("name");if(!name){length(fallback,bytes);name=fallback;}
    if(!fs15_scene_rename(&node,sizeof(node),name,std::strlen(name)+1,s.memory))throw std::runtime_error("Scene rename failed");
    float t[3]{},r[3]{},scale[3]={1,1,1};a.triple("translation",t);a.triple("rotation",r);a.triple("scale",scale);
    for(float& v:r)v=(v/180.0f)*3.1415927410125732421875f;
    std::memcpy(node.translation,t,12);std::memcpy(node.rotation,r,12);std::memcpy(node.scale,scale,12);node.flags&=~uint32_t(0x80);
    if(!s.rebuild||!s.rebuild(s.user,&node))throw std::runtime_error("Scene TRS rebuild failed");
    const bool visible=a.boolean("visibility",true);if(bool(node.flags&1u)!=visible){node.flags=(node.flags&~1u)|uint32_t(visible);propagate(s,node,2);}
    if(a.boolean("oc"))node.flags|=0x20u;else node.flags&=~uint32_t(0x20);
    if(physics) {
        uint32_t flags=0;
        if(a.boolean("static"))flags=0x48;else if(a.boolean("dynamic"))flags=0x88;else if(a.boolean("kinematic"))flags=0x108;
        if(a.boolean("compound")){if(flags&8u)flags|=0x200;}else if(a.boolean("compoundChild"))flags|=0x408;
        FS15RigidBodyProperty* body=nullptr;
        if(flags&8u) {
            body=static_cast<FS15RigidBodyProperty*>(s.memory->allocate(s.memory->user,sizeof(FS15RigidBodyProperty)));if(!body)throw std::bad_alloc();
            fs15_scene_property_construct(0,body,sizeof(*body),&node);replace(node,body,s);body->flags|=flags;body->changes|=flags;
        }
        if(a.text("collision")&&!a.boolean("collision")&&body)fs15_scene_property_flags(body,0x800,0,nullptr);
        if(a.boolean("trigger")&&body)fs15_scene_property_flags(body,0x1000,1,nullptr);
        if(a.text("collisionMask")){const uint32_t mask=integer(a,"collisionMask",s);if(body){body->collision_mask=mask;body->changes|=0x40000;}}
        struct FloatField {const char* name;size_t offset;uint32_t dirty;};
        const FloatField fields[]={{"restitution",0x54,0x80000},{"staticFriction",0x58,0x100000},{"dynamicFriction",0x5c,0x100000},{"linearDamping",0x60,0x200000},{"angularDamping",0x64,0x200000}};
        for(const auto& field:fields){float value=0;if(a.number(field.name,value)&&body){std::memcpy(reinterpret_cast<uint8_t*>(body)+field.offset,&value,4);body->changes|=field.dirty;}}
        float density=0;if(a.number("density",density)&&body)fs15_scene_property_density(body,density,nullptr);
        if(a.text("solverIterationCount")){const uint32_t value=integer(a,"solverIterationCount",s);if(body){body->solver_iterations=value;body->changes|=0x2000000;}}
    }
    if(joint&&a.boolean("joint")) {
        auto* p=static_cast<FS15JointProperty*>(s.memory->allocate(s.memory->user,sizeof(FS15JointProperty)));if(!p)throw std::bad_alloc();
        fs15_scene_property_construct(1,p,sizeof(*p),&node);replace(node,p,s);
        if(a.boolean("projection")){p->flags|=0x40;a.number("projDistance",p->projection_distance);a.number("projAngle",p->projection_angle);}
        bool drive=register_id;
        if(a.boolean("xAxisDrive")){p->flags|=0x80;drive=true;}if(a.boolean("yAxisDrive")){p->flags|=0x100;drive=true;}if(a.boolean("zAxisDrive")){p->flags|=0x200;drive=true;}
        if(a.boolean("drivePos"))p->flags|=0x400;
        if(drive){a.number("driveForceLimit",p->drive_force);a.number("driveSpring",p->drive_spring);a.number("driveDamping",p->drive_damping);}
        if(a.boolean("breakableJoint"))p->flags|=0x800;
        a.number("jointBreakForce",p->break_force);a.number("jointBreakTorque",p->break_torque);
        const char* keys[4][3]={{"rotMinX","rotMinY","rotMinZ"},{"rotMaxX","rotMaxY","rotMaxZ"},
            {"transMinX","transMinY","transMinZ"},{"transMaxX","transMaxY","transMaxZ"}};
        for(unsigned i=0;i<3;++i) {
            if(a.number(keys[0][i],p->rotation_min[i]))p->rotation_limited[i]=1;
            if(a.number(keys[1][i],p->rotation_max[i]))p->rotation_limited[i]=1;
            if(a.number(keys[2][i],p->translation_min[i]))p->translation_limited[i]=1;
            if(a.number(keys[3][i],p->translation_max[i]))p->translation_limited[i]=1;
        }
    }
    float distance=0;
    if(a.number("clipDistance",distance)){distance=distance<=0?0:distance;if(node.local_clip[0]!=distance){node.local_clip[0]=distance;propagate(s,node,8);}}
    if(a.number("minClipDistance",distance)){distance=distance>0?distance:0;if(node.local_clip[1]!=distance){node.local_clip[1]=distance;propagate(s,node,8);}}
    if(a.text("objectMask")){uint32_t mask=integer(a,"objectMask",s);if(node.local_masks[0]!=mask){node.local_masks[0]=mask;propagate(s,node,0x10);}}
    if(a.text("lightMask")){uint32_t mask=integer(a,"lightMask",s);if(node.local_masks[1]!=mask){node.local_masks[1]=mask;propagate(s,node,0x20);}}
    const char* id=a.text("nodeId")?"nodeId":a.text("refId")?"refId":nullptr;
    if(id){const uint32_t value=integer(a,id,s);if(register_id&&(!s.register_node||!s.register_node(s.user,value,&node)))throw std::runtime_error("Scene ID registration failed");}
}
}
extern "C" int fs15_scene_xml_find(const FS15SceneXmlAttribute* attrs,size_t count,const char* key,size_t bytes,const char** out) {
    if(!out)return 0;
    try{length(key,bytes);Attributes a(attrs,count);*out=a.text(key);return 1;}catch(...){return 0;}
}
extern "C" int fs15_scene_xml_apply(FS15SceneStateRecord* node,const FS15SceneXmlAttribute* attrs,size_t count,int physics,int register_id,int joint,const char* fallback,size_t bytes,const FS15SceneXmlServices* services) {
    if(!node||!services||!services->memory||!services->memory->allocate||!services->memory->deallocate)return 0;
    try{Attributes a(attrs,count);apply(*node,a,physics!=0,register_id!=0,joint!=0,fallback,bytes,*services);return 1;}catch(...){return 0;}
}
