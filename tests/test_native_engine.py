"""Asset-free staging guards; none of these tests execute a GIANTS runtime."""
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
from xml.etree import ElementTree as ET

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from prepare_native_engine import MOD_NAME, WHEELS, prepare


class NativeEnginePreparation(unittest.TestCase):
    def setUp(self):
        fixtures = Path(__file__).resolve().parents[1] / 'build/test-fixtures/native-engine-guards'
        fixtures.mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=fixtures)
        if not Path(self.temp.name).resolve().is_relative_to(fixtures.resolve()):
            raise AssertionError('Temporary fixture escaped the workspace')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / 'workspace'
        self.game = Path(self.temp.name) / 'installed-game'
        (self.game / 'x64').mkdir(parents=True)
        (self.game / 'FarmingSimulator2015.xml').write_text('<startup/>')
        (self.game / 'x64/FarmingSimulator2015Game.exe').write_bytes(b'authored fixture, not executable')
        self.source = self.root / 'recovered/lua/dataS/scripts/vehicles/WheelsUtil.lua'
        self.source.parent.mkdir(parents=True)
        self.source.write_text('WheelsUtil = {}\n')
        (self.root / 'analysis').mkdir()
        row = dict(path=WHEELS, status='exact_match', source_sha256=hashlib.sha256(self.source.read_bytes()).hexdigest())
        (self.root / 'analysis/lua-recovery.json').write_text(json.dumps(dict(files=[row])))
        probes = self.root / 'src/native_engine'
        probes.mkdir(parents=True)
        for name in ('bootstrap.lua', 'verify.lua'):
            (probes / name).write_text('-- authored fixture\n')

    def test_isolated_profile_and_source_order(self):
        before = (self.game / 'FarmingSimulator2015.xml').read_bytes()
        manifest = prepare(self.root, self.game)
        self.assertFalse(manifest['native_boot_verified'])
        self.assertFalse(manifest['recovered_script_execution_verified'])
        self.assertEqual(manifest['command'][-2], '-profile')
        self.assertFalse(manifest['command'][-1].endswith('/'))
        mod = Path(manifest['profile']) / 'mods' / MOD_NAME
        sources = ET.parse(mod / 'modDesc.xml').findall('./extraSourceFiles/sourceFile')
        self.assertEqual([x.get('filename') for x in sources], ['bootstrap.lua', 'recovered/WheelsUtil.lua', 'verify.lua'])
        self.assertEqual((mod / 'recovered/WheelsUtil.lua').read_bytes(), self.source.read_bytes())
        self.assertEqual((self.game / 'FarmingSimulator2015.xml').read_bytes(), before)

    def test_changed_source_rejected_before_staging(self):
        self.source.write_text('changed source\n')
        with self.assertRaisesRegex(ValueError, 'unchanged hash'):
            prepare(self.root, self.game)
        self.assertFalse((self.root / 'build').exists())

    def test_preferred_bytecode_is_rejected(self):
        manifest = prepare(self.root, self.game)
        (Path(manifest['profile']) / 'mods' / MOD_NAME / 'recovered/WheelsUtil.luc').write_bytes(b'authored fixture')
        with self.assertRaisesRegex(ValueError, 'neighboring .luc'):
            prepare(self.root, self.game)

    def test_profile_link_outside_workspace_is_rejected(self):
        (self.root / 'build').mkdir()
        try:
            (self.root / 'build/native-engine').symlink_to(self.game, target_is_directory=True)
        except OSError:
            self.skipTest('Directory symlink creation unavailable')
        with self.assertRaisesRegex(ValueError, 'stay inside'):
            prepare(self.root, self.game)
        self.assertFalse((self.game / 'profile').exists())


if __name__ == '__main__':
    unittest.main()
