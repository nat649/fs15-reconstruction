"""Verify explicit scene vtable discovery against pinned data and exported code."""
import csv
import hashlib
import json
import struct
from inventory import ROOT,PE,sha256,write_json
from native_gar import DEFAULT_IMAGE,EXPECTED_IMAGE

TABLES={'1404c0cd8':'1400b15e0','1404c5d98':'14010fa90'}


def main():
    if sha256(DEFAULT_IMAGE)!=EXPECTED_IMAGE:raise ValueError('Unsupported native image')
    image=DEFAULT_IMAGE.read_bytes();pe=PE(image)
    discovery=ROOT/'analysis/vtable-discovery'
    expected_slots,expected_created=map(int,(discovery/'complete.txt').read_text().strip().split(','))
    with (discovery/'slots.csv').open(newline='') as stream:rows=list(csv.DictReader(stream))
    with (ROOT/'recovered/decompiled/game-full/decompiled-index.csv').open(newline='') as stream:
        catalogue={row['entry']:row for row in csv.DictReader(stream)}
    expected={(table,str(slot)) for table in TABLES for slot in range(0,64,8)}
    if len(rows)!=expected_slots or len(rows)!=len(expected) or {(r['table'],r['slot']) for r in rows}!=expected:
        raise ValueError('Incomplete or duplicate bounded vtable slots')
    for row in rows:
        pointer=struct.unpack_from('<Q',image,pe.offset(int(row['table'],16)-pe.base+int(row['slot'])))[0]
        if pointer!=int(row['target'],16) or row['containing_function']!=row['target']:
            raise ValueError('Vtable pointer or catalogue entry differs')
        if row['target'] not in catalogue or catalogue[row['target']]['status']!='ok':
            raise ValueError('Vtable target has no successful pseudocode export')
    for table,constructor in TABLES.items():
        row=catalogue[constructor]
        if '&PTR_FUN_'+table not in (ROOT/'recovered/decompiled/game-full'/row['file']).read_text():
            raise ValueError('Missing observed constructor/vtable association')
    created=sorted({row['target'] for row in rows if row['status']=='created_from_explicit_vtable'})
    if len(created)!=expected_created:raise ValueError('Discovery completion marker differs')
    report=dict(status='passed',reference_sha256=EXPECTED_IMAGE,tables=len(TABLES),observed_slots=len(rows),
        distinct_targets=len({row['target'] for row in rows}),newly_catalogued_functions=len(created),
        new_entries=['0x'+entry for entry in created],
        discovery_csv_sha256=hashlib.sha256((discovery/'slots.csv').read_bytes()).hexdigest(),
        existing_function_bodies_split=False,
        scope='Two constructor-observed scene-node tables, eight explicit slots each. '
              'All targets have local pseudocode exports. This is not an exhaustive engine vtable, class or lifecycle recovery.')
    write_json(ROOT/'analysis/scene-vtables-summary.json',report)
    print(json.dumps(report,indent=2))


if __name__=='__main__':main()
