#include "density.h"
#include "../core/rle16.h"
#include "../core/api.h"
#include <fstream>
#include <stdexcept>
#include <cstring>
#include <algorithm>
namespace fs15world {
DensityGrid load_grle(const std::filesystem::path& path){
    std::ifstream f(path,std::ios::binary);std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(f)),{});
    if(data.size()<21||std::memcmp(data.data(),"GRLE\1",5))throw std::runtime_error("Unsupported GRLE header");
    auto u32=[&](unsigned offset){std::uint32_t v=0;for(unsigned b=0;b<4;++b)v|=std::uint32_t(data[offset+b])<<(8*b);return v;};
    DensityGrid out;out.width=u32(5);out.height=u32(9);out.value_bytes=u32(13);unsigned size=u32(17);
    if(!out.width||!out.height||out.width>4096||out.height>4096||(out.value_bytes!=1&&out.value_bytes!=2)||size!=data.size()-21||size%out.value_bytes)throw std::runtime_error("Invalid bounded GRLE dimensions/payload");
    out.values.resize(size_t(out.width)*out.height);
    // Check complete run terminators before using the permissive native leaves.
    // File validation is authored; decoding uses recovered GE6 algorithms.
    const unsigned unit=out.value_bytes,maximum=unit==1?255:65535;
    auto word=[&](size_t i){return unsigned(data[i])|(unit==2?unsigned(data[i+1])<<8:0);};
    size_t cursor=21;unsigned previous=0;bool have_previous=false;
    while(cursor<data.size()){
        const auto value=word(cursor);cursor+=unit;
        if(have_previous&&value==previous){
            unsigned extension;
            do{if(cursor>=data.size())throw std::runtime_error("GRLE truncated run count");extension=word(cursor);cursor+=unit;}while(extension==maximum);
            have_previous=false;
        }else have_previous=true;
        previous=value;
    }
    if(out.value_bytes==2){std::vector<std::uint16_t> words(size/2);for(size_t i=0;i<words.size();++i)words[i]=std::uint16_t(word(21+i*2));
        std::vector<std::uint8_t> decoded(out.values.size()*2);
        auto count=decode_rle16(words.data(),words.size(),16,16,decoded.data(),decoded.size());
        if(count!=out.values.size())throw std::runtime_error("GRLE16 decoded sample count mismatch");
        for(size_t i=0;i<out.values.size();++i)out.values[i]=std::uint16_t(decoded[i*2]|unsigned(decoded[i*2+1])<<8);
    }else{
        std::vector<std::uint8_t> decoded(out.values.size());
        auto count=fs15_decode_rle8(data.data()+21,size,8,8,decoded.data(),decoded.size());
        if(count!=out.values.size())throw std::runtime_error("GRLE8 decoded sample count mismatch");
        std::copy(decoded.begin(),decoded.end(),out.values.begin());
    }
    return out;
}
}
