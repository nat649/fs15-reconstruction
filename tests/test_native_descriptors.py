"""Authored pseudocode-shaped fixtures; no recovered game code is embedded."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from index_native_exports import descriptors


class DescriptorRecovery(unittest.TestCase):
    def fixture(self, name_first=False, label=False, service='140031bc0'):
        fields = ['local_a0 = &PTR_FUN_1404b6300;', 'local_40 = "authoredFixtureApi";',
                  'local_38 = ' + ('&LAB_' if label else 'FUN_') + '140111111;']
        if name_first:
            fields[0], fields[1] = fields[1], fields[0]
        call = ('FUN_' + service + '(local_context,' + ('FUN_140222222,' if service == '140031bc0' else '') + '&local_a0);')
        return '\n'.join(fields + [call])

    def test_name_before_vtable_and_wrapper_are_recovered(self):
        rows = list(descriptors(self.fixture(name_first=True), '140333333'))
        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0]['name'], 'authoredFixtureApi')
        self.assertEqual(rows[0]['wrapper'], '0x140222222')

    def test_code_label_is_distinguished_from_function(self):
        row = next(descriptors(self.fixture(label=True), '140333333'))
        self.assertEqual(row['implementation_kind'], 'code_label')

    def test_existing_registration_service_remains_supported(self):
        row = next(descriptors(self.fixture(service='140024f50'), '140333333'))
        self.assertNotIn('wrapper', row)

    def test_unrelated_or_stale_slots_do_not_match(self):
        for code in (self.fixture().replace('local_40', 'local_48'),
                     self.fixture().replace('&PTR_FUN_1404b6300', '&PTR_FUN_140000000'),
                     self.fixture().replace('local_38 =', '\n' * 41 + 'local_38 =')):
            self.assertEqual(list(descriptors(code, '140333333')), [])

    def test_reused_descriptor_does_not_reuse_old_name(self):
        code = self.fixture(service='140024f50') + '\nlocal_a0 = &PTR_FUN_1404b6300;\nlocal_38 = FUN_140444444;\nFUN_140024f50(local_context,&local_a0);'
        self.assertEqual(len(list(descriptors(code, '140333333'))), 1)


if __name__ == '__main__':
    unittest.main()
