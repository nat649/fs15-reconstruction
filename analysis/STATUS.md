# Project status — October 14, 2026

**The native analysis and Lua recovery milestones are complete. The full game is not rebuilt.**

The reference is Farming Simulator 15 for Windows **1.4.2.0**. Its original
executables were checked after analysis and remain unchanged. Original files
and machine-specific analysis outputs remain local.

## Compiled components

| Component | Source | Validation | Scope |
|---|---|---|---|
| Launcher | `src/launcher/main.cpp` | Nine behavioral checks; dry run against the installed startup configuration | Starts the original game binary; not binary-identical source recovery |
| RLE8-to-bit-vector decoder | `src/core/rle8.cpp` | 266 comparisons with the original native function, covering 853,086 values | One engine routine with a documented valid-input domain |

The RLE reconstruction comes from **0x14028f430**, reached through references
to `src/base/util/RLEUtil.cpp`. Its descriptive name is inferred from code
and those references, not recovered from a debug symbol.

Differential tests execute the original function's **823 bytes** in the test
process and compare its outputs with recompiled C++. The Ghidra export checks
that the leaf has no external calls or memory references. The harness accepts
only the pinned SHA-256 reference; workspace verification also matches it to
the captured game code. No test code is injected into the running game.

Coverage includes 1–8-bit values, selected strides up to 16 bits, literals,
repeat-count extensions, prefilled outputs, and 64 KiB segment boundaries.
The reconstruction adds a checked output-capacity failure. Complete GRLE
files, the 16-bit input variant, arbitrary malformed engine inputs, and
gameplay are not validated yet.

Validation metadata: [reference](rle8-reference.json),
[differential results](rle8-validation.json). Instruction listings and native
reference bytes are generated locally and excluded from Git.

## Decompilation results

### Script layer recovery

The native `source` call chain and archive provider have now been traced.
Both local GAR archives have been extracted:

| Archive | Files | Relevant validation |
|---|---:|---|
| `dataS.gar` | 619 | 470 LuaJIT v2 chunks; 114 strict XML/I3D checks passed; seven original description XML files have missing attribute whitespace |
| `dataS2.gar` | 502 | 20 strict XML/I3D checks passed; asset headers and PNG CRCs checked |

All **1,121** extracted file sizes and hashes were rechecked against their
manifests. Independent checks passed for **374 DDS headers, 81 PNG CRC sets,
21 WAV headers, and 470 LuaJIT headers**. Seven strict XML diagnostics remain
recorded without changing the original extracted bytes.

The original native cipher is still used as a version-pinned oracle.
It is not a reconstructed, independent cipher implementation. The GAR
header/index parser and extraction checks are new code. The installed
game files were not replaced and no DLC path was used for this extraction.

All **470 Lua sources recompile** after a repeated-literal adapter fixes an
LJD AST identity issue. Comparison against the original compiled chunks:

- **254 exact stripped-bytecode round trips**.
- **216 syntax-only results**, with behavioral equivalence still unverified.
- **Zero remaining syntax failures** in this recovery pass.

The rebuilt script package contains 470 recompiled chunks and a per-file
manifest. Chunks use portable game-relative names. The package was not
installed into the game, and game scripts were not executed by validation.
It is available locally at `build/FS15ScriptsRebuilt.zip`.

Public summaries: [recovery](lua-recovery-summary.json),
[script build](lua-build-summary.json), [file verification](recovered-files-verification.json).

### Native analysis

| Analysis | Functions catalogued | Pseudocode exports | Export failures |
|---|---:|---:|---:|
| Original launcher | 654 | 651 | 0 |
| x64 game, initial selection | 12,166 | 249 | 1 |
| x64 game, focused selection | Same catalogue | 44 | 0 |

The game selections overlap. Counts include C/C++ runtime and third-party
library code and are not completion percentages.

Automatic engine analysis completed within 300 seconds. **0x1400f8420**
exceeded its initial 20-second decompilation timeout. Two data-creation
diagnostics remain at `0x140457006` and `0x140457047`; exports and project
saving succeeded. The original PDB is unavailable. Raw pseudocode still
requires type recovery, naming, and reconstruction before compilation.

## Runtime code snapshot

On disk, the game's `.text` section has entropy close to 8 and cannot be
analysed directly as normal machine code. The installed game was started
normally through Steam, and only its loaded code section was copied into
a local PE analysis image.

- Captured code: **4,657,066 bytes**.
- Loaded code entropy: **6.5405**.
- Original x64 binary SHA-256:
  `21ce6fa0f776ad69f5a707a8c391cba6d2b80bdb612ffdf47f109372478b5488`.
- Analysis image SHA-256:
  `6d1beaf9d7e3c57dd51cec03385a53bda82b4ac946e622d8d2fa592a56d71f9f`.

Bytes outside the documented code range were verified unchanged. This partial
snapshot is an analysis input, not a runnable reconstruction or a complete
saved process state.

## Initial native function map

Virtual addresses use the x64 image's preferred base `0x140000000`.

| Address | Observation |
|---|---|
| `0x140011a30` | Lua registration routine; 112 observed name/implementation/wrapper associations indexed locally |
| `0x14000e200` | Native implementation registered as Lua `source`; traced into resource loading |
| `0x1400149d0` | Lua adapter associated with `source` |
| `0x14001ea90` | Reference to `dataS.gar` and initialization checks; full role undetermined |
| `0x14002b8c0` | Resource opening through a provider list under a critical section |
| `0x140096580`, `0x140098630` | Archive-related routines with an MD5 collision diagnostic; format unconfirmed |
| `0x14028e3d0` | Selects decoding variants for one- or two-byte input elements |
| `0x14028f430` | RLE8 decoder reconstructed and compared with the original |
| `0x14028f770` | Two-byte input variant; pseudocode available, not reconstructed |
| `0x14027d5f0` | Archive factory; GAR is attempted before other formats |
| `0x1402a6d70` | GAR header/index loading and original cipher setup |
| `0x1402e6e60`, `0x1402e7500` | Original cipher initializer and buffer decoder used by the local oracle |
| `0x140280060` | Lua resource load; prefers `.luc` and handles the custom version-3 chunk transform |

Lua adapter signatures are partly inferred. Ghidra `undefined8` parameters
must not be treated as established source types.

## Remaining work

1. Investigate the 216 non-identical Lua round trips and validate behavior.
2. Reconstruct the GAR cipher independently and document native Lua interfaces.
3. Reconstruct RLE/GRLE variants and other isolated modules using differential
   comparisons with the original code.
4. Reconstruct engine startup, rendering, resources, audio, input, physics,
   and game behavior.
5. Validate a complete rebuilt executable through actual launches and play.

The recovered game scripts and assets remain local and excluded from Git.
Current validation establishes the analysis pipeline, launcher behavior,
one native routine, archive recovery, and a recompiled Lua layer. It does
not establish a rebuilt engine startup or full gameplay equivalence.
