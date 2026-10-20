#pragma once
// Authored PE resource lookup and image storage for Linux window transport.
// Original bytes are copied privately; PNG decoding is provided by libpng.
#include <png.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>
namespace ge6_images {
#define GE6_IMAGE_MS __attribute__((ms_abi))
inline void(*trace)(const char*)=nullptr;
inline void(*set_error)(uint32_t)=nullptr;
inline void(*stop)(const char*,uintptr_t)=nullptr;
inline void(*record)(const char*,uint32_t,uint32_t)=nullptr;
struct Failure {uint32_t error;};
inline void need(bool ok,uint32_t error=193){if(!ok)throw Failure{error};}
template<class T>inline T field(const std::vector<uint8_t>& bytes,size_t at){need(at<=bytes.size()&&sizeof(T)<=bytes.size()-at);T value;std::memcpy(&value,bytes.data()+at,sizeof(value));return value;}
struct Name {uint32_t id=0;std::u16string text;bool integer=true;};
inline Name name(const uint16_t* raw){
    auto id=reinterpret_cast<uintptr_t>(raw);if(id<=65535)return {uint32_t(id),{},true};
    std::u16string text;for(size_t i=0;i<256;++i){if(!raw[i]){
        if(text.size()>1&&text[0]==u'#'){uint32_t value=0;bool numeric=true;for(size_t j=1;j<text.size();++j){if(text[j]<u'0'||text[j]>u'9'||value>6553){numeric=false;break;}value=value*10+(text[j]-u'0');}if(numeric&&value<=65535)return {value,{},true};}
        return {0,text,false};}text.push_back(char16_t(raw[i]));}throw Failure{87};
}
struct Table {
    uint32_t rva=0;std::vector<uint8_t> bytes;
    uint32_t child(uint32_t directory,const Name& key,uint32_t absent)const{
        uint32_t n=field<uint16_t>(bytes,size_t(directory)+12)+uint32_t(field<uint16_t>(bytes,size_t(directory)+14));
        need(n<=4096&&size_t(directory)+16<=bytes.size()&&size_t(n)*8<=bytes.size()-directory-16);
        for(uint32_t i=0;i<n;++i){size_t at=size_t(directory)+16+i*8;uint32_t k=field<uint32_t>(bytes,at);bool match=false;
            if(k&0x80000000u){size_t p=k&0x7fffffffu;auto length=field<uint16_t>(bytes,p);need(length<=255&&p+2<=bytes.size()&&size_t(length)*2<=bytes.size()-p-2);if(!key.integer&&key.text.size()==length){match=true;for(size_t j=0;j<length;++j)if(char16_t(field<uint16_t>(bytes,p+2+j*2))!=key.text[j])match=false;}}
            else match=key.integer&&key.id==k;
            if(match)return field<uint32_t>(bytes,at+4);
        }throw Failure{absent};
    }
    std::vector<uint8_t> find(uint32_t type,const Name& key)const{
        need(!bytes.empty(),1813);auto directory=child(0,{type,{},true},1813);need(directory&0x80000000u);
        directory=child(directory&0x7fffffffu,key,1814);need(directory&0x80000000u);directory&=0x7fffffffu;
        // Deterministic first available language. Full Windows language fallback
        // is not supplied; original icon names have one language in this image.
        auto n=uint32_t(field<uint16_t>(bytes,size_t(directory)+12))+field<uint16_t>(bytes,size_t(directory)+14);need(n>0,1815);
        auto data=field<uint32_t>(bytes,size_t(directory)+20);need(!(data&0x80000000u));
        uint32_t address=field<uint32_t>(bytes,data),size=field<uint32_t>(bytes,size_t(data)+4);need(address>=rva);
        size_t offset=address-rva;need(offset<=bytes.size()&&size<=bytes.size()-offset);
        return {bytes.begin()+offset,bytes.begin()+offset+size};
    }
};
struct Icon {uint32_t width=0,height=0;std::vector<uint8_t> rgba;bool shared=false;};
inline Icon decode(const std::vector<uint8_t>& bytes){
    Icon out;
    if(bytes.size()>=8&&!png_sig_cmp(bytes.data(),0,8)){
        png_image image{};image.version=PNG_IMAGE_VERSION;
        if(!png_image_begin_read_from_memory(&image,bytes.data(),bytes.size())){png_image_free(&image);throw Failure{193};}
        if(!image.width||!image.height||image.width>1024||image.height>1024){png_image_free(&image);throw Failure{193};}
        out.width=image.width;out.height=image.height;image.format=PNG_FORMAT_RGBA;out.rgba.resize(PNG_IMAGE_SIZE(image));
        auto ok=png_image_finish_read(&image,nullptr,out.rgba.data(),0,nullptr);png_image_free(&image);need(ok!=0);return out;
    }
    need(field<uint32_t>(bytes,0)==40&&field<uint16_t>(bytes,12)==1&&field<uint32_t>(bytes,16)==0);
    int32_t width=field<int32_t>(bytes,4),doubled=field<int32_t>(bytes,8);uint16_t bits=field<uint16_t>(bytes,14);
    need(width>0&&width<=1024&&doubled>0&&doubled<=2048&&!(doubled&1)&&(bits==8||bits==24||bits==32));
    out.width=uint32_t(width);out.height=uint32_t(doubled/2);uint32_t colors=bits==8?field<uint32_t>(bytes,32):0;if(bits==8&&!colors)colors=256;need(colors<=256);
    size_t pixels=40+colors*4,stride=((size_t(width)*bits+31)/32)*4,mask_stride=((size_t(width)+31)/32)*4,mask=pixels+stride*out.height;
    need(mask<=bytes.size()&&mask_stride*out.height<=bytes.size()-mask);out.rgba.resize(size_t(out.width)*out.height*4);
    bool has_alpha=false;if(bits==32)for(uint32_t y=0;y<out.height;++y)for(uint32_t x=0;x<out.width;++x)has_alpha=has_alpha||bytes[pixels+y*stride+x*4+3]!=0;
    for(uint32_t y=0;y<out.height;++y)for(uint32_t x=0;x<out.width;++x){size_t row=out.height-y-1,from=pixels+row*stride+x*(bits/8),to=(size_t(y)*out.width+x)*4;
        if(bits==8){auto index=bytes[from];need(index<colors);from=40+index*4;}
        out.rgba[to]=bytes[from+2];out.rgba[to+1]=bytes[from+1];out.rgba[to+2]=bytes[from];
        bool transparent=(bytes[mask+row*mask_stride+x/8]&(0x80>>(x%8)))!=0;
        out.rgba[to+3]=transparent?0:(bits==32&&has_alpha?bytes[from+3]:255);
    }return out;
}
inline std::mutex mutex;
inline uintptr_t module=0,next=0x74000000;
inline Table table;
inline std::map<uintptr_t,std::shared_ptr<Icon>> icons;
inline std::map<std::tuple<uint32_t,std::u16string,int,int>,uintptr_t> shared;
inline void install(uintptr_t instance,uint32_t rva,std::vector<uint8_t> bytes){std::lock_guard<std::mutex> lock(mutex);module=instance;table={rva,std::move(bytes)};icons.clear();shared.clear();}
inline std::shared_ptr<Icon> get(uintptr_t handle){std::lock_guard<std::mutex> lock(mutex);auto found=icons.find(handle);return found==icons.end()?nullptr:found->second;}
inline GE6_IMAGE_MS uintptr_t load(uintptr_t instance,const uint16_t* raw,uint32_t type,int cx,int cy,uint32_t flags){
    trace("LoadImageW");
    if(type!=1||(flags&~0x8000u)||cx||cy){stop("LoadImageW",reinterpret_cast<uintptr_t>(__builtin_return_address(0)));return 0;}
    try{auto key=name(raw);std::lock_guard<std::mutex> lock(mutex);need(instance&&instance==module,126);
        auto group=table.find(14,key);auto count=field<uint16_t>(group,4);need(field<uint16_t>(group,0)==0&&field<uint16_t>(group,2)==1&&count>0&&size_t(count)*14+6<=group.size());
        auto cache=std::make_tuple(key.integer?key.id:0,key.integer?std::u16string{}:key.text,cx,cy);
        if(flags&0x8000){auto previous=shared.find(cache);if(previous!=shared.end())return previous->second;}
        // cx=cy=0 without DEFAULTSIZE selects the first original group frame.
        auto id=field<uint16_t>(group,18);auto data=table.find(3,{id,{},true});need(data.size()==field<uint32_t>(group,14));
        auto icon=std::make_shared<Icon>(decode(data));auto w=group[6]?uint32_t(group[6]):256,h=group[7]?uint32_t(group[7]):256;need(icon->width==w&&icon->height==h);
        icon->shared=(flags&0x8000)!=0;auto handle=next++;icons.emplace(handle,icon);if(icon->shared)shared.emplace(std::move(cache),handle);record("LOADED",icon->width,icon->height);return handle;
    }catch(const Failure& failure){set_error(failure.error);if(failure.error==1813||failure.error==1814)record("ABSENT",failure.error,0);return 0;}
}
inline GE6_IMAGE_MS int destroy(uintptr_t handle){trace("DestroyIcon");std::lock_guard<std::mutex> lock(mutex);auto found=icons.find(handle);if(found==icons.end()){set_error(6);return 0;}if(!found->second->shared)icons.erase(found);return 1;}
#undef GE6_IMAGE_MS
}
