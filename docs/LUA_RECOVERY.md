# Lua recovery and rebuilt script package

## What was recovered

The local `dataS.gar` contains 470 compiled `.luc` chunks, all with the
standard `ESC LJ 02` header. Their debug data includes source paths, line
information, and local names. The generic loader's custom version-3
transform was identified, but none of these 470 chunks needed it.

The strict recovery pipeline uses the pinned [LJD checkout](https://github.com/Dr-MTN/luajit-decompiler)
listed in `analysis/lua-toolchain.json`. It disables the unsafe extra pass
and does not enable `catch_asserts`. Each output is compiled/loaded with the
pinned official [LuaJIT toolchain](https://github.com/LuaJIT/LuaJIT), built
for x64 with GC64 disabled. No game chunk is executed by these checks.

## Repeated literal adapter

The first pass produced 70 syntax failures. LJD's substitution logic reused
literal AST objects in sibling expressions; its writer tracks object identity
and omitted later occurrences. A call such as `emit(nil, nil, nil)` could
be written as `emit(nil, , )`.

`scripts/ljd_driver.py` clones primitive and constant leaf nodes during slot
substitution. It leaves the third-party checkout unchanged. The authored
fixture `tests/fixtures/repeated_literals.lua` is compiled, decompiled through
the adapter, and recompiled; its stripped bytecode matches exactly.

After this correction, all 470 outputs compile:

| Result | Files | Meaning |
|---|---:|---|
| `exact_match` | 254 | Original and rebuilt chunks produce identical `string.dump(chunk, true)` output under the verification runtime |
| `syntax_only` | 216 | Source compiles and its bytecode loads, but the stripped dump differs; behavior remains unverified |

An exact match is a strong check for that chunk. A syntax-only result must
not be presented as behavioral equivalence. Differences may involve code
generation or actual reconstruction errors and require investigation.

## Built artifact

`scripts/compile_recovered_lua.py` checks each recovered source hash against
the recovery manifest, compiles it without executing its code, assigns a
portable `@dataS/...` chunk name, and rechecks the compiled chunk's comparison
status. It creates:

- `build/recompiled/dataS/`: 470 rebuilt chunks.
- `build/FS15ScriptsRebuilt.zip`: those chunks, a per-file build manifest,
  and a scope/validation notice.

The package is not automatically installed. It is a script-layer artifact,
not an independent native engine or a complete playable reconstruction.
Recovered sources, compiled chunks, and original assets are excluded from
Git; only tools and aggregate results are published.
