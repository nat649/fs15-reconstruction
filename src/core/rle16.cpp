#include "rle16.h"
extern "C" __declspec(dllexport) std::size_t fs15_decode_rle16(const std::uint16_t* input,std::size_t words,unsigned stride,unsigned bits,std::uint8_t* output,std::size_t bytes)noexcept{
    return decode_rle16(input,words,stride,bits,output,bytes);
}
