# GAR 5.3 recovery

This parser targets the two observed FS15 1.4.2.0 archives, with version
word `0x00050003`. It rejects other versions and unsupported record fields.
No original archive data is included in the repository.

## Observed layout

The file begins with a 512-byte header. At offset `0x60` are the four bytes
`GAR `, followed by a little-endian version word at `0x64`. The native reader
at `0x1402a6d70` adds the 32-bit values at `0x68` and `0x6c` to obtain the
index record count for this version. Their broader meaning is not assumed.
For the reference archives these values are `(5, 614)` and `(5, 497)`.

The encrypted index starts at offset 512 and occupies `count * 512` bytes.
Each observed decoded record uses:

| Record offset | Interpretation used by this parser |
|---|---|
| `0x00` | 32-bit record kind, observed value 1 |
| `0x04` | 32-bit flags, observed value 0 |
| `0x08` | Encrypted payload length, interpreted as a 64-bit value |
| `0x10` | Original payload length, interpreted as a 64-bit value |
| `0x18` | Absolute payload offset, interpreted as a 64-bit value |
| `0x20` | Reserved 64-bit value, observed value 0 |
| `0x28` | NUL-terminated UTF-8 path in the remaining 472 bytes |

The upper halves of the observed span values are zero. File payloads use
complete eight-byte encrypted blocks with one to eight padding bytes.
The parser checks index bounds, payload bounds and overlaps, padding length,
name termination, case collisions, path traversal, and Windows path aliases.

## Cipher dependency

`scripts/native_gar.py` requires the exact previously verified analysis image
with SHA-256
`6d1beaf9d7e3c57dd51cec03385a53bda82b4ac946e622d8d2fa592a56d71f9f`.
It reserves the reference image address range in the extraction process,
copies the code/data sections, and calls the cipher initializer
`0x1402e6e60` and buffer decoder `0x1402e7500`. The PE entry point and game
initialization are not called. Key arguments are read from the observed
GAR factory instructions in that local image.

This is an **original-code oracle**, not a standalone cipher reconstruction.
No reference code bytes are distributed. An independent cipher is remaining
work; the current pipeline is a way to recover and validate local inputs for
the reconstruction effort.

## Extraction evidence

The two archives yielded 619 and 502 files. Hash verification covers all
1,121 outputs. Additional checks cover 374 DDS headers, 81 complete PNG
chunk CRC sets, 21 WAV headers, and 470 LuaJIT v2 headers.

Strict XML parsing passed for 134 of 141 XML/I3D files. Seven description
files contain adjacent attributes without required whitespace in their
extracted bytes. Those diagnostics are recorded; extraction does not repair
or rewrite the original files.

Use `scripts/extract_gar.py` followed by `scripts/verify_recovered_files.py`.
The full manifests and original content remain local. Aggregate metadata
is stored in `analysis/*-extraction-summary.json`.
