"""Summarize a private original-code Linux VM run without publishing its payload."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
from inventory import ROOT
from native_gar import EXPECTED_IMAGE
from ge6_linux_images import LOCAL_IMAGE


def verify(log, executable, source_commit, run_id, image_kind='reference'):
    raw = log.read_bytes()
    text = raw.decode(errors='replace')
    for marker in ('GE6_LINUX_VM_STARTED', 'GE6_NATIVE_NT_TIB_READY', 'GE6_ORIGINAL_CRT_ENTER',
                   'GE6_ORIGINAL_MAIN_REACHED', 'GE6_LINUX_VM_RESULT=78'):
        if marker not in text:
            raise ValueError('Missing native Linux evidence marker: ' + marker)
    mapped = re.search(r'GE6_IMAGE_MAPPED base=(0x[0-9a-f]+) entry=(0x[0-9a-f]+) supported_imports=(\d+) blocked_imports=(\d+)', text)
    blocked = re.search(r'GE6_BLOCKED_IMPORT ([^!\s]+)!([^\s]+) caller=(0x[0-9a-f]+) os_calls=(\d+)', text)
    main = re.search(r'GE6_ORIGINAL_MAIN_REACHED caller=(0x[0-9a-f]+)', text)
    kernel = re.search(r'^Linux .+$', text, re.MULTILINE)
    if not all((mapped, blocked, main, kernel)) or 'GE6_NATIVE_FAULT' in text:
        raise ValueError('Incomplete or faulted native Linux run')
    if mapped.groups()[:2] != ('0x140000000', '0x1403fc96c') or not 0x14000c9a0 <= int(main[1], 16) < 0x14000c9a0 + 4364:
        raise ValueError('Unexpected native entry addresses')
    if image_kind not in ('reference', 'local-platform'):
        raise ValueError('Unknown private image kind')
    if image_kind == 'local-platform' and ('GE6_IMAGE_KIND=local-platform' not in text or 'GE6_VM_IMAGE_KIND=local-platform' not in text):
        raise ValueError('Missing local-platform image provenance')
    commit = subprocess.check_output(['git', 'rev-parse', source_commit + '^{commit}'], cwd=ROOT, text=True).strip()
    sources = ('src/ge6_linux/main.cpp', 'src/ge6_linux/ansi_reference.h', 'src/ge6_linux/CMakeLists.txt',
               'tests/check_ge6_linux_boot.py', 'scripts/check_ge6_linux_vm.py')
    if image_kind == 'local-platform':
        sources += ('src/ge6_linux/local_platform.h', 'src/ge6_linux/file_services.h',
                    'scripts/ge6_linux_images.py', 'scripts/prepare_local_engine.py')
    pins = {name: hashlib.sha256(subprocess.check_output(['git', 'show', commit + ':' + name], cwd=ROOT)).hexdigest()
            for name in sources}
    return dict(scope='Original GE6 instructions executed in a local x64 Linux VM through authored OS/ABI transport; not a complete engine port',
                source_commit=commit, committed_source_sha256=pins,
                ci_run=f'https://github.com/nat649/fs15-reconstruction/actions/runs/{int(run_id)}',
                image_kind=image_kind, original_image_sha256=LOCAL_IMAGE if image_kind == 'local-platform' else EXPECTED_IMAGE,
                linux_elf_sha256=hashlib.sha256(executable.read_bytes()).hexdigest(),
                private_serial_log_sha256=hashlib.sha256(raw).hexdigest(), kernel=kernel[0].strip(),
                original_crt_entered=True, original_application_entry_reached=True, original_main_caller=main[1],
                native_instruction_patches=image_kind == 'local-platform',
                existing_local_platform_bootstrap_modified=image_kind == 'local-platform',
                linux_loader_patches_instructions=False, import_addresses_rebound=True, windows_os=False, wine_or_proton_used=False,
                imported_names=int(mapped[3]) + int(mapped[4]), bound_import_names=int(mapped[3]),
                stop_trampoline_names=int(mapped[4]),
                stopped_at=dict(library=blocked[1], symbol=blocked[2], caller=blocked[3], traced_os_calls=int(blocked[4])),
                full_engine_initialization_verified=False, original_renderer_initialized=False,
                menu_verified=False, playable_linux_game_verified=False, recompiled_engine=False,
                limits='CRT/main entry evidence only. Import totals include unexecuted optional paths, not an engine completion percentage. Original binary, private initramfs and serial log stay local.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--log', required=True, type=Path)
    parser.add_argument('--exe', required=True, type=Path)
    parser.add_argument('--source-commit', required=True)
    parser.add_argument('--ci-run', required=True)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--image-kind', choices=('reference', 'local-platform'), default='reference')
    args = parser.parse_args()
    result = verify(args.log, args.exe, args.source_commit, args.ci_run, args.image_kind)
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))
