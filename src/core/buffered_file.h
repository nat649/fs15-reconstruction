#pragma once
#include "file_stream.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Recovered default-disk-stream paths 0x1402e86b0, 0x1402e8830,
 * 0x1402e8930. Native record: disk stream prefix, 32 KiB buffer at +0x48,
 * cursor +0x8048, fill +0x804c, logical position +0x8050, sticky error +0x8054.
 * OS callbacks supply the previously recovered default disk methods; arbitrary
 * derived stream vtables and original open/close allocation are not recovered.
 * Authored capacity/state guards reject invalid memory envelopes unchanged. */
#define FS15_BUFFERED_FILE_BYTES 0x8058u
#define FS15_FILE_BUFFER_BYTES 0x8000u
FS15_CORE_API uint32_t fs15_buffered_file_read(void*,size_t,void*,size_t,uint32_t,const FS15_FileServices*);
/* Origins: 0 absolute, 1 current+offset, 2 size-offset, all uint32 wrapping.
 * Native result 0 means a recognized seek request, even if disk seek failed.
 * UINT32_MAX means invalid origin; sticky error is never cleared by seek. */
FS15_CORE_API uint32_t fs15_buffered_file_seek(void*,size_t,uint32_t,int,const FS15_FileServices*);
/* Native status: zero for an open default disk stream without a sticky read
 * error; UINT32_MAX otherwise. Ordinary EOF does not set the sticky error. */
FS15_CORE_API uint32_t fs15_buffered_file_status(const void*,size_t);
#ifdef __cplusplus
}
#endif
