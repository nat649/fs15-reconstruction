"""Compare original/recovered Lua gameplay and render-rate/pause invariance.

Only whole chunks loaded through source() switch to original local bytecode.
Authored services and the explicitly documented recovered source slices remain
the same in both modes. This is not full GIANTS/PhysX engine parity.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, required=True)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    root, output = args.root.resolve(), args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    recovery = json.loads((root / 'analysis/lua-recovery.json').read_text())
    host_script_hash = digest(root / 'src/engine/mission.lua')
    executable_hash = digest(args.exe.resolve())
    # Pin both sides of the reference comparison before executing any chunk.
    for row in recovery['files']:
        original = root / 'recovered/luajit/dataS' / row['path']
        source = (root / 'recovered/lua/dataS' / row['path']).with_suffix('.lua')
        if digest(original) != row['input_sha256'] or digest(source) != row['source_sha256']:
            raise AssertionError(f'Lua comparison input changed after recovery: {row["path"]}')
    comparisons = []
    reference_chunks = set()

    def run(map_name, scenario, rate, reference=False, paused=False, selfcheck=False):
        if digest(root / 'src/engine/mission.lua') != host_script_hash or digest(args.exe.resolve()) != executable_hash:
            raise AssertionError('Authored host changed during comparison; restart with fixed inputs')
        label = f'{map_name}-{scenario}-{rate}-{"bytecode" if reference else "source"}'
        if paused:
            label += '-pause'
        if selfcheck:
            label += '-selfcheck'
        print(f'Checking {label}', flush=True)
        report, capture = output / (label + '.json'), output / (label + '.png')
        command = [str(args.exe.resolve()), '--root', str(root), '--map', map_name, '--smoke',
                   '--test-frame-rate', str(rate), '--report', str(report), '--capture', str(capture)]
        if selfcheck:
            command.append('--mission-test')
        else:
            command += ['--scenario', scenario]
        if reference:
            command.append('--reference-bytecode')
        if paused:
            command.append('--test-pause')
        result = subprocess.run(command, timeout=180)
        if result.returncode:
            log = (root / 'logs' / f'engine-{map_name}.log').read_text(errors='replace')
            raise AssertionError(f'{label} exited {result.returncode}:\n{log[-2500:]}')
        data = json.loads(report.read_text())
        mission = data['mission']
        if not data['ready'] or data['original_engine_loaded'] or data['visible_vehicle_gpu_samples'] <= 0:
            raise AssertionError(f'{label}: native gameplay did not render')
        if mission['paused'] or mission['discarded_time_s'] != 0:
            raise AssertionError(f'{label}: simulation is paused or discarded ordinary frame time')
        loaded = mission.pop('reference_bytecode_chunks')
        if reference:
            if loaded <= 0:
                raise AssertionError('Reference run did not execute original Lua bytecode')
            reference_chunks.add(loaded)
        elif loaded != 0:
            raise AssertionError('Normal gameplay unexpectedly loaded reference bytecode')
        if not selfcheck:
            expected = 300 if scenario == 'harvest' else 240
            if mission['simulation_ticks'] != expected:
                raise AssertionError(f'{label}: expected {expected} fixed ticks')
        if selfcheck and mission['checks'] < 43:
            raise AssertionError('Original bytecode did not complete the gameplay assertions')
        if scenario == 'harvest' and not selfcheck and mission['harvested_l'] <= 1:
            raise AssertionError('Original crop work area did not harvest')
        if scenario == 'tip':
            if (mission['unloaded_l'] <= 0 or
                    abs(mission['implement_fill_l'] + mission['unloaded_l'] - mission['implement_capacity_l']) > 0.001 or
                    abs(mission['farm_wheat_l'] - mission['unloaded_l'] - 5000) > 0.001):
                raise AssertionError('Original trailer/farm inventory was not conserved')
        return mission, digest(capture)

    def same(left, right, label):
        if left[0] != right[0]:
            keys = [key for key in left[0] if left[0][key] != right[0].get(key)]
            raise AssertionError(f'{label}: gameplay state differs: {keys}')
        if left[1] != right[1]:
            raise AssertionError(f'{label}: rendered image differs')

    for map_name in ('Map01', 'Map02'):
        for scenario in ('harvest', 'tip'):
            baseline = run(map_name, scenario, 60)
            for rate in (15, 144):
                same(baseline, run(map_name, scenario, rate), f'{map_name}/{scenario}/{rate} Hz')
            same(baseline, run(map_name, scenario, 30, paused=True), f'{map_name}/{scenario}/pause')
            same(baseline, run(map_name, scenario, 60, reference=True), f'{map_name}/{scenario}/original Lua')
            comparisons.append(dict(map=map_name, scenario=scenario, matching_render_rates=[15, 30, 60, 144],
                                    pause_and_focus_checks=True, original_bytecode_matches=True,
                                    simulation_ticks=baseline[0]['simulation_ticks'],
                                    image_sha256=baseline[1]))
        same(run(map_name, 'selfcheck', 60, selfcheck=True),
             run(map_name, 'selfcheck', 60, reference=True, selfcheck=True),
             f'{map_name}/full gameplay fixtures/original Lua')
    summary = dict(status='passed', native_runs=24, maps=['Map01', 'Map02'],
                   original_bytecode_chunks_per_reference_run=sorted(reference_chunks),
                   fixed_step_seconds=1 / 60, scenarios=comparisons,
                   gameplay_assertions_per_map=43,
                   input_manifest_sha256=digest(root / 'analysis/lua-recovery.json'),
                   authored_host_script_sha256=host_script_hash, native_host_sha256=executable_hash,
                   scope='Whole original Lua chunks versus recovered sources in the same authored native host; source slices remain shared; no full GE6/PhysX parity')
    (output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    (root / 'analysis/mission-reliability.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(json.dumps(summary, indent=2))


if __name__ == '__main__':
    main()
