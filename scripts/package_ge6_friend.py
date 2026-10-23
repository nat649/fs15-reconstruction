"""Prepare a PRIVATE GE6 Linux tester archive from already prepared local inputs.

Only this authored packager is public. Its output contains original resources
and must stay in ignored build storage or a user-authorized private transfer.
"""
import argparse
from datetime import datetime, timezone
import gzip
import hashlib
import json
from pathlib import Path, PurePosixPath
import shutil
import tarfile

ROOT = Path(__file__).resolve().parents[1]
LOCAL_IMAGE = 'e8a9f3b4397fb9aca31fdcb7471728f4f6ef7940a85911f1f727dbe09377a955'
TOP_FILES = ('x64/FarmingSimulator2015Game.exe', 'game.xml', 'FarmingSimulator2015.xml',
             'VERSION', 'steamdata.dat', 'dataS.gar', 'dataS2.gar', 'crc_files.xml')


def sha(path):
    value = hashlib.sha256()
    with path.open('rb') as stream:
        for data in iter(lambda: stream.read(1024 * 1024), b''):
            value.update(data)
    return value.hexdigest()


def resource_rows(root, expected=None):
    paths = set(TOP_FILES)
    for folder in ('data', 'shared', 'profileTemplate'):
        if not (root / folder).is_dir():
            raise ValueError('Missing prepared resource directory: ' + folder)
        paths.update(path.relative_to(root).as_posix() for path in (root / folder).rglob('*') if path.is_file())
    if expected is not None and paths != set(expected):
        raise ValueError('Prepared resource inventory differs from the verified input manifest')
    if sha(root / TOP_FILES[0]) != LOCAL_IMAGE:
        raise ValueError('Expected the already prepared, pinned Local GE6 image')
    rows = {}
    for name in sorted(paths):
        path = root / name
        if path.is_symlink() or not path.resolve().is_relative_to(root.resolve()) or not path.is_file():
            raise ValueError('Unsafe or missing resource: ' + name)
        row = expected[name] if expected is not None else dict(size=path.stat().st_size, sha256=sha(path))
        if path.stat().st_size != row['size']:
            raise ValueError('Resource size differs: ' + name)
        rows['resources/' + name] = row
    return rows


class CheckedReader:
    def __init__(self, stream):
        self.stream, self.digest = stream, hashlib.sha256()

    def read(self, size=-1):
        data = self.stream.read(size)
        self.digest.update(data)
        return data


def create(runtime, resources, output, expected=None):
    runtime, resources, output = runtime.resolve(), resources.resolve(), output.resolve()
    if not output.is_relative_to((ROOT / 'build').resolve()) or not output.name.endswith('.tar.gz'):
        raise ValueError('Keep this private .tar.gz output inside ignored build storage')
    stage = output.with_suffix('').with_suffix('')
    if output.exists() or stage.exists():
        raise ValueError('Archive or staging directory already exists; choose a new output name')
    build = json.loads((runtime / 'BUILD.json').read_text(encoding='utf-8'))
    abi = json.loads((runtime / 'ABI.json').read_text(encoding='utf-8'))
    binary = next(row for row in abi['binaries'] if row['file'] == 'FS15GE6LinuxBoot')
    if abi['status'] != 'passed' or sha(runtime / 'FS15GE6LinuxBoot') != binary['sha256']:
        raise ValueError('Portable runtime differs from its ABI report')
    rows = resource_rows(resources, expected)
    stage.mkdir(parents=True)
    for name in ('start-ge6.sh', 'start-ge6.py', 'README-FIRST.txt'):
        # The launcher shipped beside the compiled artifact is source-pinned.
        shutil.copy2(runtime / 'launcher' / name, stage / name)
    shutil.copy2(ROOT / 'NOTICE.md', stage / 'NOTICE.md')
    for name in ('BUILD.json', 'ABI.json'):
        shutil.copy2(runtime / name, stage / name)
    (stage / 'bin').mkdir()
    for name in ('FS15GE6LinuxBoot', 'FS15GE6X11Snapshot'):
        shutil.copy2(runtime / name, stage / 'bin' / name)
    for name in ('ge6-audio-relink', 'licenses'):
        if any(path.is_symlink() for path in (runtime / name).rglob('*')):
            raise ValueError('Linked dependency materials')
        shutil.copytree(runtime / name, stage / name)
    from xml.etree import ElementTree as ET
    settings = ET.fromstring((resources / 'game.xml').read_bytes())
    for key, value in {'width': '1280', 'height': '720', 'fullscreen': 'false', 'filter/msaa': '0'}.items():
        node = settings.find('graphic/display/' + key)
        if node is None:
            raise ValueError('Unexpected original graphics-settings layout')
        node.text = value
    (stage / 'profile-template').mkdir()
    ET.ElementTree(settings).write(stage / 'profile-template/game.xml', encoding='utf-8', xml_declaration=True)
    for path in sorted(stage.rglob('*')):
        if path.is_file():
            rows[path.relative_to(stage).as_posix()] = dict(size=path.stat().st_size, sha256=sha(path))
    manifest = dict(scope='Private original GE6 Linux test; original resources included locally',
                    created_utc=datetime.now(timezone.utc).isoformat(), build=build,
                    playable_game_verified=False, files=rows)
    manifest_file = stage / 'PACKAGE-MANIFEST.json'
    manifest_file.write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    entries = dict(rows)
    entries['PACKAGE-MANIFEST.json'] = dict(size=manifest_file.stat().st_size, sha256=sha(manifest_file))
    temporary = output.with_suffix('.gz.partial')
    try:
        with temporary.open('xb') as raw, gzip.GzipFile(filename='', mode='wb', compresslevel=1, fileobj=raw, mtime=0) as compressed, tarfile.open(fileobj=compressed, mode='w|') as archive:
            for index, (name, row) in enumerate(entries.items(), 1):
                source = resources / name[len('resources/'):] if name.startswith('resources/') else stage / name
                member = tarfile.TarInfo(stage.name + '/' + name)
                member.size = row['size']
                member.mode = 0o755 if name.startswith('bin/') or name.endswith('.sh') else 0o644
                with source.open('rb') as stream:
                    checked = CheckedReader(stream)
                    archive.addfile(member, checked)
                    if checked.digest.hexdigest() != row['sha256']:
                        raise ValueError('File changed or failed validation: ' + name)
                if index % 400 == 0:
                    print('Packed {}/{} files'.format(index, len(entries)), flush=True)
        temporary.rename(output)
    except BaseException:
        temporary.unlink(missing_ok=True)
        raise
    output.with_suffix('.gz.sha256').write_text(sha(output) + '  ' + output.name + '\n', encoding='utf-8')
    print('PRIVATE_ARCHIVE_READY: ' + str(output), flush=True)
    print('Compressed bytes: ' + str(output.stat().st_size), flush=True)
    return output


def verify_archive(path):
    """Independently decompress and verify every archived member without extraction."""
    digests, sizes, manifest, top = {}, {}, None, None
    with gzip.open(path, 'rb') as compressed:
        with tarfile.open(fileobj=compressed, mode='r|') as archive:
            for member in archive:
                name = PurePosixPath(member.name)
                if not member.isfile() or name.is_absolute() or '..' in name.parts or len(name.parts) < 2:
                    raise ValueError('Unexpected archive member')
                top = top or name.parts[0]
                if name.parts[0] != top:
                    raise ValueError('Multiple archive roots')
                relative = PurePosixPath(*name.parts[1:]).as_posix()
                if relative in digests:
                    raise ValueError('Duplicate archive member')
                with archive.extractfile(member) as stream:
                    if relative == 'PACKAGE-MANIFEST.json':
                        data = stream.read()
                        manifest = json.loads(data)
                        digests[relative] = hashlib.sha256(data).hexdigest()
                    else:
                        digest = hashlib.sha256()
                        for data in iter(lambda: stream.read(1024 * 1024), b''):
                            digest.update(data)
                        digests[relative] = digest.hexdigest()
                sizes[relative] = member.size
                if (relative.startswith('bin/') or relative.endswith('.sh')) and member.mode & 0o111 != 0o111:
                    raise ValueError('Lost executable permission')
        while compressed.read(1024 * 1024):
            pass
    if manifest is None or set(digests) - {'PACKAGE-MANIFEST.json'} != set(manifest['files']):
        raise ValueError('Archive inventory differs from manifest')
    for name, row in manifest['files'].items():
        if digests[name] != row['sha256'] or sizes[name] != row['size']:
            raise ValueError('Archived file hash or size differs: ' + name)
    result = dict(status='passed', files=len(digests), compressed_bytes=path.stat().st_size,
                  all_member_hashes_verified=True, gzip_integrity_verified=True)
    print(json.dumps(result, indent=2), flush=True)
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path)
    parser.add_argument('--resources', type=Path)
    parser.add_argument('--resource-manifest', type=Path)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--verify', type=Path, help='Verify an existing private archive only')
    args = parser.parse_args()
    if args.verify:
        verify_archive(args.verify)
    else:
        if not all((args.runtime, args.resources, args.output)):
            parser.error('--runtime, --resources and --output are required')
        expected = json.loads(args.resource_manifest.read_text(encoding='utf-8')) if args.resource_manifest else None
        result = create(args.runtime, args.resources, args.output, expected)
        verify_archive(result)
