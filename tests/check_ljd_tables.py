"""Check table initialization recovery using authored Lua and exact bytecode."""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def main():
    runtime = ROOT / 'tools/LuaJIT/src/luajit.exe'
    output = ROOT / 'build/table-initialization-regression'
    output.mkdir(parents=True, exist_ok=True)
    original = output / 'original.luc'
    reconstructed = output / 'reconstructed.lua'
    subprocess.run([str(runtime), str(ROOT / 'scripts/lua/compile_chunk.lua'),
                    str(ROOT / 'tests/fixtures/table_initialization.lua'), str(original)], check=True)
    subprocess.run([sys.executable, str(ROOT / 'scripts/ljd_driver.py'),
                    str(ROOT / 'tools/luajit-decompiler/main.py'), '--file', str(original),
                    '--output', str(reconstructed), '--unsafe', 'false'], check=True)
    for chunk in (original, reconstructed):
        subprocess.run([str(runtime), str(ROOT / 'tests/fixtures/check_table_initialization.lua'),
                        str(chunk)], check=True)
    result = subprocess.run([str(runtime), str(ROOT / 'scripts/lua/check_chunk.lua'),
                             str(original), str(reconstructed)], check=True, capture_output=True, text=True)
    if result.stdout.strip() != 'exact_match':
        raise AssertionError(f'Table initialization did not round trip: {result.stdout}')
    print('Table initialization fixture: exact stripped-bytecode round trip')

    # Check the comparison itself: template hash order may vary, whereas
    # changed constants, calls or allocation instructions must still fail.
    comparisons = [
        ('return {a=1,b=2,c=3,d=4,e=5}', 'return {e=5,d=4,c=3,b=2,a=1}', 'exact_match'),
        ('return {a=1,b=2,c=3}', 'return {a=1,b=2,c=4}', 'syntax_only'),
        ('local t={} t.x=17 return t', 'local t={x=17} return t', 'syntax_only'),
        ('return first()', 'return second()', 'syntax_only'),
        ('error("must not execute")', 'error("must not execute")', 'exact_match'),
    ]
    for i, (left, right, expected) in enumerate(comparisons):
        paths = [output / f'comparison-{i}-{side}.lua' for side in ('left', 'right')]
        for path, source in zip(paths, (left, right)):
            path.write_text(source, encoding='utf-8')
        result = subprocess.run([str(runtime), str(ROOT / 'scripts/lua/check_chunk.lua'),
                                 *map(str, paths)], check=True, capture_output=True, text=True)
        if result.stdout.strip() != expected:
            raise AssertionError(f'Comparison control {i}: expected {expected}, got {result.stdout}')
    print('Deterministic comparison: hash-order equality and four comparison/execution controls pass')


if __name__ == '__main__':
    main()
