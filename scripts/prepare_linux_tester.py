"""Build a local-only Linux tester package from a verified menu resource trace."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path, PurePosixPath
import shutil
import struct
import tarfile

REPO = Path(__file__).resolve().parents[1]
BINARIES = {'FS15PortableHost', 'FS15ScriptHost', 'FS15MenuLinux'}


def relative(name):
    path = PurePosixPath(name)
    if path.is_absolute() or not path.parts or any(p in ('.', '..') or ':' in p or '\\' in p for p in path.parts):
        raise ValueError('Invalid relative path: '+name)
    return path


def extract_runtime(archive, target):
    checksum = archive.with_name(archive.name.removesuffix('.tar.gz')+'.sha256')
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    if not checksum.is_file() or checksum.read_text().split()[0].lower() != digest:
        raise ValueError('Missing or mismatched runtime archive SHA-256 sidecar')
    entries, total = [], 0
    with tarfile.open(archive, 'r:gz') as tar:
        for item in tar.getmembers():
            if item.isdir():
                continue
            name = item.name.removeprefix('./')
            path = relative(name)
            if not item.isfile() or item.size > 100*1024*1024:
                raise ValueError('Unexpected runtime archive entry: '+name)
            if not (path.parts[0] == 'share' or name in ('start-linux.sh', 'README-LINUX.txt') or
                    len(path.parts) == 2 and path.parts[0] == 'bin' and path.name in BINARIES):
                raise ValueError('Unexpected runtime archive location: '+name)
            if name in [entry[0] for entry in entries]:
                raise ValueError('Duplicate archive entry: '+name)
            total += item.size
            if total > 200*1024*1024:
                raise ValueError('Runtime archive is too large')
            entries.append((name, tar.extractfile(item).read()))
    for binary in BINARIES:
        matches = [data for name, data in entries if name == 'bin/'+binary]
        if len(matches) != 1 or len(matches[0]) < 64 or matches[0][:6] != b'\x7fELF\x02\x01' or struct.unpack_from('<H', matches[0], 18)[0] != 62:
            raise ValueError('Missing Linux x86_64 executable: '+binary)
    for name, data in entries:
        path = target.joinpath(*PurePosixPath(name).parts)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    return digest


def prepare(runtime, trace, destination, resource_root=REPO):
    destination = destination.resolve()
    # Keep all local packages under the checkout's ignored build directory.
    destination.relative_to((REPO/'build').resolve())
    if destination.exists() or destination.with_suffix('.tar.gz').exists():
        raise ValueError('Destination/archive already exists; choose a new folder')
    lines = trace.read_text(encoding='utf-8-sig').splitlines()
    for marker in ('BOOT_OK ', 'INPUT_CHECK_OK ', 'SMOKE_OK '):
        if not any(line.startswith(marker) for line in lines):
            raise ValueError('Trace is missing a successful '+marker.strip())
    if any(line.startswith('FATAL ') for line in lines):
        raise ValueError('Trace contains a failure')
    sources = sorted({line[7:] for line in lines if line.startswith('SOURCE ')})
    assets = sorted({line[6:] for line in lines if line.startswith('ASSET ') and
                     (line[6:].startswith(('dataS/', 'dataS2/', 'shared/font/')) or
                      line[6:] in ('data/maps/map01/map_preview.dds', 'data/maps/map02/map_preview.dds'))})
    if not sources or not assets:
        raise ValueError('Trace has no menu sources/assets')
    recovery = json.loads((REPO/'analysis/lua-recovery.json').read_text(encoding='utf-8-sig'))
    records = {row['path']: row for row in recovery['files']}
    copied = []
    for name in sources:
        rel = relative(name)
        if not name.startswith('dataS/') or rel.suffix != '.lua':
            raise ValueError('Unexpected source path: '+name)
        path = resource_root/'recovered/lua'/str(rel)
        key = str(PurePosixPath(name[len('dataS/'):]).with_suffix('.luc'))
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        if key not in records or records[key]['source_sha256'] != digest:
            raise ValueError('Recovered source does not match the recovery manifest: '+name)
        copied.append((path, 'resources/lua/'+name))
    for name in assets:
        path = resource_root/'recovered/gar'/str(relative(name))
        if not path.is_file() or path.is_symlink():
            raise ValueError('Missing regular local resource: '+name)
        copied.append((path, 'resources/assets/'+name))
    # Validate everything before creating the package. No local executable or
    # entire recovered tree is copied; only the bounded traced frontend set.
    destination.mkdir(parents=True)
    digest = extract_runtime(runtime, destination)
    for source, name in copied:
        target = destination.joinpath(*PurePosixPath(name).parts)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
    for name in ('bootstrap.lua', 'career.lua', 'newgame.lua'):
        shutil.copyfile(REPO/'src/menu'/name, destination/'resources'/name)
    shutil.copyfile(REPO/'scripts/start-linux.sh', destination/'start-linux.sh')
    script = destination/'share/fs15-reconstruction/tester/linux-tester.py'
    script.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(REPO/'scripts/linux-tester.py', script)
    shutil.copyfile(REPO/'docs/LINUX_TESTER.md', destination/'README-LINUX.txt')
    files = [{'path': p.relative_to(destination).as_posix(), 'bytes': p.stat().st_size,
              'sha256': hashlib.sha256(p.read_bytes()).hexdigest()}
             for p in sorted(destination.rglob('*')) if p.is_file()]
    manifest = {'created_utc': datetime.now(timezone.utc).isoformat(),
                'scope': 'local-only recovered frontend test', 'contains_original_resources': True,
                'public_redistribution': False, 'native_engine_started': False, 'playable': False,
                'original_resources_tested_on_linux': False, 'runtime_archive_sha256': digest,
                'recovery_manifest_sha256': hashlib.sha256((REPO/'analysis/lua-recovery.json').read_bytes()).hexdigest(),
                'trace_sha256': hashlib.sha256(trace.read_bytes()).hexdigest(),
                'source_count': len(sources), 'asset_count': len(assets), 'files': files}
    (destination/'TESTER-MANIFEST.json').write_text(json.dumps(manifest, indent=2)+'\n', encoding='utf-8')
    archive = destination.with_suffix('.tar.gz')
    with tarfile.open(archive, 'w:gz') as tar:
        for path in sorted(destination.rglob('*')):
            if path.is_file():
                info = tar.gettarinfo(str(path), arcname=destination.name+'/'+path.relative_to(destination).as_posix())
                info.mode = 0o755 if path.parent.name == 'bin' or path.name == 'start-linux.sh' else 0o644
                info.uid = info.gid = 0
                info.uname = info.gname = ''
                with path.open('rb') as stream:
                    tar.addfile(info, stream)
    archive_digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    archive.with_name(archive.name.removesuffix('.tar.gz')+'.sha256').write_text(archive_digest+'  '+archive.name+'\n')
    print(str(archive))
    print(f'{len(sources)} recovered sources; {len(assets)} original menu resources; private local package only')
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--trace', type=Path, required=True)
    parser.add_argument('--destination', type=Path, required=True)
    args = parser.parse_args()
    prepare(args.runtime.resolve(), args.trace.resolve(), args.destination)


if __name__ == '__main__':
    main()
