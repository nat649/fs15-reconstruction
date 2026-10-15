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

CALLS = re.compile(r'\bFUN_([0-9a-f]+)\(')
ASSIGNMENT = re.compile(r'(local_[0-9a-f]+) = (.*);')
REGISTRATION = re.compile(r'FUN_(140024f50|140031bc0)\((.*),&(local_[0-9a-f]+)\);')


def descriptors(code, registration):
    """Match explicit descriptor slots, including reordered initializers.

    The two observed registration functions use the same descriptor layout:
    vtable at +0, name at +0x60, implementation at +0x68. Stack-local suffixes
    count down as addresses increase. Do not associate adjacent unrelated data.
    """
    assigned = {}
    consumed = {}
    for line_number, line in enumerate(code.splitlines()):
        match = ASSIGNMENT.fullmatch(line.strip())
        if match:
            assigned[match[1]] = (match[2], line_number)
        call = REGISTRATION.fullmatch(re.sub(r'\s+', '', line))
        if not call:
            continue
        base = call[3]
        offset = int(base[6:], 16)
        if offset < 0x68:
            continue
        slots = [base, 'local_' + format(offset - 0x60, 'x'), 'local_' + format(offset - 0x68, 'x')]
        values = [assigned.get(slot) for slot in slots]
        if any(value is None or line_number - value[1] > 40 or value[1] <= consumed.get(base, -1) for value in values):
            continue
        vtable, name, implementation = [value[0] for value in values]
        if vtable != '&PTR_FUN_1404b6300':
            continue
        name_match = re.fullmatch(r'"([^"\\]+)"', name)
        impl_match = re.fullmatch(r'(FUN_|&LAB_)([0-9a-f]+)', implementation)
        if name_match is None or impl_match is None:
            continue
        result = dict(name=name_match[1], implementation='0x' + impl_match[2],
                      implementation_kind='code_label' if impl_match[1] == '&LAB_' else 'function',
                      registration_function='0x' + registration,
                      registration_service='0x' + call[1],
                      evidence='explicit descriptor vtable, +0x60 name, +0x68 callable and observed registration call')
        if call[1] == '140031bc0':
            wrapper = re.search(r',FUN_([0-9a-f]+)$', call[2])
            if wrapper is None:
                continue
            result['wrapper'] = '0x' + wrapper[1]
        consumed[base] = line_number
        yield result


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
    catalogue_path = args.export / 'functions.csv'
    if not catalogue_path.exists():
        catalogue_path = ROOT / 'recovered/decompiled/game-runtime/functions.csv'
    with catalogue_path.open(newline='', encoding='utf-8') as stream:
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
        if binding['implementation_kind'] == 'code_label' and binding['implementation'][2:] in eligible:
            binding['pseudocode_reference_kind'] = 'code_label'
            binding['implementation_kind'] = 'function'
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
                   descriptor_function_pointers=sum(row['implementation_kind'] == 'function' for row in bindings),
                   descriptor_code_labels=sum(row['implementation_kind'] == 'code_label' for row in bindings),
                   descriptor_scope='Observed native scripting descriptors, including general utilities; overlap with earlier Lua/native mappings is possible.',
                   unresolved=failures,
                   scope='All eligible functions in the existing Ghidra catalogue were attempted. Pseudocode is not buildable recovered source or proof of complete function discovery, signatures, engine reconstruction or behavior parity.')
    write_json(ROOT / 'analysis/native-export-summary.json', summary)
    print(json.dumps(summary, indent=2))


if __name__ == '__main__':
    main()
