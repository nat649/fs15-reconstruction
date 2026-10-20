#pragma once
// Authored resources and class records only; no original icons are embedded.
#include "window_classes.h"
#include <array>
namespace ge6_window_test {
inline __attribute__((ms_abi)) int64_t procedure(uintptr_t,uint32_t,uintptr_t,intptr_t){return 0;}
inline void check(uint32_t& error){
    auto require=[](bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);};
    std::vector<uint8_t> bytes(1024);auto put=[&](size_t at,auto value){std::memcpy(bytes.data()+at,&value,sizeof(value));};
    auto word=[&](size_t at,uint16_t value){put(at,value);};auto dword=[&](size_t at,uint32_t value){put(at,value);};
    constexpr uint32_t rva=0x3000;constexpr uintptr_t instance=0x140000000;
    word(14,2);dword(16,3);dword(20,0x80000040);dword(24,14);dword(28,0x80000060);
    word(0x40+14,1);dword(0x50,7);dword(0x54,0x80000080);
    word(0x60+12,1);word(0x60+14,1);dword(0x70,0x80000180);dword(0x74,0x800000a0);dword(0x78,101);dword(0x7c,0x800000a0);
    word(0x80+14,1);dword(0x90,1033);dword(0x94,0xc0);
    word(0xa0+14,1);dword(0xb0,1033);dword(0xb4,0xe0);
    dword(0xc0,rva+0x300);dword(0xc4,64);dword(0xe0,rva+0x280);dword(0xe4,20);
    const std::u16string label=u"AuthoredIcon";word(0x180,uint16_t(label.size()));std::memcpy(bytes.data()+0x182,label.data(),label.size()*2);
    word(0x282,1);word(0x284,1);bytes[0x286]=bytes[0x287]=2;word(0x28a,1);word(0x28c,32);dword(0x28e,64);word(0x292,7);
    dword(0x300,40);dword(0x304,2);dword(0x308,4);word(0x30c,1);word(0x30e,32);
    const std::array<uint8_t,16> bgra{{255,0,0,255,255,255,255,255,0,0,255,128,0,255,0,255}};
    std::memcpy(bytes.data()+0x328,bgra.data(),16);bytes[0x338]=0x40;
    const std::vector<uint8_t> expected{255,0,0,128,0,255,0,255,0,0,255,255,255,255,255,0};
    ge6_images::install(instance,rva,bytes);auto ordinal=reinterpret_cast<const uint16_t*>(101);auto named=reinterpret_cast<const uint16_t*>(label.c_str());
    require(!ge6_images::load(instance,reinterpret_cast<const uint16_t*>(u"SDL_app"),1,0,0,0)&&error==1814,"Missing named icon/error was fabricated");
    auto first=ge6_images::load(instance,ordinal,1,0,0,0),second=ge6_images::load(instance,named,1,0,0,0);
    require(first&&second&&first!=second&&ge6_images::get(first)->rgba==expected&&ge6_images::get(second)->rgba==expected,"Original-format icon lookup/orientation/alpha/mask differs");
    auto shared=ge6_images::load(instance,ordinal,1,0,0,0x8000);require(shared&&shared==ge6_images::load(instance,reinterpret_cast<const uint16_t*>(u"#101"),1,0,0,0x8000),"Shared icon/decimal ordinal differs");
    ge6_classes::ClassW cls{};cls.style=0x1020;cls.procedure=procedure;cls.instance=instance;cls.icon=first;cls.name=reinterpret_cast<const uint16_t*>(u"AuthoredWindow");
    auto atom=ge6_classes::add(&cls);require(atom&&!ge6_classes::add(&cls)&&error==1410,"Class atom/duplicate detection differs");
    require(ge6_images::destroy(first)&&!ge6_images::get(first),"Nonshared icon not released");
    {std::lock_guard<std::mutex> lock(ge6_classes::mutex);auto value=ge6_classes::find(instance,{0,u"authoredwindow",false});require(value&&value->icon->rgba==expected&&value->value.procedure==procedure&&value->value.style==0x1020,"Class lost copied name, procedure or retained icon pixels");value->windows=1;}
    require(!ge6_classes::remove(reinterpret_cast<const uint16_t*>(uintptr_t(atom)),instance)&&error==1412,"Removed class with window references");
    {std::lock_guard<std::mutex> lock(ge6_classes::mutex);ge6_classes::classes.at(atom)->windows=0;}
    require(ge6_classes::remove(reinterpret_cast<const uint16_t*>(u"AUTHOREDWINDOW"),instance)&&!ge6_classes::remove(cls.name,instance),"Class removal/lifetime differs");
    require(ge6_images::destroy(second)&&!ge6_images::destroy(second)&&ge6_images::destroy(shared)&&ge6_images::get(shared),"Icon lifetime/shared cache differs");
    // Real PNG decoding through the same resource tree, using authored pixels.
    png_image image{};image.version=PNG_IMAGE_VERSION;image.width=2;image.height=2;image.format=PNG_FORMAT_RGBA;png_alloc_size_t size=0;
    require(png_image_write_to_memory(&image,nullptr,&size,0,expected.data(),0,nullptr)!=0,"Cannot size authored PNG");std::vector<uint8_t> png(size);
    require(png_image_write_to_memory(&image,png.data(),&size,0,expected.data(),0,nullptr)!=0,"Cannot encode authored PNG");png.resize(size);png_image_free(&image);
    require(png.size()<=bytes.size()-0x300,"Authored PNG exceeds fixture");std::copy(png.begin(),png.end(),bytes.begin()+0x300);dword(0xc4,uint32_t(png.size()));dword(0x28e,uint32_t(png.size()));
    ge6_images::install(instance,rva,bytes);auto encoded=ge6_images::load(instance,ordinal,1,0,0,0);require(encoded&&ge6_images::get(encoded)->rgba==expected,"PNG icon pixels differ");ge6_images::destroy(encoded);
    // Indexed DIB rows with a two-entry palette and opaque AND mask.
    std::vector<uint8_t> indexed(64);auto store=[&](size_t at,uint32_t value){std::memcpy(indexed.data()+at,&value,4);};store(0,40);store(4,2);store(8,4);indexed[12]=1;indexed[14]=8;store(32,2);indexed[42]=255;indexed[45]=255;indexed[48]=1;indexed[49]=0;indexed[52]=0;indexed[53]=1;
    require(ge6_images::decode(indexed).rgba==std::vector<uint8_t>({255,0,0,255,0,255,0,255,0,255,0,255,255,0,0,255}),"Indexed DIB palette/orientation differs");
    dword(0xc0,rva+1020);dword(0xc4,64);ge6_images::install(instance,rva,bytes);require(!ge6_images::load(instance,ordinal,1,0,0,0)&&error==193,"Out-of-bounds PE image payload accepted");
    dword(0xc0,rva+0x300);dword(0x94,0x80000040);ge6_images::install(instance,rva,bytes);require(!ge6_images::load(instance,ordinal,1,0,0,0)&&error==193,"Resource directory cycle accepted as data");
}
}
