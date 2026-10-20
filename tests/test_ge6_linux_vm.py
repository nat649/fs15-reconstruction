"""Authored payloads validate private VM packaging; no original code executes."""
from pathlib import Path
import hashlib
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from check_ge6_linux_vm import archive_entries, build_initramfs


class VmPackagingTests(unittest.TestCase):
    def test_integrity_inputs_are_preserved_with_unique_parent_directories(self):
        fixtures = Path(__file__).resolve().parents[1] / 'build/test-fixtures/ge6-vm'
        fixtures.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=fixtures) as directory:
            root = Path(directory)
            alpine, executable, image = (root / name for name in ('alpine', 'elf', 'image'))
            for item in (alpine, executable, image):
                item.write_bytes(b'authored fixture')
            resources = root / 'resources'
            names = ('game.xml', 'FarmingSimulator2015.xml', 'VERSION', 'steamdata.dat', 'crc_files.xml',
                     'data/maps/map01.i3d', 'data/maps/map01/map01_dem.png')
            for name in names:
                path = resources / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(('unchanged authored input: ' + name).encode())
            alpine_entries = [('usr/bin/busybox', b'authored busybox'), ('usr/lib/ld-musl-x86_64.so.1', b'authored libc')]
            with patch('check_ge6_linux_vm.image_kind', return_value='fixture'), patch('check_ge6_linux_vm.archive_entries', return_value=alpine_entries):
                payload = build_initramfs(alpine, executable, image, resources, integrity=True)
            entries = list(archive_entries(payload))
            self.assertEqual(len(entries), len(dict(entries)))
            unpacked = dict(entries)
            for name in names:
                self.assertEqual(unpacked['game/' + name], (resources / name).read_bytes())
                digest = hashlib.sha256((resources / name).read_bytes()).hexdigest()
                self.assertIn(f'path={name} sha256={digest}'.encode(), unpacked['init'])
            self.assertIn('game/data/maps/map01', unpacked)
            self.assertNotIn('game/dataS.gar', unpacked)
            self.assertIn(b'-profile Z:/profile', unpacked['init'])


if __name__ == '__main__':
    unittest.main()
