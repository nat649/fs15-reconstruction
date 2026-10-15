#include "density.h"
#include "../core/rle16.h"
#include <fstream>
#include <stdexcept>
#include <cstring>
#include <algorithm>
namespace fs15world {
DensityGrid load_grle(const std::filesystem::path& path){
    std::ifstream f(path,std::ios::binary);std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(f)),{});
    if(data.size()<21||std::memcmp(data.data(),"GRLE\1",5))throw std::runtime_error("Unsupported GRLE header");
    auto u32=[&](unsigned offset){std::uint32_t v;std::memcpy(&v,data.data()+offset,4);return v;};
    DensityGrid out;out.width=u32(5);out.height=u32(9);out.value_bytes=u32(13);unsigned size=u32(17);
    if(!out.width||!out.height||out.width>4096||out.height>4096||(out.value_bytes!=1&&out.value_bytes!=2)||size!=data.size()-21||size%out.value_bytes)throw std::runtime_error("Invalid bounded GRLE dimensions/payload");
    out.values.resize(size_t(out.width)*out.height);
    if(out.value_bytes==2){std::vector<std::uint16_t> words(size/2);std::memcpy(words.data(),data.data()+21,size);
        auto count=decode_rle16(words.data(),words.size(),16,16,reinterpret_cast<std::uint8_t*>(out.values.data()),out.values.size()*2);
        if(count!=out.values.size())throw std::runtime_error("GRLE16 decoded sample count mismatch");
    }else{
        size_t cursor=21,count=0;bool previous_valid=false;std::uint8_t previous=0;
        auto emit=[&](std::uint8_t v,size_t n){if(n>out.values.size()-count)throw std::runtime_error("GRLE8 output overflow");std::fill_n(out.values.begin()+count,n,v);count+=n;};
        while(cursor<data.size()){auto v=data[cursor++];emit(v,1);if(previous_valid&&v==previous){size_t n=0;if(cursor>=data.size())throw std::runtime_error("GRLE8 missing run count");std::uint8_t e;do{if(cursor>=data.size())throw std::runtime_error("GRLE8 truncated extended run");e=data[cursor++];n+=e;}while(e==255);emit(v,n);previous_valid=false;}else previous_valid=true;previous=v;}
        if(count!=out.values.size())throw std::runtime_error("GRLE8 decoded sample count mismatch");
    }
    return out;
}
}
