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
    ('scene_group_xml_callbacks', ['0x1400266d0','0x140028150',
        '0x1400281d0','0x14018c1a0','0x14018c370'], 'analysis/scene-groups-validation.json'),
    ('scene_lod_registration_and_distance_cache', ['0x1400d7a70','0x1400d7be0',
        '0x1400d7bb0','0x1400d7620','0x1400d7840','0x1400d78f0',
        '0x1400d60b0','0x1400d7d50','0x1400d7e20','0x1400d7ea0',
        '0x1400d7db0','0x1400b6260','0x140087e50'], 'analysis/scene-lod-validation.json'),
    ('scene_owned_pointer_vectors', ['0x1400d5f30','0x1400d34e0',
        '0x14005a1b0'], 'analysis/scene-owned-validation.json'),
    ('scene_xml_common_and_pending_properties', ['0x140194460','0x14018c100',
        '0x140127910','0x140127d50','0x1401279a0','0x1401497e0',
        '0x1401498b0','0x140149880','0x14009e5b0','0x14009e5f0',
        '0x140129720','0x140299930'], 'analysis/scene-xml-validation.json'),
    ('animation_xml_callbacks', ['0x140026e60','0x140026850','0x140199890',
        '0x140199970','0x140199d70','0x140199e90','0x140199ec0',
        '0x140199fa0','0x14019a100'], 'analysis/animation-xml-validation.json'),
    ('animation_resource_loading', ['0x1402805b0','0x1401cb5c0','0x1401bd190',
        '0x1401bd2f0','0x1401768b0','0x1401751e0','0x1401ca840'],
        'analysis/animation-resources-validation.json'),
    ('animation_playback', ['0x1400524b0','0x140174af0','0x1402806f0','0x1401766d0',
        '0x140176950','0x1401762a0','0x140176330','0x140176370','0x1401763b0',
        '0x140175820'], 'analysis/animation-validation.json'),
    ('shared_scene_animation_lifetime', ['0x140174c10','0x140174cc0','0x140174e10',
        '0x140174e40','0x1401750c0','0x1401750f0','0x1401752f0','0x140176800',
        '0x1401bd100','0x140041980','0x1400b1520','0x1400b1e10'], 'analysis/shared-scene-validation.json'),
    ('scene_attributes', ['0x1400246f0','0x140136870','0x140136980','0x14027e1c0',
        '0x1400e6900','0x1400e69b0','0x14016edc0','0x1401365f0'], 'analysis/scene-attributes-validation.json'),
    ('scene_object_lifecycle', ['0x14004d640','0x14004d760','0x14004d7a0','0x1400b5ec0',
        '0x1400b15e0','0x14010fa90','0x14010fb40','0x1400b1710','0x1401362a0',
        '0x14010fbb0','0x1400b5f90','0x1400b16e0','0x14010fb10'], 'analysis/scene-lifecycle-validation.json'),
    ('scene_hierarchy', ['0x1401363d0','0x1400b17b0','0x1400b19c0'], 'analysis/scene-hierarchy-validation.json'),
    ('scene_trs_and_lua_updates', ['0x14009de00','0x14009d8c0','0x140057980','0x14009e520',
        '0x14010fd00','0x1400e51f0','0x1400e60c0','0x1400e6610','0x1400e5260',
        '0x1400e6130','0x1400e6750'], 'analysis/scene-trs-validation.json'),
    ('scene_state_and_dispatch', ['0x1400b1b00','0x1400b6160','0x1400b61f0',
        '0x1400b15d0','0x14010fce0'], 'analysis/scene-state-validation.json'),
    ('scene_math_and_transforms', ['0x14009da60','0x14009e160','0x14009e0a0','0x14009df40',
        '0x140280380','0x14009dcb0','0x1400b1a50','0x1400e5330','0x1400e5410',
        '0x1400e54e0','0x1400e55f0'], 'analysis/scene-core-validation.json'),
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
