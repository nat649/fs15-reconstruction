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
from verify_native_engine import inspect_log, verify


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

    def native_log(self):
        return ('GIANTS Engine Runtime 6.0.2 64bit (authored fixture)\n'
                'OpenGL initialization successful\n'
                'Load mod: FS15_ReconstructionProbe\n'
                'FS15_NATIVE_PROBE_BEGIN\n'
                'FS15_NATIVE_RECOVERED_LUA_OK comparisons=104\n')

    def test_descriptor_discovery_is_not_script_execution(self):
        text = self.native_log().split('FS15_NATIVE_PROBE_BEGIN')[0]
        report = inspect_log(text)
        self.assertTrue(report['native_boot_verified'])
        self.assertFalse(report['recovered_script_execution_verified'])

    def test_order_and_comparison_count_are_required(self):
        for text in (self.native_log().replace('comparisons=104', 'comparisons=103'),
                     self.native_log().replace('FS15_NATIVE_PROBE_BEGIN\n', '') + 'FS15_NATIVE_PROBE_BEGIN\n',
                     self.native_log().replace('GIANTS Engine Runtime', 'Authored test host')):
            self.assertFalse(inspect_log(text)['recovered_script_execution_verified'])

    def test_stale_log_cannot_validate_new_launch(self):
        manifest = prepare(self.root, self.game)
        (Path(manifest['profile']) / 'log.txt').write_text(self.native_log())
        manifest['launch_requested_utc'] = '2099-01-01T00:00:00+00:00'
        (self.root / 'build/native-engine/manifest.json').write_text(json.dumps(manifest))
        with self.assertRaisesRegex(ValueError, 'predates'):
            verify(self.root)

    def test_lua_failure_prevents_validation(self):
        manifest = prepare(self.root, self.game)
        (Path(manifest['profile']) / 'log.txt').write_text(self.native_log() + 'Error: Running LUA method loadMap\n')
        with self.assertRaisesRegex(ValueError, 'Lua errors'):
            verify(self.root)


if __name__ == '__main__':
    unittest.main()
