"""Authored launch-plan fixtures. No original game or Steam process executes."""
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from prepare_native_engine import configure_launch
from unified_launcher import home_plan


class UnifiedLaunchPlans(unittest.TestCase):
    def setUp(self):
        fixtures = Path(__file__).resolve().parents[1] / 'build/test-fixtures/unified-launcher'
        fixtures.mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=fixtures)
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / 'FS15EngineRebuilt.exe').write_bytes(b'authored fixture, not executable')
        maps = self.root / 'recovered/gar/data/maps'
        maps.mkdir(parents=True)
        (maps / 'map02.i3d').write_text('<authored-fixture/>')
        self.profile = self.root / 'profile'
        self.profile.mkdir()
        self.steam = self.root / 'steam-client.exe'
        self.steam.write_bytes(b'authored fixture, not executable')

    def manifest(self):
        return dict(profile=str(self.profile), game_directory=str(self.root / 'common/FS15'),
                    command=['launcher.exe', '--game-dir', str(self.root / 'common/FS15'), '--', '-profile', str(self.profile)])

    def test_home_forwards_map_and_difficulty_without_steam(self):
        plan = home_plan(self.root, 'Map02', 3)
        self.assertEqual(plan['command'][-5:], ['--map', 'Map02', '--play', '--difficulty', '3'])
        self.assertNotIn('--steam', plan['command'])
        self.assertEqual(plan['command'][2], str(self.root.resolve()))

    def test_home_rejects_missing_resources_and_invalid_selections(self):
        for map_id, difficulty in [('Map01', 1), ('../Map02', 2), ('Map02', 4)]:
            with self.assertRaises(ValueError):
                home_plan(self.root, map_id, difficulty)

    def test_explicit_direct_never_substitutes_steam(self):
        with patch('prepare_native_engine.restart_safe_profile', return_value=self.profile):
            plan = configure_launch(self.manifest(), steam=self.steam, direct=True, dry_run=True)
        self.assertNotIn('--steam', plan['command'])
        self.assertIn('--dry-run', plan['command'])
        self.assertNotIn('launch_requested_utc', plan)

    def test_steam_transport_is_explicit_in_command(self):
        with patch('prepare_native_engine.restart_safe_profile', return_value=self.profile):
            plan = configure_launch(self.manifest(), steam=self.steam, dry_run=True)
        self.assertEqual(plan['command'][2:4], ['--steam', str(self.steam.resolve())])


if __name__ == '__main__':
    unittest.main()
