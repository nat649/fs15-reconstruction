"""Publication regressions use authored files and synthetic credentials only."""
from contextlib import redirect_stdout
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
import check_public_repo as audit


class PublicationHistoryTests(unittest.TestCase):
    def setUp(self):
        self.parent = (Path(__file__).resolve().parents[1] / 'build/test-fixtures').resolve()
        self.parent.mkdir(parents=True, exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(prefix='public-audit-', dir=self.parent)
        self.root = Path(self.temporary.name).resolve()
        self.git('init', '-q')
        self.assertEqual(Path(self.git('rev-parse', '--show-toplevel').strip()).resolve(), self.root)

    def tearDown(self):
        if self.root.parent != self.parent:
            raise RuntimeError('Unsafe publication-fixture cleanup target')
        self.temporary.cleanup()

    def git(self, *args):
        result = subprocess.run(['git', '-c', 'user.name=AuditFixture', '-c', 'user.email=fixture@example.invalid',
                                 '-c', 'commit.gpgsign=false', '-c', 'core.hooksPath=' + str(self.root / 'no-hooks'), *args],
                                cwd=self.root, check=True, capture_output=True, text=True)
        return result.stdout

    def commit(self):
        self.git('add', '--all')
        self.git('commit', '-qm', 'Authored audit fixture')

    def check(self):
        output = io.StringIO()
        with patch.object(audit, 'ROOT', self.root), redirect_stdout(output):
            failed = audit.main()
        return failed, json.loads(output.getvalue()), output.getvalue()

    def test_clean_source_passes(self):
        (self.root / 'README.md').write_text('Authored synthetic repository.\n')
        self.commit()
        failed, result, _ = self.check()
        self.assertFalse(failed)
        self.assertEqual(result['commits'], 1)
        self.assertEqual(result['findings'], [])

    def test_removed_synthetic_credential_still_blocks_history(self):
        synthetic = 'ghp_' + 'A' * 36
        path = self.root / 'example.txt'
        path.write_text(synthetic)
        self.commit()
        path.write_text('Removed fixture credential.\n')
        self.commit()
        failed, result, printed = self.check()
        self.assertTrue(failed)
        self.assertTrue(any(row['scope'] == 'history' and row['rule'] == 'github-credential' for row in result['findings']))
        self.assertNotIn(synthetic, printed)

    def test_renaming_an_excluded_history_path_does_not_hide_it(self):
        folder = self.root / 'inputs'
        folder.mkdir()
        (folder / 'authored.txt').write_text('Synthetic reference content.\n')
        self.commit()
        self.git('mv', 'inputs/authored.txt', 'README.md')
        self.commit()
        failed, result, _ = self.check()
        self.assertTrue(failed)
        self.assertTrue(any(row['path'] == 'inputs/authored.txt' and row['scope'] == 'history' for row in result['findings']))


if __name__ == '__main__':
    unittest.main()
