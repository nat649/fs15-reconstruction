"""Compile the recovered Lua layer into a local package; does not install or run it."""
import argparse
from collections import Counter
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import subprocess
import zipfile
from gar import safe_relative_name
from inventory import ROOT, write_json, sha256


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--luajit", type=Path, default=ROOT / "tools/LuaJIT/src/luajit.exe")
    args = parser.parse_args()
    recovery = json.loads((ROOT / "analysis/lua-recovery.json").read_text(encoding="utf-8"))
    source_root = ROOT / "recovered/lua/dataS"
    output_root = ROOT / "build/recompiled/dataS"
    rows = recovery["files"]
    if any(row["status"] not in ("exact_match", "syntax_only") for row in rows):
        raise ValueError("Resolve failed source recovery before building the complete script package")

    def compile_one(row):
        relative = safe_relative_name(row["path"])
        source = (source_root.joinpath(*relative.parts)).with_suffix(".lua")
        if sha256(source) != row["source_sha256"]:
            raise ValueError(f"Recovered source changed after validation: {relative}")
        output = output_root.joinpath(*relative.parts)
        output.parent.mkdir(parents=True, exist_ok=True)
        # Give chunks a portable game-relative name instead of a developer's path.
        chunk_name = "@dataS/" + relative.with_suffix(".lua").as_posix()
        subprocess.run([str(args.luajit), str(ROOT / "scripts/lua/compile_chunk.lua"),
                        str(source), str(output), chunk_name], check=True, capture_output=True, timeout=20)
        check = subprocess.run([str(args.luajit), str(ROOT / "scripts/lua/check_chunk.lua"),
                                 str(ROOT / "recovered/luajit/dataS" / relative.as_posix()), str(output)],
                                check=True, capture_output=True, text=True, timeout=20)
        if check.stdout.strip() != row["status"]:
            raise ValueError(f"Packaged bytecode validation changed: {relative}")
        return dict(path=relative.as_posix(), status=row["status"], source_sha256=row["source_sha256"],
                    bytecode_sha256=sha256(output), size=output.stat().st_size)

    with ThreadPoolExecutor(max_workers=4) as pool:
        compiled = list(pool.map(compile_one, rows))
    counts = Counter(row["status"] for row in compiled)
    metadata = dict(scope="Recompiled Lua script layer only; native engine is not rebuilt",
                    scripts=len(compiled), counts=dict(counts), installed_into_game=False,
                    executed_game_scripts=False,
                    note="syntax_only files compile but behavioral equivalence remains unverified", files=compiled)
    write_json(ROOT / "analysis/lua-build.json", metadata)
    package = ROOT / "build/FS15ScriptsRebuilt.zip"
    with zipfile.ZipFile(package, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for row in compiled:
            archive.write(output_root / row["path"], "dataS/" + row["path"])
        archive.writestr("BUILD-MANIFEST.json", json.dumps(metadata, indent=2))
        archive.writestr("README.txt", "Recompiled FS15 Lua script layer. This is not a complete rebuilt game.\n"
                          "The manifest separates exact stripped-bytecode round trips from syntax-only results.\n"
                          "Do not treat syntax-only results as verified behavior. No installed game files were replaced.\n")
    summary = dict(scope=metadata["scope"], scripts=len(compiled), counts=dict(counts),
                   package="build/FS15ScriptsRebuilt.zip", package_sha256=sha256(package),
                   package_bytes=package.stat().st_size, installed_into_game=False, executed_game_scripts=False)
    write_json(ROOT / "analysis/lua-build-summary.json", summary)
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
