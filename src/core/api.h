#pragma once
#include <stddef.h>
#include <stdint.h>
#include "export.h"
#ifdef __cplusplus
extern "C" {
# define FS15_NOEXCEPT noexcept
#else
# define FS15_NOEXCEPT
#endif
FS15_CORE_API size_t fs15_decode_rle8(const uint8_t*,size_t,unsigned,unsigned,uint8_t*,size_t) FS15_NOEXCEPT;
FS15_CORE_API size_t fs15_decode_rle16(const uint16_t*,size_t,unsigned,unsigned,uint8_t*,size_t) FS15_NOEXCEPT;
FS15_CORE_API int fs15_compute_tire_forces(const float*,float,float,float,float*);
/* Original adapter IDs in address order; float slots use native script tag 3. */
enum FS15WheelQuery {
    FS15_WHEEL_AXLE_SPEED=0, FS15_WHEEL_CONTACT_OBJECT=1, FS15_WHEEL_CONTACT_POINT=2,
    FS15_WHEEL_CONTACT_NORMAL=3, FS15_WHEEL_CONTACT_FORCE=4, FS15_WHEEL_POSITION=5,
    FS15_WHEEL_SLIP=6
};
/* Caller-owned native-layout fixtures. This API does not create physics bodies. */
FS15_CORE_API const void* fs15_resolve_wheel_record(const void*,const void*,int32_t,const void**) FS15_NOEXCEPT;
FS15_CORE_API int fs15_query_wheel_record(unsigned,const void*,size_t,int,void*,size_t) FS15_NOEXCEPT;
enum FS15BitOperation { FS15_BIT_AND=0,FS15_BIT_OR=1,FS15_BIT_NOT=2,FS15_BIT_XOR=3,
                       FS15_BIT_SHIFT_LEFT=4,FS15_BIT_SHIFT_RIGHT=5,FS15_BIT_HIGHEST_SET=6 };
FS15_CORE_API int fs15_script_bit_operation(unsigned,void*,size_t) FS15_NOEXCEPT;
#ifdef __cplusplus
}
#endif
