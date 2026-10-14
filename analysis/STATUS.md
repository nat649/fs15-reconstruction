# Project status — October 14, 2026

**An initial reconstruction milestone is complete. The full game is not rebuilt.**

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
| `0x14000e200` | Native implementation registered as Lua `source`; next decompilation target |
| `0x1400149d0` | Lua adapter associated with `source` |
| `0x14001ea90` | Reference to `dataS.gar` and initialization checks; full role undetermined |
| `0x14002b8c0` | Resource opening through a provider list under a critical section |
| `0x140096580`, `0x140098630` | Archive-related routines with an MD5 collision diagnostic; format unconfirmed |
| `0x14028e3d0` | Selects decoding variants for one- or two-byte input elements |
| `0x14028f430` | RLE8 decoder reconstructed and compared with the original |
| `0x14028f770` | Two-byte input variant; pseudocode available, not reconstructed |

Lua adapter signatures are partly inferred. Ghidra `undefined8` parameters
must not be treated as established source types.

## Remaining work

1. Follow `source` and file providers to identify the GAR reader and recover
   scripts from this exact version.
2. Document native structures and interfaces exposed to Lua.
3. Reconstruct RLE/GRLE variants and other isolated modules using differential
   comparisons with the original code.
4. Reconstruct engine startup, rendering, resources, audio, input, physics,
   and game behavior.
5. Validate a complete rebuilt executable through actual launches and play.

`dataS.gar` and `dataS2.gar` are not ZIP files and have not been extracted.
The locally recovered SDK Lua example is not the full game's Lua source.
Current validation establishes the analysis pipeline, launcher behavior,
and one native routine; it does not establish a rebuilt engine startup.
