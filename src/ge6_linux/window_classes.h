#pragma once
// Authored process-local window-class metadata. No OS window is created here.
#include "image_resources.h"
#include <cstddef>
namespace ge6_classes {
#define GE6_CLASS_MS __attribute__((ms_abi))
using Procedure=int64_t(GE6_CLASS_MS*)(uintptr_t,uint32_t,uintptr_t,intptr_t);
struct ClassW {uint32_t style,pad;Procedure procedure;int32_t class_extra,window_extra;uintptr_t instance,icon,cursor,brush;const uint16_t* menu;const uint16_t* name;};
static_assert(sizeof(ClassW)==72&&offsetof(ClassW,procedure)==8&&offsetof(ClassW,name)==64);
inline void(*trace)(const char*)=nullptr;
inline void(*set_error)(uint32_t)=nullptr;
inline void(*stop)(const char*,uintptr_t)=nullptr;
inline void(*record)(const char*,uint16_t,uintptr_t)=nullptr;
struct Class {ClassW value;std::u16string name;std::shared_ptr<ge6_images::Icon> icon;uint32_t windows=0;};
inline std::mutex mutex;
inline std::map<uint16_t,std::shared_ptr<Class>> classes;
inline uint32_t next=0xc000;
inline std::u16string normalized(std::u16string text){for(auto& c:text)if(c>=u'A'&&c<=u'Z')c+=32;return text;}
inline std::shared_ptr<Class> find(uintptr_t instance,const ge6_images::Name& key){for(auto& item:classes)if(item.second->value.instance==instance&&(key.integer?item.first==key.id:normalized(item.second->name)==normalized(key.text)))return item.second;return nullptr;}
inline GE6_CLASS_MS uint16_t add(const ClassW* input){
    trace("RegisterClassW");if(!input){set_error(87);return 0;}
    try{auto key=ge6_images::name(input->name);if(key.integer||key.text.empty()||!input->procedure||input->class_extra<0||input->window_extra<0||input->class_extra>4096||input->window_extra>4096){set_error(87);return 0;}
        if(input->instance!=ge6_images::module){set_error(126);return 0;}
        bool ascii=std::all_of(key.text.begin(),key.text.end(),[](char16_t c){return c<128;});
        if(!ascii||input->menu||input->cursor||input->brush){stop("RegisterClassW",reinterpret_cast<uintptr_t>(__builtin_return_address(0)));return 0;}
        auto icon=input->icon?ge6_images::get(input->icon):nullptr;if(input->icon&&!icon){set_error(6);return 0;}
        std::lock_guard<std::mutex> lock(mutex);if(find(input->instance,key)){set_error(1410);return 0;}if(next>65535){set_error(8);return 0;}
        auto value=std::make_shared<Class>();value->value=*input;value->name=key.text;value->icon=std::move(icon);value->value.name=reinterpret_cast<const uint16_t*>(value->name.c_str());
        auto atom=uint16_t(next++);classes.emplace(atom,value);record("REGISTER",atom,reinterpret_cast<uintptr_t>(input->procedure));return atom;
    }catch(const ge6_images::Failure& failure){set_error(failure.error);return 0;}
}
inline GE6_CLASS_MS int remove(const uint16_t* raw,uintptr_t instance){trace("UnregisterClassW");try{auto key=ge6_images::name(raw);std::lock_guard<std::mutex> lock(mutex);auto value=find(instance,key);if(!value){set_error(1411);return 0;}if(value->windows){set_error(1412);return 0;}for(auto it=classes.begin();it!=classes.end();++it)if(it->second==value){auto atom=it->first;classes.erase(it);record("REMOVE",atom,0);return 1;}}catch(const ge6_images::Failure& failure){set_error(failure.error);}return 0;}
#undef GE6_CLASS_MS
}
