"""Regression check for repeated literal loss using authored source only."""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def main():
    runtime = ROOT / "tools/LuaJIT/src/luajit.exe"
    output = ROOT / "build/literal-regression"
    output.mkdir(parents=True, exist_ok=True)
    original = output / "original.luc"
    reconstructed = output / "reconstructed.lua"
    subprocess.run([str(runtime), str(ROOT / "scripts/lua/compile_chunk.lua"),
                    str(ROOT / "tests/fixtures/repeated_literals.lua"), str(original)], check=True)
    subprocess.run([sys.executable, str(ROOT / "scripts/ljd_driver.py"),
                    str(ROOT / "tools/luajit-decompiler/main.py"), "--file", str(original),
                    "--output", str(reconstructed), "--unsafe", "false"], check=True)
    result = subprocess.run([str(runtime), str(ROOT / "scripts/lua/check_chunk.lua"),
                             str(original), str(reconstructed)], check=True, capture_output=True, text=True)
    if result.stdout.strip() != "exact_match":
        raise AssertionError(f"Repeated-literal regression did not round trip: {result.stdout}")
    print("Repeated-literal fixture: exact stripped-bytecode round trip")


if __name__ == "__main__":
    main()
