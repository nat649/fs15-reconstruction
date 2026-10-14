"""Strict LJD recovery with syntax checks and stripped-bytecode round trips."""
import argparse
from collections import Counter
from concurrent.futures import ThreadPoolExecutor, as_completed
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import sys
from inventory import ROOT, write_json


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=ROOT / "recovered/luajit/dataS")
    parser.add_argument("--output", type=Path, default=ROOT / "recovered/lua/dataS")
    parser.add_argument("--decompiler", type=Path, default=ROOT / "tools/luajit-decompiler/main.py")
    parser.add_argument("--luajit", type=Path, default=ROOT / "tools/LuaJIT/src/luajit.exe")
    parser.add_argument("--workers", type=int, default=4)
    parser.add_argument("--timeout", type=int, default=90)
    args = parser.parse_args()
    if not 1 <= args.workers <= 8:
        parser.error("--workers must be between 1 and 8")
    for tool in (args.decompiler, args.luajit):
        if not tool.is_file():
            parser.error(f"Missing tool: {tool}")
    files = sorted(args.input.rglob("*.luc"))
    if not files:
        parser.error("No recovered .luc files found")
    candidate_root = ROOT / "recovered/lua-candidates" / args.input.name
    log_root = ROOT / "analysis/logs/lua"
    environment = dict(os.environ, PYTHONUTF8="1")

    def recover(source):
        relative = source.relative_to(args.input)
        candidate = (candidate_root / relative).with_suffix(".lua")
        candidate.parent.mkdir(parents=True, exist_ok=True)
        log = (log_root / relative).with_suffix(".log")
        log.parent.mkdir(parents=True, exist_ok=True)
        row = dict(path=relative.as_posix(), input_sha256=hashlib.sha256(source.read_bytes()).hexdigest())
        output = ""
        try:
            result = subprocess.run([sys.executable, str(ROOT / "scripts/ljd_driver.py"), str(args.decompiler), "--file", str(source),
                                     "--output", str(candidate), "--unsafe", "false", "--with-line-numbers"],
                                    cwd=ROOT, env=environment, capture_output=True, text=True,
                                    encoding="utf-8", errors="replace", timeout=args.timeout)
            output = result.stdout + result.stderr
            if result.returncode or not candidate.is_file():
                row["status"] = "decompile_failed"
                row["returncode"] = result.returncode
            else:
                validation = subprocess.run([str(args.luajit), str(ROOT / "scripts/lua/check_chunk.lua"),
                                             str(source), str(candidate)], cwd=ROOT,
                                            capture_output=True, text=True, encoding="utf-8",
                                            errors="replace", timeout=20)
                output += "\n--- Compile/round-trip check ---\n" + validation.stdout + validation.stderr
                if validation.returncode:
                    row["status"] = "original_load_failed" if validation.returncode == 2 else "syntax_failed"
                else:
                    marker = validation.stdout.strip()
                    if marker not in ("exact_match", "syntax_only"):
                        raise ValueError(f"Unexpected validation result: {marker!r}")
                    row["status"] = marker
                    destination = (args.output / relative).with_suffix(".lua")
                    destination.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(candidate, destination)
                    data = destination.read_bytes()
                    row["source_sha256"] = hashlib.sha256(data).hexdigest()
                    row["source_bytes"] = len(data)
        except subprocess.TimeoutExpired:
            row["status"] = "timeout"
            output += "\nTimed out\n"
        except (OSError, ValueError) as error:
            row["status"] = "tool_error"
            output += f"\n{error}\n"
        log.write_text(output, encoding="utf-8")
        return row

    rows = []
    counts = Counter()
    with ThreadPoolExecutor(max_workers=args.workers) as pool:
        futures = [pool.submit(recover, source) for source in files]
        for future in as_completed(futures):
            row = future.result()
            rows.append(row)
            counts[row["status"]] += 1
            if len(rows) % 25 == 0 or len(rows) == len(files):
                print(f"Processed {len(rows)}/{len(files)}: {dict(counts)}", flush=True)
    rows.sort(key=lambda row: row["path"])
    summary = dict(total=len(files), counts=dict(sorted(counts.items())),
                   mode="strict LJD with distinct-literal adapter; unsafe pass disabled; no catch_asserts", executed_game_scripts=False,
                   validation="load/compile without execution; exact_match compares string.dump(chunk, true)",
                   note="syntax_only is not proof of behavioral equivalence; exact_match is a stripped-bytecode round trip")
    write_json(ROOT / "analysis/lua-recovery.json", dict(**summary, files=rows))
    write_json(ROOT / "analysis/lua-recovery-summary.json", summary)
    print(summary, flush=True)


if __name__ == "__main__":
    main()
