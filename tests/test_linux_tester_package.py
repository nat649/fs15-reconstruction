"""Archive extraction guards for the local Linux tester preparation tool."""
import hashlib
import importlib.util
import io
from pathlib import Path
import struct
import tarfile
import tempfile
import unittest

SPEC = importlib.util.spec_from_file_location('prepare_linux_tester', Path(__file__).resolve().parents[1]/'scripts/prepare_linux_tester.py')
PACKAGE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PACKAGE)


def fixture_directory():
    base = Path(__file__).resolve().parents[1]/'build/tester-package-fixtures'
    base.mkdir(parents=True, exist_ok=True)
    return tempfile.TemporaryDirectory(dir=base)


class LinuxTesterPackage(unittest.TestCase):
    def make_archive(self, root, extra=None, machine=62):
        archive = root/'runtime.tar.gz'
        elf = bytearray(64)
        elf[:6] = b'\x7fELF\x02\x01'
        struct.pack_into('<H', elf, 18, machine)
        with tarfile.open(archive, 'w:gz') as tar:
            for binary in PACKAGE.BINARIES:
                item = tarfile.TarInfo('bin/'+binary)
                item.size = len(elf)
                tar.addfile(item, io.BytesIO(elf))
            if extra:
                tar.addfile(extra, io.BytesIO(b'x') if extra.isfile() else None)
        (root/'runtime.sha256').write_text(hashlib.sha256(archive.read_bytes()).hexdigest()+'  runtime.tar.gz\n')
        return archive

    def test_regular_elfs_extract_without_original_resources(self):
        with fixture_directory() as tmp:
            root = Path(tmp)
            PACKAGE.extract_runtime(self.make_archive(root), root/'result')
            self.assertEqual({p.name for p in (root/'result/bin').iterdir()}, PACKAGE.BINARIES)

    def test_traversal_is_rejected_before_writes(self):
        with fixture_directory() as tmp:
            root = Path(tmp)
            item = tarfile.TarInfo('../escape')
            item.size = 1
            with self.assertRaises(ValueError):
                PACKAGE.extract_runtime(self.make_archive(root, item), root/'result')
            self.assertFalse((root/'result').exists())
            self.assertFalse((root/'escape').exists())

    def test_links_are_rejected_before_writes(self):
        with fixture_directory() as tmp:
            root = Path(tmp)
            item = tarfile.TarInfo('share/link')
            item.type = tarfile.SYMTYPE
            item.linkname = '../../outside'
            with self.assertRaises(ValueError):
                PACKAGE.extract_runtime(self.make_archive(root, item), root/'result')
            self.assertFalse((root/'result').exists())

    def test_wrong_architecture_is_rejected(self):
        with fixture_directory() as tmp:
            root = Path(tmp)
            with self.assertRaisesRegex(ValueError, 'x86_64'):
                PACKAGE.extract_runtime(self.make_archive(root, machine=183), root/'result')
            self.assertFalse((root/'result').exists())

    def test_checksum_mismatch_is_rejected(self):
        with fixture_directory() as tmp:
            root = Path(tmp)
            archive = self.make_archive(root)
            archive.write_bytes(archive.read_bytes()+b'changed')
            with self.assertRaisesRegex(ValueError, 'SHA-256'):
                PACKAGE.extract_runtime(archive, root/'result')
            self.assertFalse((root/'result').exists())


if __name__ == '__main__':
    unittest.main()
