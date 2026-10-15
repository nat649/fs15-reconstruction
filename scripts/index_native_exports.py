"""Verify a full local Ghidra pass and index explicit engine API descriptors.

Recovered pseudocode and detailed API associations stay local. Only aggregate
counts, hashes and unresolved entry points are published by this script.
"""
import argparse
from collections import Counter
import csv
import hashlib
import json
from pathlib import Path
import re
from inventory import ROOT, sha256, write_json
from native_gar import EXPECTED_IMAGE, DEFAULT_IMAGE

DESCRIPTOR = re.compile(r'(local_\w+) = &PTR_FUN_1404b6300;')
NAME = re.compile(r'local_\w+ = "([^"\\]+)";')
IMPLEMENTATION = re.compile(r'local_\w+ = FUN_([0-9a-f]+);')
CALLS = re.compile(r'\bFUN_([0-9a-f]+)\(')


def descriptors(code, registration):
    """Only accept the observed descriptor vtable and registration call pair."""
    pending = None
    for line in code.splitlines():
        match = DESCRIPTOR.fullmatch(line.strip())
        if match:
            pending = dict(descriptor=match[1], age=0)
            continue
        if pending is None:
            continue
        pending['age'] += 1
        if pending['age'] > 40:
            pending = None
            continue
        match = NAME.fullmatch(line.strip())
        if match:
            pending['name'] = match[1]
        match = IMPLEMENTATION.fullmatch(line.strip())
        if match:
            pending['implementation'] = '0x' + match[1]
        if 'FUN_140024f50(' in line:
            if '&' + pending['descriptor'] + ')' in line and 'name' in pending and 'implementation' in pending:
                yield dict(name=pending['name'], implementation=pending['implementation'],
                           registration_function='0x' + registration,
                           evidence='explicit descriptor vtable/name/function and registration call')
            pending = None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--export', type=Path, default=ROOT / 'recovered/decompiled/game-full')
    args = parser.parse_args()
    progress = json.loads((args.export / 'progress.json').read_text())
    if not progress['complete_pass']:
        parser.error('Full export has not finished; leave its current progress intact')
    if sha256(DEFAULT_IMAGE) != EXPECTED_IMAGE:
        parser.error('Native analysis image does not match the pinned reference')
    with (args.export / 'decompiled-index.csv').open(newline='', encoding='utf-8') as stream:
        rows = list(csv.DictReader(stream))
    if len(rows) != progress['eligible'] or len({row['entry'] for row in rows}) != len(rows):
        raise ValueError('Incomplete/duplicate function catalogue')
    with (ROOT / 'recovered/decompiled/game-runtime/functions.csv').open(newline='', encoding='utf-8') as stream:
        catalogue = list(csv.DictReader(stream))
    eligible = {row['entry']: row for row in catalogue if row['thunk'] == 'false' and row['external'] == 'false'}
    if len(catalogue) != progress['catalogued'] or {row['entry'] for row in rows} != eligible.keys():
        raise ValueError('Full export differs from the original analyzed function catalogue')
    if any(row['size'] != eligible[row['entry']]['size'] for row in rows):
        raise ValueError('Function sizes differ from the analyzed catalogue')
    bindings = []
    digest = hashlib.sha256()
    byte_count = 0
    by_address = {}
    for row in rows:
        if row['status'] != 'ok':
            continue
        path = args.export / row['file']
        if path.parent != args.export or path.suffix != '.c':
            raise ValueError('Unexpected export path')
        data = path.read_bytes()
        code = data.decode('utf-8')
        if f' * Address: {row["entry"]}\n' not in code or ' * Export: full catalogue\n' not in code:
            raise ValueError(f'Export identity differs: {row["entry"]}')
        digest.update((row['entry'] + ':' + hashlib.sha256(data).hexdigest() + '\n').encode())
        byte_count += len(data)
        by_address['0x' + row['entry']] = sorted({'0x' + address for address in CALLS.findall(code)})
        bindings.extend(descriptors(code, row['entry']))
    counts = Counter(row['status'] for row in rows)
    if counts['ok'] != progress['exported'] or counts['failed'] != progress['failed']:
        raise ValueError('Export progress and verified files disagree')
    for binding in bindings:
        binding['direct_named_callees'] = by_address.get(binding['implementation'], [])
    if not any(row['name'] == 'createWheelShape' for row in bindings):
        raise ValueError('Expected engine wheel descriptors were not recovered')
    write_json(ROOT / 'analysis/engine-bindings.json', dict(scope='Observed explicit engine descriptors; signatures and full behavior still require validation', bindings=bindings))
    failures = [dict(entry='0x' + row['entry'], size=int(row['size']), diagnostic=row['diagnostic']) for row in rows if row['status'] != 'ok']
    summary = dict(reference_sha256=EXPECTED_IMAGE, catalogued=progress['catalogued'], eligible=progress['eligible'],
                   excluded_thunks_or_external=progress['catalogued'] - progress['eligible'],
                   exported=counts['ok'], failed=counts['failed'], complete_pass=True,
                   pseudocode_bytes=byte_count, export_set_sha256=digest.hexdigest(),
                   explicit_engine_descriptors=len(bindings), distinct_engine_names=len({row['name'] for row in bindings}),
                   descriptor_registration_functions=len({row['registration_function'] for row in bindings}),
                   unresolved=failures,
                   scope='All eligible functions in the existing Ghidra catalogue were attempted. Pseudocode is not buildable recovered source or proof of complete function discovery, signatures, engine reconstruction or behavior parity.')
    write_json(ROOT / 'analysis/native-export-summary.json', summary)
    print(json.dumps(summary, indent=2))


if __name__ == '__main__':
    main()
