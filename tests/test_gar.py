"""Format validation tests with synthetic records, not original game data."""
import struct
import sys
import tempfile
from pathlib import Path
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from gar import GarArchive, safe_relative_name, normalize_luajit


def fixture(names=("scripts/example.lua",), contents=(b"return 1",)):
    header = bytearray(512)
    header[0x60:0x64] = b"GAR "
    struct.pack_into("<III", header, 0x64, 0x50003, 0, len(names))
    index = bytearray()
    payload = bytearray()
    start = 512 + len(names) * 512
    for name, content in zip(names, contents):
        padding = 8 - len(content) % 8
        padded = content + bytes([padding]) * padding
        record = bytearray(512)
        struct.pack_into("<IIQQQQ", record, 0, 1, 0, len(padded), len(content), start + len(payload), 0)
        encoded_name = name.encode("utf-8")
        record[40:40 + len(encoded_name)] = encoded_name
        index.extend(record)
        payload.extend(padded)
    return header + index + payload


class GarTests(unittest.TestCase):
    def setUp(self):
        self.fixture_root = Path(__file__).resolve().parents[1] / "build/test-fixtures"
        self.fixture_root.mkdir(parents=True, exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(dir=self.fixture_root)
        self.path = Path(self.temporary.name) / "synthetic.gar"

    def tearDown(self):
        if not Path(self.temporary.name).resolve().is_relative_to(self.fixture_root.resolve()):
            raise RuntimeError("Temporary test directory escaped the workspace")
        self.temporary.cleanup()

    def archive(self, data, decrypt=lambda data: data):
        self.path.write_bytes(data)
        return GarArchive(self.path, decrypt)

    def test_valid_record_and_exact_payload_length(self):
        with self.archive(fixture()) as archive:
            self.assertEqual(archive.header_counts, (0, 1))
            self.assertEqual(archive.entries[0].name, "scripts/example.lua")
            self.assertEqual(archive.read(archive.entries[0]), b"return 1")

    def test_both_header_counts_are_added(self):
        data = fixture(("a", "b"), (b"A", b"B"))
        struct.pack_into("<II", data, 0x68, 1, 1)
        with self.archive(data) as archive:
            self.assertEqual([archive.read(entry) for entry in archive.entries], [b"A", b"B"])

    def test_unsafe_paths_rejected(self):
        for name in ("../escape", "a/../../escape", "/absolute", "C:/absolute", "a//b", "a/./b", "\\\\server\\file", "a\0b"):
            with self.subTest(name=name), self.assertRaises(ValueError):
                safe_relative_name(name)

    def test_archive_traversal_rejected_before_writing(self):
        with self.assertRaisesRegex(ValueError, "Unsafe archive path"):
            self.archive(fixture(("../escape",)))

    def test_windows_device_names_and_aliases_rejected(self):
        for name in ("NUL", "a/CON.lua", "LPT1.xml", "COM9", ".. /escape", "a./file", "file ", "file?.lua"):
            with self.subTest(name=name), self.assertRaises(ValueError):
                safe_relative_name(name)

    def test_case_collision_rejected(self):
        with self.assertRaisesRegex(ValueError, "Colliding archive path"):
            self.archive(fixture(("File.lua", "file.lua"), (b"a", b"b")))

    def test_bad_magic_rejected(self):
        data = fixture()
        data[0x60] = 0
        with self.assertRaisesRegex(ValueError, "supported GAR"):
            self.archive(data)

    def test_other_version_rejected(self):
        data = fixture()
        struct.pack_into("<I", data, 0x64, 0x50002)
        with self.assertRaisesRegex(ValueError, "version"):
            self.archive(data)

    def test_index_count_cannot_exceed_file(self):
        data = fixture()
        struct.pack_into("<I", data, 0x6c, 0xffffffff)
        with self.assertRaisesRegex(ValueError, "index extends"):
            self.archive(data)

    def test_short_decoded_index_rejected(self):
        with self.assertRaisesRegex(ValueError, "index length"):
            self.archive(fixture(), lambda data: data[:-1])

    def test_payload_outside_archive_rejected(self):
        data = fixture()
        struct.pack_into("<Q", data, 512 + 24, len(data) + 8)
        with self.assertRaisesRegex(ValueError, "outside the archive"):
            self.archive(data)

    def test_overlapping_payloads_rejected(self):
        data = fixture(("a", "b"), (b"A", b"B"))
        first_offset = struct.unpack_from("<Q", data, 512 + 24)[0]
        struct.pack_into("<Q", data, 1024 + 24, first_offset)
        with self.assertRaisesRegex(ValueError, "Overlapping payload"):
            self.archive(data)

    def test_impossible_plain_length_rejected(self):
        data = fixture()
        struct.pack_into("<Q", data, 512 + 16, 1000)
        with self.assertRaisesRegex(ValueError, "padding"):
            self.archive(data)

    def test_unknown_record_metadata_rejected(self):
        data = fixture()
        struct.pack_into("<I", data, 512 + 4, 1)
        with self.assertRaisesRegex(ValueError, "Unsupported GAR record"):
            self.archive(data)

    def test_unterminated_name_rejected(self):
        data = fixture()
        data[512 + 40:1024] = b"A" * (512 - 40)
        with self.assertRaisesRegex(ValueError, "Unterminated name"):
            self.archive(data)

    def test_script_version_two_is_unchanged(self):
        source = b"\x1bLJ\x02\x00\x01test"
        self.assertEqual(normalize_luajit(source, bytes(range(8))), (source, False))

    def test_script_version_three_known_transform(self):
        source = b"\x1bLJ\x03" + b"\x00" * 8
        expected = b"\x1bLJ\x02\x08\x0a\x0c\x0e\x08\x0a\x0c\x0e"
        self.assertEqual(normalize_luajit(source, bytes(range(8))), (expected, True))

    def test_text_script_is_unchanged(self):
        self.assertEqual(normalize_luajit(b"return 42", bytes(8)), (b"return 42", False))

    def test_invalid_script_key_rejected(self):
        with self.assertRaises(ValueError):
            normalize_luajit(b"\x1bLJ\x03", b"short")


if __name__ == "__main__":
    unittest.main()
