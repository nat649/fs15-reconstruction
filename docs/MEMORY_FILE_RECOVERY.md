# Native memory streams and Linux resource staging

The portable core now reconstructs six default memory-stream operations:

| Native entry | Behavior |
|---|---|
| `0x14006bc00` | Valid when the stored data pointer is nonnull |
| `0x14006bcf0` | Copy the remaining requested bytes, clamp at size and advance position |
| `0x14006bd50` | Clamp an absolute seek to size and return a boolean success |
| `0x14005f670` | Return the stored DWORD size, including with a null data pointer |
| `0x14006bc10` | Return position, or `0xffffffff` for a null data pointer |
| `0x14006bd60` | Compute EOF from size <= position; no sticky EOF byte |

The x64 record stores the data pointer at `+8`, ownership byte at `+0x10`, size
at `+0x14` and position at `+0x18`. These operations preserve ownership and
other record bytes. Unlike the disk reader, a memory seek above size clamps to
the end, and seeking back clears computed EOF. The original seek returns a
boolean in AL; undefined upper return-register bits are not treated as data.
Null data remains an invalid stream even for a zero-byte read. Size and EOF
queries retain their original independent behavior.

## Discovery and native evidence

Two observed memory-stream tables, `0x1404bbb38` and `0x1404d9ff8`, were audited
over twelve slots each. Six uncovered functions were catalogued without splitting
existing bodies. Four of those new entries are used by this component; two
shared constant-return methods remain analysis-only. Validity and read already
existed in the catalogue. The full local pseudocode export now covers 11,642
eligible functions out of 12,207 catalogue entries, with zero export failures.
This is analysis coverage, not complete buildable GE6 source.

The prior five scene tables retain their separate forty-slot audit. Memory and
scene vtable dependency roots are now reported separately. The complete native
dependency export and source-pinned reconstruction ledger were refreshed.

[Differential evidence](../analysis/memory-file-validation.json) records
**13,366 operations across 238 sequences** against unchanged original routines:
2,658 reads, 2,238 seeks, 2,030 validity queries, 1,950 size queries, 2,263
position queries and 2,227 EOF queries. Complete records, guarded output buffers
and unchanged source buffers match. Cases include empty buffers, embedded NULs,
short/end reads, rewinds, extreme unsigned seeks, nine null-data sequences and
four original local Lua/XML/font/DDS resources. Instruction dumps, original
bytes and detailed vectors remain local.

## Integration and limits

`ReadOnlyMemoryFile` is an authored borrowed-storage owner adapter around the
recovered operations. It never frees or modifies caller storage. An empty view
uses a stable empty byte so its validity does not depend on the C++ container's
empty-data-pointer choice. This initialization is not a recovered native factory.

The bounded script-read helper now feeds Lua through the disk stream, recovered
32 KiB buffered callbacks, then this memory stream. Recovered menu sources and
the script host both use this path. The existing string copy is performed by
the recovered reader without changing bytes or encoding. Bytecode remains
restricted to the separately labelled local parity oracle.

Authored envelope/capacity guards reject malformed records, position beyond size,
invalid spans and the original uint32 addition-overflow read case. Those cases
are outside the differential contract; arbitrary memory is not made safe by
these checks. Native factories/destructors, ownership transfer/free, GAR cipher,
archive providers, writes and full engine startup remain open. Loading the
frontend through these streams does not establish native GE6 Linux startup.

```powershell
python scripts/check_memory_file.py --dll build/linux-native-file/Release/FS15Core.dll
```

The native oracle needs the pinned local Windows analysis image. Portable core
and Linux integration tests use authored fixtures; original-resource Linux
desktop and full gameplay validation remain separate.

All twenty-seven asset-free component/transport tests pass on Windows x64,
Linux x64 and ARM64 Linux under QEMU. The Linux script-host and ten frontend/
tester checks also pass in
[CI run 39123910117](https://github.com/nat649/fs15-reconstruction/actions/runs/39123910117).
The original English menu and localization comparison pass separately on Windows.
Platform results and downloaded ELF/archive hashes are in the validation report.
