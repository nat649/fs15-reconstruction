#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

// Reconstructed from the native GE6 leaf 0x14028f770. Unlike the unchecked
// original, input/value/output limits are enforced before any write.
inline std::size_t decode_rle16(const std::uint16_t* input,std::size_t words,unsigned stride,unsigned bits,std::uint8_t* output,std::size_t bytes){
    constexpr auto failure=std::numeric_limits<std::size_t>::max();
    if((!input&&words)||!output||!bits||bits>16||stride<bits||stride>32||bytes>failure/8)return failure;
    std::size_t count=0,cursor=0;std::uint16_t previous=0;bool have_previous=false;
    const unsigned mask=(1u<<bits)-1;
    auto write=[&](std::uint16_t value){
        if(value>mask||count>failure/stride)return false;auto start=count*stride;
        if(start>failure-bits||(start+bits-1)/8>=bytes)return false;
        if(stride==16&&bits==16)std::memcpy(output+count*2,&value,2);
        else for(unsigned b=0;b<bits;++b){auto bit=start+b;auto m=std::uint8_t(1u<<(bit&7));auto& target=output[bit>>3];target=std::uint8_t((target&~m)|(((value>>b)&1u)?m:0));}
        ++count;return true;
    };
    while(cursor<words){auto value=input[cursor++];if(!write(value))return failure;
        if(have_previous&&value==previous){if(cursor<words){std::size_t repeats=0;std::uint16_t extension;
            do{extension=input[cursor++];if(repeats>failure-extension)return failure;repeats+=extension;}while(extension==65535&&cursor<words);
            if(repeats>(bytes*8)/stride-count)return failure;
            for(std::size_t i=0;i<repeats;++i)if(!write(value))return failure;
            have_previous=false;
        }}else have_previous=true;
        previous=value;
    }
    return count;
}
