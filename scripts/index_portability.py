"""Audit known native call dependencies; never equate pseudocode with a port.

Ghidra dependency edges and per-function details remain local. Virtual/computed
calls, dynamic loads, undiscovered functions and incomplete types mean this is
an observed graph, not a complete engine link graph or proof of port readiness.
"""
import csv
import hashlib
import json
from inventory import ROOT, PE, sha256, write_json
from native_gar import DEFAULT_IMAGE, EXPECTED_IMAGE
from index_recompilation import COMPONENTS, source_pinned


def closure(graph, seeds):
    """Include unresolved targets instead of silently discarding missing edges."""
    pending = list(seeds)
    reached, missing = set(), set()
    while pending:
        address = pending.pop()
        if address in reached:
            continue
        reached.add(address)
        if address not in graph:
            missing.add(address)
            continue
        pending.extend(graph[address]['callees'])
    return reached, missing


def main():
    if sha256(DEFAULT_IMAGE) != EXPECTED_IMAGE:
        raise ValueError('Native reference image differs from the pinned analysis')
    path = ROOT/'analysis/native-dependencies/dependencies.csv'
    with path.open(newline='',encoding='utf-8') as stream:
        rows = list(csv.DictReader(stream))
    expected_count = int((path.parent/'dependencies-complete.txt').read_text())
    with (ROOT/'recovered/decompiled/game-full/functions.csv').open(newline='',encoding='utf-8') as stream:
        catalogue = {row['entry']:row for row in csv.DictReader(stream)}
    graph = {}
    for row in rows:
        entry = row['entry']
        if entry in graph or entry not in catalogue:
            raise ValueError('Dependency entry is duplicated or outside the pseudocode catalogue')
        if any(row[field]!=catalogue[entry][field] for field in ('external','thunk')):
            raise ValueError('Dependency and pseudocode catalogues differ')
        graph[entry] = dict(callees=row['callees'].split(';') if row['callees'] else [],
                           external=row['external']=='true',thunk=row['thunk']=='true',
                           call_sites=int(row['call_sites']),computed_call_sites=int(row['computed_call_sites']))
    if len(rows)!=expected_count or graph.keys()!=catalogue.keys():
        raise ValueError('Dependency export is incomplete for the current catalogue')
    recovered = set()
    for _, entries, evidence in COMPONENTS:
        report=json.loads((ROOT/evidence).read_text())
        if not source_pinned(ROOT,report):
            raise ValueError('Recompilation evidence is absent/stale: '+evidence)
        recovered.update(entry.removeprefix('0x') for entry in entries)
    bindings=json.loads((ROOT/'analysis/engine-bindings.json').read_text())['bindings']
    seeds={row['implementation'].removeprefix('0x') for row in bindings}
    roots={'script_descriptors':seeds,'native_crt_startup':{'1403fc96c'}}
    vtable_path=ROOT/'analysis/vtable-discovery/slots.csv'
    if vtable_path.exists():
        with vtable_path.open(newline='',encoding='utf-8') as stream:
            slots=list(csv.DictReader(stream))
        memory_tables={'1404bbb38','1404d9ff8'}
        roots['observed_scene_vtables']={row['target'] for row in slots if row['table'] not in memory_tables}
        roots['observed_memory_stream_vtables']={row['target'] for row in slots if row['table'] in memory_tables}
    audits={}
    for name, targets in roots.items():
        reached, missing=closure(graph,targets)
        external_targets={entry for entry in missing if entry.startswith('EXTERNAL:')}
        eligible={entry for entry in reached if entry in graph and not graph[entry]['external'] and not graph[entry]['thunk']}
        audits[name]=dict(seed_entries=len(targets),known_reachable_entries=len(reached)-len(missing),
            eligible_reachable_entries=len(eligible),bounded_recovered_entries=len(eligible&recovered),
            remaining_entries_without_validated_components=len(eligible-recovered),
            functions_with_computed_calls=sum(graph[entry]['computed_call_sites']>0 for entry in eligible),
            computed_call_sites=sum(graph[entry]['computed_call_sites'] for entry in eligible),
            external_targets_outside_function_catalogue=len(external_targets),
            uncatalogued_native_target_entries=len(missing-external_targets),
            scope='Known static Ghidra edges, including known thunk targets. Computed calls are counted, not closed.')
    image=PE(DEFAULT_IMAGE.read_bytes())
    imports=[dict(library=row['dll'],imports=len(row['functions'])) for row in image.imports()]
    details=dict(reference_sha256=EXPECTED_IMAGE,graph=graph,recovered_entries=sorted(recovered),
                 roots={name:sorted(values) for name,values in roots.items()})
    write_json(ROOT/'analysis/native-portability.json',details)
    summary=dict(reference_sha256=EXPECTED_IMAGE,dependency_export_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
        catalogued_entries=len(graph),known_distinct_call_edges=sum(len(row['callees']) for row in graph.values()),
        eligible_functions_with_undefined_prototypes=sum(
            'undefined' in row['prototype'] for row in catalogue.values() if row['external']=='false' and row['thunk']=='false'),
        native_import_libraries=imports,root_audits=audits,
        full_link_graph_recovered=False,full_native_rebuild_verified=False,switch_build_verified=False,
        scope='Static dependency inventory from the current catalogue. Undefined prototypes are decompiler observations, '
              'not recovered types. Static CRT roots do not include all dynamic/virtual engine initialization. '
              'Counts are not engine completion percentages.')
    write_json(ROOT/'analysis/native-portability-summary.json',summary)
    print(json.dumps(summary,indent=2))


if __name__=='__main__':
    main()
