"""Authored source-pinning fixtures; no original code or native process runs."""
import hashlib
from pathlib import Path
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from index_recompilation import source_pinned


class RecompilationEvidence(unittest.TestCase):
    def setUp(self):
        fixtures=Path(__file__).resolve().parents[1]/'build/test-fixtures/recompilation-evidence'
        fixtures.mkdir(parents=True,exist_ok=True)
        self.temp=tempfile.TemporaryDirectory(dir=fixtures)
        self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name)
        source=self.root/'src/core/fixture.cpp'
        source.parent.mkdir(parents=True)
        source.write_bytes(b'// authored fixture\n')
        self.source=source
        self.report=dict(status='passed',rebuilt_source_sha256={'src/core/fixture.cpp':hashlib.sha256(source.read_bytes()).hexdigest()})

    def test_matching_source_and_passed_report_are_required(self):
        self.assertTrue(source_pinned(self.root,self.report))
        self.report['status']='not_run'
        self.assertFalse(source_pinned(self.root,self.report))

    def test_changed_source_invalidates_evidence(self):
        self.source.write_bytes(b'// changed after comparison\n')
        self.assertFalse(source_pinned(self.root,self.report))

    def test_source_pin_cannot_escape_component_directory(self):
        self.report['rebuilt_source_sha256']={'outside.cpp':'unused'}
        self.assertFalse(source_pinned(self.root,self.report))


if __name__=='__main__':
    unittest.main()
