"""Compare recovered animation/tipping Lua against original normalized bytecode."""
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    traces = {}
    for mode in ("original", "recovered"):
        result = subprocess.run([str(ROOT / "tools/LuaJIT/src/luajit.exe"),
                                 str(ROOT / "tests/fixtures/vehicle_animation.lua"), str(ROOT), mode],
                                capture_output=True, text=True, timeout=30)
        if result.returncode:
            raise AssertionError(f"{mode}: {result.stderr}\n{result.stdout[-1500:]}")
        traces[mode] = result.stdout
    if traces["original"] != traces["recovered"]:
        for i, (a, b) in enumerate(zip(traces["original"].splitlines(), traces["recovered"].splitlines())):
            if a != b:
                raise AssertionError(f"Animation differential mismatch at sample {i}:\n{a}\n{b}")
        raise AssertionError("Animation trace lengths differ")
    lines = traces["original"].splitlines()
    if not lines[-1].startswith("PASS samples="):
        raise AssertionError("Fixture assertions did not finish")
    files = []
    for name in ("scripts/SpecializationUtil", "scripts/vehicles/specializations/AnimatedVehicle", "scripts/vehicles/specializations/Trailer"):
        files.append(dict(path=name, source_sha256=hashlib.sha256((ROOT / f"recovered/lua/dataS/{name}.lua").read_bytes()).hexdigest(),
                          bytecode_sha256=hashlib.sha256((ROOT / f"recovered/luajit/dataS/{name}.luc").read_bytes()).hexdigest()))
    report = dict(scope="Authored service fixtures; recovered Lua versus original normalized bytecode, not full native game parity",
                  samples=int(lines[-1].split("=")[1]), matching_traces=True,
                  scenarios=["forward chained parts", "reverse at double speed", "stop time", "forward/reverse seek", "joint-limit callbacks", "looping", "tip/door/discharge/automatic close"],
                  trace_sha256=hashlib.sha256(traces["original"].encode()).hexdigest(), files=files)
    (ROOT / "analysis/vehicle-animation-validation.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"Original/recovered animation traces match: {report['samples']} samples, seven scenarios")


if __name__ == "__main__":
    main()
