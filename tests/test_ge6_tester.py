"""Portable launcher preflight checks. No original GE6 payload is used."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location('ge6_tester', Path(__file__).resolve().parents[1] / 'packaging/ge6-linux/start-ge6.py')
tester = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(tester)


class GE6TesterChecks(unittest.TestCase):
    def test_baseline_and_modern_arch_debian_versions(self):
        for version in ('2.31', '2.36', '2.39', '2.41', '2.44'):
            tester.check_platform('Linux', 'x86_64', 'glibc', version)
        for args in [('Linux', 'aarch64', 'glibc', '2.41'), ('Linux', 'x86_64', 'musl', '1.2.5'),
                     ('Linux', 'x86_64', 'glibc', '2.9'), ('Linux', 'x86_64', 'glibc', '2.30'),
                     ('Windows', 'AMD64', 'glibc', '2.41')]:
            with self.assertRaises(RuntimeError):
                tester.check_platform(*args)

    def test_distro_family_uses_correct_package_manager(self):
        self.assertIn('pacman -Syu', tester.install_hint({'ID': 'arch'}))
        self.assertIn('pacman -Syu', tester.install_hint({'ID': 'endeavouros', 'ID_LIKE': 'arch'}))
        for distro in ({'ID': 'debian'}, {'ID': 'ubuntu'}, {'ID': 'linuxmint', 'ID_LIKE': 'ubuntu debian'}):
            self.assertIn('apt-get install', tester.install_hint(distro))
        self.assertNotIn('sudo', tester.install_hint({'ID': 'unknown'}))

    def test_reads_os_release_without_executing_its_contents(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'os-release'
            path.write_text('ID=endeavouros\nID_LIKE="arch"\nPRETTY_NAME="Example Linux"\n#COMMENT=x\n', encoding='utf-8')
            self.assertEqual(tester.distro_info(path), {'ID': 'endeavouros', 'ID_LIKE': 'arch', 'PRETTY_NAME': 'Example Linux'})

    def test_real_refresh_needed_without_inventing_a_display_mode(self):
        self.assertEqual(tester.display_rates('   1920x1080 59.94*+ 60.00'), [59.94])
        for text in ('1280x720 0.00*', 'Screen 0: no active output', '1920x1080 60.00'):
            with self.assertRaises(RuntimeError):
                tester.display_rates(text)

    def test_package_corruption_and_escaping_paths_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'fixture.txt').write_bytes(b'authored fixture')
            rows = {'fixture.txt': {'size': 16, 'sha256': hashlib.sha256(b'authored fixture').hexdigest()}}
            manifest = root / 'PACKAGE-MANIFEST.json'
            manifest.write_text(json.dumps({'files': rows}), encoding='utf-8')
            with patch.object(tester, 'ROOT', root):
                tester.verify(True)
                (root / 'fixture.txt').write_bytes(b'changed')
                with self.assertRaises(RuntimeError):
                    tester.verify(True)
                manifest.write_text(json.dumps({'files': {'../escaped': rows['fixture.txt']}}), encoding='utf-8')
                with self.assertRaises(RuntimeError):
                    tester.verify(True)


if __name__ == '__main__':
    unittest.main()
