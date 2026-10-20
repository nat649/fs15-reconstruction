"""Authored logs test provenance parsing, not original engine execution."""
import hashlib
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from ge6_linux_images import LOCAL_IMAGE
from verify_ge6_linux_boot import verify


class BootEvidenceTests(unittest.TestCase):
    def test_local_input_is_explicitly_modified(self):
        result = self.parse('local-platform')
        self.assertEqual(result['original_image_sha256'], LOCAL_IMAGE)
        self.assertTrue(result['native_instruction_patches'])
        self.assertFalse(result['linux_loader_patches_instructions'])
        self.assertFalse(result['menu_verified'])
        self.assertEqual(result['stopped_at']['symbol'], 'ordinal:115')
        self.assertIn('src/ge6_linux/network_services.h', result['committed_source_sha256'])

    def test_local_input_cannot_be_reported_as_reference(self):
        with self.assertRaisesRegex(ValueError, 'provenance'):
            self.parse('reference')

    def test_faulted_run_is_rejected(self):
        with self.assertRaisesRegex(ValueError, 'faulted'):
            self.parse('local-platform', 'GE6_NATIVE_FAULT signal=11\n')

    def test_startup_journal_does_not_establish_complete_engine_boot(self):
        result = self.parse('local-platform', 'GE6_ORIGINAL_LOG_BEGIN\nGIANTS Engine Runtime 6.0.2 64bit (authored fixture)\nGE6_ORIGINAL_LOG_END\n')
        self.assertTrue(result['original_core_startup_log_observed'])
        self.assertIsNotNone(result['captured_original_log_sha256'])
        self.assertFalse(result['full_engine_initialization_verified'])
        self.assertFalse(result['menu_verified'])

    def test_worker_entry_without_native_os_call_is_not_execution_evidence(self):
        result = self.parse('local-platform', 'GE6_NATIVE_THREAD_ENTER entry=0x140043620 tid=12 code=0\n')
        self.assertEqual(result['original_workers_with_os_calls'], [])

    def test_worker_execution_requires_matching_image_entry(self):
        result = self.parse('local-platform', 'GE6_NATIVE_THREAD_ENTER entry=0x140043620 tid=12 code=0\nGE6_NATIVE_THREAD_FIRST_OS name=SetEvent entry=0x140043620 tid=12\n')
        self.assertEqual(result['original_workers_with_os_calls'], [dict(entry='0x140043620', linux_tid=12, first_os_call='SetEvent')])
        self.assertFalse(result['original_worker_shutdown_verified'])
        with self.assertRaisesRegex(ValueError, 'Worker OS-call'):
            self.parse('local-platform', 'GE6_NATIVE_THREAD_FIRST_OS name=SetEvent entry=0x140043620 tid=13\n')

    def parse(self, kind, extra=''):
        fixtures = Path(__file__).resolve().parents[1] / 'build/test-fixtures/ge6-boot-evidence'
        fixtures.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=fixtures) as directory:
            log = Path(directory) / 'authored.log'
            executable = Path(directory) / 'authored.elf'
            executable.write_bytes(b'authored test payload')
            log.write_text('GE6_LINUX_VM_STARTED\nLinux authored-fixture\n'
                           'GE6_VM_IMAGE_KIND=local-platform\nGE6_IMAGE_KIND=local-platform\n'
                           'GE6_IMAGE_MAPPED base=0x140000000 entry=0x1403fc96c supported_imports=83 blocked_imports=341\n'
                           'GE6_NATIVE_NT_TIB_READY\nGE6_ORIGINAL_CRT_ENTER\n'
                           'GE6_ORIGINAL_MAIN_REACHED caller=0x14000c9f5\n'
                           'GE6_BLOCKED_IMPORT WS2_32.dll!ordinal:115 caller=0x1400011cc os_calls=160\n'
                           'GE6_LINUX_VM_RESULT=78\n' + extra)
            def git_output(args, **kwargs):
                if args[1] == 'ls-tree':
                    return b'src/ge6_linux/network_services.h\n'
                return 'a' * 40 if args[1] == 'rev-parse' else b'authored source fixture'
            with patch('verify_ge6_linux_boot.subprocess.check_output', side_effect=git_output):
                result = verify(log, executable, 'HEAD', 1, kind)
            self.assertEqual(result['private_serial_log_sha256'], hashlib.sha256(log.read_bytes()).hexdigest())
            return result


if __name__ == '__main__':
    unittest.main()
