"""Track pseudocode separately from bounded, source-pinned native reconstructions.

Detailed per-function work stays local. The aggregate is not an engine-completion
percentage: the catalogue includes runtime/library code and discovery is open.
"""
from collections import Counter
import csv
import hashlib
import json
from pathlib import Path
from inventory import ROOT, write_json

COMPONENTS = [
    ('point_transform', ['0x1400bfec0'], 'analysis/transform-validation.json'),
    ('rle8', ['0x14028f430'], 'analysis/rle8-validation.json'),
    ('rle16', ['0x14028f770'], 'analysis/rle16-validation.json'),
    ('tire_math', ['0x1400c24c0'], 'analysis/tire-forces-validation.json'),
    ('wheel_lookup_and_reads', ['0x1400c9e50','0x1400e77e0','0x1400e7850','0x1400e78d0',
                              '0x1400e7990','0x1400e7a30','0x1400e7ac0','0x1400e7b90'],
     'analysis/wheel-queries-validation.json'),
    ('bit_operations', ['0x1400dc960','0x1400dc980','0x1400dc9a0','0x1400dc9c0',
                        '0x1400dc9e0','0x1400dca00','0x1400dca20'], 'analysis/bit-operations-validation.json')
]


def source_pinned(root, report):
    pins = report.get('rebuilt_source_sha256', {})
    if report.get('status') != 'passed' or not pins:
        return False
    for path, digest in pins.items():
        target=(root/path).resolve()
        if not target.is_relative_to((root/'src/core').resolve()) or not target.is_file():
            return False
        if hashlib.sha256(target.read_bytes()).hexdigest() != digest:
            return False
    return True


def main():
    export = ROOT / 'recovered/decompiled/game-full'
    progress = json.loads((export / 'progress.json').read_text())
    if not progress['complete_pass']:
        raise ValueError('Wait for a completed catalogue export before indexing recompilation work')
    with (export / 'decompiled-index.csv').open(newline='') as stream:
        rows = list(csv.DictReader(stream))
    work = {'0x'+row['entry']:dict(entry='0x'+row['entry'],stage='pseudocode_only',
                                 pseudocode=row['file'],size=int(row['size'])) for row in rows}
    if len(work) != progress['eligible'] or any(row['status'] != 'ok' for row in rows):
        raise ValueError('Current pseudocode catalogue is incomplete or contains duplicate entries')
    components = []
    for name, addresses, evidence in COMPONENTS:
        report = json.loads((ROOT/evidence).read_text())
        if not source_pinned(ROOT, report):
            raise ValueError('Native evidence is absent or stale for component: '+name)
        for address in addresses:
            if address not in work or work[address]['stage'] != 'pseudocode_only':
                raise ValueError('Reconstruction entry is absent or duplicated: '+address)
            work[address].update(stage='validated_component_paths',component=name,evidence=evidence,
                                 limitation=report.get('scope','Bounded component comparisons only'))
        components.append(dict(name=name,entries=len(addresses),evidence=evidence))
    write_json(ROOT/'analysis/native-recompilation.json',dict(functions=list(work.values())))
    counts=Counter(row['stage'] for row in work.values())
    discovery_path=ROOT/'analysis/binding-discovery/bindings.csv'
    discovery=[]
    if discovery_path.exists():
        with discovery_path.open(newline='') as stream:
            discovery=list(csv.DictReader(stream))
    write_json(ROOT/'analysis/binding-discovery-summary.json',dict(
        explicit_unique_targets=len(discovery),new_functions=sum(row['status']=='created_from_explicit_binding' for row in discovery),
        scope='Only explicit native binding code labels were audited; existing function bodies were not split; wider discovery remains open'))
    report=dict(catalogued=progress['catalogued'],eligible=progress['eligible'],
                pseudocode_only=counts['pseudocode_only'],validated_component_path_entries=counts['validated_component_paths'],
                components=components,full_native_rebuild_verified=False,switch_build_verified=False,
                scope='Bounded source-pinned component recovery, not a function-completion percentage or a complete GE6/console rebuild')
    write_json(ROOT/'analysis/native-recompilation-summary.json',report)
    print(json.dumps(report,indent=2))


if __name__=='__main__':
    main()
