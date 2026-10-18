# GE6 buffered default-disk reader

Three more native callbacks are reconstructed in portable C++:

| Entry | Behavior |
|---|---|
| `0x1402e86b0` | Read through a 32 KiB buffer; refill through the default disk read method and stop after a short refill |
| `0x1402e8830` | Seek in the cached window or invalidate it and dispatch a disk seek |
| `0x1402e8930` | Report open/default-disk validity and the sticky refill-error state |

The original callback table initializer `0x1402e8960` references these routines.
The recovered code uses the earlier [disk-stream operations](FILE_STREAM_RECOVERY.md)
through explicit OS seams; arbitrary derived stream implementations are outside
this component. Original open/close allocation and write callbacks remain open.

## Preserved behavior

The native layout retains the disk-stream prefix, an inline 32 KiB buffer at
`+0x48`, cursor at `+0x8048`, fill count at `+0x804c`, logical position at
`+0x8050` and sticky error byte at `+0x8054`. The reconstructed API preserves
other bytes. Authored capacity/cursor guards reject malformed envelopes.

A cached seek window includes its first byte and excludes its end. An in-window
seek changes only cursor/logical position and performs no OS operation. A seek
to its end clears both cursor and fill before calling the disk seek method.
Offsets wrap as unsigned 32-bit values. Origin 1 computes current + offset;
origin 2 computes size **minus** offset. It is not POSIX's end + offset rule.

The original seek callback ignores the disk method's failure return, updates
its logical position on that path, and returns zero for recognized origins.
That result does not prove a successful disk movement. Invalid origin returns
`0xffffffff`. An unknown logical position (`0xffffffff`) follows the original
disk-tell branch instead of inventing a cached position.

A short refill stops the current read request after available bytes are copied.
Ordinary EOF does not set the sticky error. A refill error sets that error and
returns the already copied prefix. Seeking does not clear the sticky error;
later reads can still return bytes while status remains an error. These details
are retained for comparison with Windows, rather than normalized to a different
stream API.

## Integration and evidence

`FS15FileTransport::BufferedReadOnlyFile` supplies authored read-only OS ownership
and initializes the native-layout record. The bounded whole-file helper now
uses the recovered buffered reader. This connects it to frontend script,
DDS/font-metrics and Linux XML reads. Third-party image/XML decoding remains
separately identified; this does not establish native GE6 startup or full VFS
compatibility.

The [source-pinned differential evidence](../analysis/buffered-file-validation.json)
covers **2,163 operations across 35 sequences**: 726 reads, 763 seeks and 674
status calls. Complete records (including all cached bytes), guarded output
buffers and **1,680 ordered OS events** match the unchanged original callbacks
and default disk methods. This includes real Windows file I/O, nine fixture file
sizes around empty/32 KiB boundaries, unknown positions, unsigned offset wraps,
null/invalid streams, three explicitly injected refill-error sequences and one
injected short-read sequence. Four original local Lua/XML/font/DDS resources
are read without modification. No original code, resources or detailed vectors
are distributed.

```powershell
python scripts/check_buffered_file.py --dll build/linux-native-file/Release/FS15Core.dll
```

The native oracle requires the pinned local analysis image. Linux core and
transport tests use authored fixtures and do not need original executable
bytes. Original-resource Linux desktop validation, the complete engine loader,
archive decoding, writing/saving and native graphics/physics remain separate.

All twenty-six asset-free component/transport tests pass on Windows x64,
Linux x64 and ARM64 Linux under QEMU. The installed Linux script-host and ten
frontend/tester checks also pass in
[CI run 39122697535](https://github.com/nat649/fs15-reconstruction/actions/runs/39122697535).
The original English frontend and localization parity pass separately on Windows
with the buffered path. ELF/archive hashes and this platform evidence are
recorded in the validation report.
