# Native file-stream recovery and Linux transport

The Linux port now uses four bounded stream operations recovered from GE6:

| Native entry | Recovered behavior |
|---|---|
| `0x14004f310` | Read a DWORD-sized request; clear EOF before an attempted read, set it on a successful short read, return `0xffffffff` on failure |
| `0x14004f410` | Absolute seek through the signed 32-bit Windows distance; preserve EOF on failure, otherwise set it from returned position versus requested position |
| `0x14004f460` | Return the low 32-bit file size or `0xffffffff` |
| `0x14004f4b0` | Query position through a zero-distance relative seek without changing EOF |

The x64 record retains its handle/token at `+8` and EOF byte at `+0x10`.
Other record bytes remain untouched. Instruction inspection confirms that seek
returns a C++ boolean in **AL**; the pseudocode's apparent 32-bit return includes
undefined upper bits. The portable API exposes a defined integer boolean.
Values from `0x80000000` through `0xffffffff` remain signed negative absolute
seek distances and normally fail. Seeking past EOF does not itself set EOF.
A successful zero-byte read clears EOF. Size/position sentinel ambiguities are
retained rather than silently replaced by 64-bit semantics.

## OS boundary and integration

`src/core/file_stream.cpp` contains the recovered control flow. Its explicit
service callbacks are authored OS seams. `FS15FileTransport` supplies a read-only
regular-file adapter: the inspected Win32 APIs on Windows, POSIX `open`, `read`,
`lseek`, `fstat`, `close` on Linux. It owns and closes its OS file token.

The bounded whole-file helper is authored: it checks the full OS size before
allocation and rejects oversize/short reads. It does not change the recovered
DWORD interface or add a native 64-bit file-service claim. The menu uses it for
source scripts, DDS data and bitmap-font metrics; the script host uses it for
localization sources, and the Linux XML adapter uses it before Expat parsing.
Third-party PNG decoders and Windows XmlLite retain their separate transports.

This is not a general Windows compatibility layer, Proton replacement or a
complete native engine startup. Native stream constructors/destructors, string
ownership, archive providers, write/append services, buffered streams, live file
mutation, Windows path/case/locking semantics and the complete VFS remain open.
The native disk-corruption notification branch is surfaced as a callback; its
original logger/global-queue effects have not been validated. POSIX error codes
are mapped to bounded ordinary errors, not claimed as full Win32 error parity.

## Validation

`scripts/check_native_file_stream.py` compares the rebuilt code against the
unchanged routines in the pinned local Windows analysis image. Four kernel IAT
slots are rebound to actual Windows APIs. Ordinary failure scenarios use
explicit injected API seams; they are counted separately from real file I/O.
No PE entry point, Steam initialization or game process is run.

The comparison covers **2,492 operations in 37 sequences**: 692 reads, 554 seeks,
620 size queries and 626 position queries. Complete records and guarded output
buffers match. Inputs include empty files, UTF-8 paths with spaces, embedded
zero bytes, short/exact/zero reads, seeks past EOF, signed-limit failures,
invalid-token branches and eleven injected failure sequences. Four original
local resources cover Lua, localization XML, font metrics and compressed DDS.
Original bytes and detailed instruction dumps remain local. Source hashes,
input hashes and aggregate counts are in [validation evidence](../analysis/file-stream-validation.json).

Two asset-free CTests exercise the recovered control flow and actual OS
transport independently. The full portable suite now has twenty-five tests.
Linux x64/ARM64 execution evidence is separate from the native Windows oracle;
neither establishes complete original-resource Linux gameplay equivalence.
All twenty-five tests and existing frontend checks pass in
[Linux CI run 39122034884](https://github.com/nat649/fs15-reconstruction/actions/runs/39122034884).
The original-resource English menu and localization comparison pass separately
on Windows with the new stream path. The downloaded Linux ELF/runtime archive
hashes are recorded in the same aggregate evidence.

```powershell
python scripts/check_native_file_stream.py --dll build/linux-native-file/Release/FS15Core.dll
```

The reference image and DLL must match the documented local recovery build.
The core tests and Linux runtime require neither original code bytes nor a
Windows runtime.
