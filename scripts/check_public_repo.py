"""Check tracked files and reachable Git history before public distribution.

Reports paths and rule names, never matching credential values. This is a
repository-specific guard for known asset paths/formats and common credential
patterns; it does not prove legal provenance or find every possible secret.
"""
import json
from pathlib import Path, PurePosixPath
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
FORBIDDEN_DIRS = {'inputs', 'recovered', 'build', 'logs', '.venv', '.vs', '.codex'}
FORBIDDEN_SUFFIXES = {'.exe', '.dll', '.lib', '.pdb', '.dmp', '.bin', '.gar', '.dds',
                      '.i3d', '.shapes', '.meshes', '.grle', '.luc', '.zip', '.7z', '.rar', '.key', '.pem'}
RULES = {
    'github-credential': re.compile(rb'\b(?:gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{30,})\b'),
    'api-credential': re.compile(rb'\bsk-(?:proj-|svcacct-)?[A-Za-z0-9_-]{32,}\b'),
    'aws-access-key': re.compile(rb'\b(?:AKIA|ASIA)[A-Z0-9]{16}\b'),
    'private-key': re.compile(rb'-----BEGIN (?:RSA |EC |OPENSSH |DSA )?PRIVATE KEY-----[\r\n]+[A-Za-z0-9+/]{20}'),
    'credential-url': re.compile(rb'https?://[^\s/:@]+:[^\s/@]{8,}@', re.I),
    'workstation-user-path': re.compile(rb'\b[A-Z]:[\\/]+Users[\\/]+(?!Public\b|Default\b)[A-Za-z0-9_.-]+[\\/]', re.I),
}


def git(*args):
    return subprocess.check_output(['git', *args], cwd=ROOT)


def path_rule(name):
    path = PurePosixPath(name)
    pieces = [part.lower() for part in path.parts]
    if any(part in FORBIDDEN_DIRS for part in pieces):
        return 'local-only-directory'
    if path.suffix.lower() in FORBIDDEN_SUFFIXES:
        return 'binary-or-original-resource-format'
    if path.name == '.env' or (path.name.startswith('.env.') and path.name != '.env.example'):
        return 'environment-configuration'
    if len(pieces) > 1 and pieces[0] == 'tools' and len(pieces) > 2:
        return 'downloaded-tool-checkout'
    if path.suffix.lower() == '.png' and path.parent != PurePosixPath('docs/screenshots'):
        return 'image-outside-render-gallery'
    return None


def content_rules(data):
    if data.startswith(b'\x89PNG\r\n\x1a\n'):
        return []  # Rendered captures are the documented binary exception.
    if data.startswith((b'MZ', b'\x7fELF', b'PK\x03\x04', b'version https://git-lfs.github.com/spec/')):
        return ['binary-or-lfs-payload']
    return [name for name, pattern in RULES.items() if pattern.search(data)]


def main():
    if git('rev-parse', '--is-shallow-repository').strip() == b'true':
        raise ValueError('Full history is required: use an unshallow fetch or checkout fetch-depth: 0')
    commits = git('rev-list', '--all').decode().splitlines()
    objects = {}
    findings = set()
    for commit in commits:
        for rule in content_rules(git('cat-file', 'commit', commit)):
            findings.add(('commit-metadata', commit, rule))
        for item in git('ls-tree', '-r', '-z', '--full-tree', commit).split(b'\0'):
            if not item:
                continue
            info, raw_name = item.split(b'\t', 1)
            mode, kind, oid = info.decode().split()
            name = raw_name.decode('utf-8')
            if kind != 'blob' or mode == '120000':
                findings.add(('history', name, 'submodule-or-symlink-needs-review'))
                continue
            objects.setdefault(oid, set()).add(name)
            if rule := path_rule(name):
                findings.add(('history', name, rule))
    process = subprocess.Popen(['git', 'cat-file', '--batch'], cwd=ROOT,
                               stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    try:
        for oid, names in objects.items():
            process.stdin.write((oid + '\n').encode()); process.stdin.flush()
            header = process.stdout.readline().split()
            if len(header) != 3 or header[0].decode() != oid or header[1] != b'blob':
                raise ValueError('Unexpected Git object response')
            size = int(header[2]); data = process.stdout.read(size)
            if len(data) != size or process.stdout.read(1) != b'\n':
                raise ValueError('Truncated Git object response')
            for rule in content_rules(data):
                for name in names:
                    findings.add(('history', name, rule))
    finally:
        process.stdin.close(); process.stdout.close()
        if process.wait() != 0:
            raise ValueError('Git object reader failed')
    tracked = [name.decode('utf-8') for name in git('ls-files', '-z').split(b'\0') if name]
    for name in tracked:
        if rule := path_rule(name):
            findings.add(('working-tree', name, rule))
        path = ROOT / name
        if path.is_symlink():
            findings.add(('working-tree', name, 'symlink-needs-review'))
        elif path.is_file():
            for rule in content_rules(path.read_bytes()):
                findings.add(('working-tree', name, rule))
        else:
            findings.add(('working-tree', name, 'tracked-file-missing'))
    summary = dict(status='failed' if findings else 'passed', commits=len(commits), historical_blobs=len(objects),
                   tracked_files=len(tracked), findings=[dict(scope=scope, path=name, rule=rule) for scope, name, rule in sorted(findings)],
                   scope='Known local-only paths, resource/binary formats, common credential patterns and workstation user paths; values are never printed')
    print(json.dumps(summary, indent=2))
    return bool(findings)


if __name__ == '__main__':
    sys.exit(main())
