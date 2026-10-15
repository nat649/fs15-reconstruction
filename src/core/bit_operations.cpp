#include "api.h"
#include <cstddef>
#include <cstdint>
#include <cstring>

// Recovered 32-bit scripting operations, native entries 0x1400dc960..0x1400dca20.
// Unsigned arithmetic and explicit shift masks preserve x64 wrapping on other CPUs.
extern "C" FS15_CORE_API int fs15_script_bit_operation(unsigned operation,void* frame_ptr,
    std::size_t bytes) noexcept {
    if(!frame_ptr || bytes<0x100 || operation>FS15_BIT_HIGHEST_SET) {
        return 0;
    }
    auto* frame=static_cast<unsigned char*>(frame_ptr);
    std::uint32_t a=0,b=0,result=0;
    std::memcpy(&a,frame,4);
    std::memcpy(&b,frame+16,4);
    switch(operation) {
        case FS15_BIT_AND: result=a&b; break;
        case FS15_BIT_OR: result=a|b; break;
        case FS15_BIT_NOT: result=~a; break;
        case FS15_BIT_XOR: result=a^b; break;
        case FS15_BIT_SHIFT_LEFT: result=a<<(b&31); break;
        case FS15_BIT_SHIFT_RIGHT: result=a>>(b&31); break;
        case FS15_BIT_HIGHEST_SET:
            while(a>1) {a>>=1;++result;}
            break;
    }
    const std::uint16_t integer_tag=1;
    std::memcpy(frame+0xf0,&result,4);
    std::memcpy(frame+0xf8,&integer_tag,2);
    return 1;
}
