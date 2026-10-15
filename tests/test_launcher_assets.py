"""Asset-preparation fixtures. All game/decoder/tool inputs are authored data."""
from contextlib import redirect_stdout
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from launcher_assets import (GAME_FILES,HOME_FILES,TERRAIN_FILES,checked_output,copy_verified,
    detect_installations,library_paths,preflight_outputs,preparation_lock,prepare_assets,resource_status,sha)


def ready_host(root):
    def write(relative,data=b'authored fixture'):
        path=root/relative;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data);return path
    write('FS15EngineRebuilt.exe')
    for name in HOME_FILES:write('recovered/gar/'+name)
    for map_id in ('map01','map02'):
        for name in ('fruit_density.grle','cultivator_density.grle'):write(f'recovered/gar/data/maps/{map_id}/{name}')
    for name in ('gui/MainScreen.lua','vehicles/WheelsUtil.lua'):write('recovered/lua/dataS/scripts/'+name)
    write('recovered/mission-provenance.json',json.dumps({'vehicles':[{'runtime':'data/vehicles/fixture.runtime.i3d'}]}).encode())
    write('recovered/gar/data/vehicles/fixture.runtime.i3d');write('recovered/gar/data/vehicles/fixture.runtime.meshes')
    return write


class LauncherAssets(unittest.TestCase):
    def setUp(self):
        base=Path(__file__).resolve().parents[1]/'build/test-fixtures/launcher-assets';base.mkdir(parents=True,exist_ok=True)
        self.temp=tempfile.TemporaryDirectory(dir=base);self.addCleanup(self.temp.cleanup)
        self.base=Path(self.temp.name);self.root=self.base/'workspace';self.root.mkdir()
        self.game=self.base/'installed FS15';self.game.mkdir()
        for name in set(GAME_FILES+TERRAIN_FILES):
            path=self.game/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(b'authored original fixture')

    def setup_prepared(self):
        write=ready_host(self.root)
        write('tools/LuaJIT/src/luajit.exe');write('tools/luajit-decompiler/main.py');write('build/shape-bridge/FS15ShapeBridge.dll')
        source=write('recovered/lua/dataS/scripts/fixture.lua',b'-- authored source fixture')
        write('analysis/lua-recovery.json',json.dumps({'files':[{'path':'scripts/fixture.luc','status':'exact_match','source_sha256':sha(source)}]}).encode())
        for name in ('dataS','dataS2'):
            archive=self.game/(name+'.gar');archive.write_bytes(b'authored archive fixture')
            asset=write(f'recovered/gar/{name}/gui/fixture.xml')
            write(f'analysis/{name}-extraction.json',json.dumps({'archive_sha256':sha(archive),'files':[{'name':'gui/fixture.xml','sha256':sha(asset)}]}).encode())
        return write

    def test_steam_libraries_handle_current_and_legacy_vdf(self):
        paths=list(library_paths('"libraryfolders" { "0" { "path" "D:\\\\Games\\\\Steam" } "1" "E:\\\\Library" }'))
        self.assertEqual([str(p) for p in paths],['D:\\Games\\Steam','E:\\Library'])

    def test_detection_uses_library_and_app_manifest(self):
        steam=self.base/'Steam';library=self.base/'other library';(steam/'steamapps').mkdir(parents=True)
        (steam/'steamapps/libraryfolders.vdf').write_text('"path" '+json.dumps(library.as_posix()))
        (library/'steamapps').mkdir(parents=True)
        import shutil
        installed=library/'steamapps/common/Custom FS15';shutil.copytree(self.game,installed)
        (library/'steamapps/appmanifest_313160.acf').write_text('"installdir" "Custom FS15"')
        with patch.dict(os.environ,{'FS15_GAME_DIR':''}):found=detect_installations(self.root,[steam])
        self.assertEqual(found,[installed.resolve()])

    def test_invalid_installations_are_not_detected(self):
        with patch.dict(os.environ,{'FS15_GAME_DIR':str(self.root)}):
            self.assertEqual(detect_installations(self.root,[]),[])

    def test_verified_copy_repairs_corruption_and_skips_matching_files(self):
        source=self.game/'FarmingSimulator2015.xml';target=self.root/'recovered/gar/fixture.xml'
        self.assertTrue(copy_verified(source,target));stamp=target.stat().st_mtime_ns
        self.assertFalse(copy_verified(source,target));self.assertEqual(target.stat().st_mtime_ns,stamp)
        target.write_bytes(b'corrupted');self.assertTrue(copy_verified(source,target));self.assertEqual(target.read_bytes(),source.read_bytes())

    def test_interrupted_copy_is_repaired(self):
        source=self.game/'FarmingSimulator2015.xml';target=self.root/'fixture.xml'
        target.with_name(target.name+'.importing').write_bytes(b'interrupted copy')
        self.assertTrue(copy_verified(source,target));self.assertEqual(target.read_bytes(),source.read_bytes())
        self.assertFalse(target.with_name(target.name+'.importing').exists())

    def test_output_traversal_and_links_are_rejected(self):
        with self.assertRaises(ValueError):checked_output(self.root,'../outside')
        outside=self.base/'outside';outside.mkdir();link=self.root/'recovered'
        try:link.symlink_to(outside,target_is_directory=True)
        except OSError:self.skipTest('Creating directory symlinks requires privileges on this Windows host')
        with self.assertRaises(ValueError):preflight_outputs(self.root)

    def test_only_one_preparation_can_hold_the_lock(self):
        with preparation_lock(self.root):
            with self.assertRaises(ValueError):
                with preparation_lock(self.root):pass
        with preparation_lock(self.root):pass

    def test_presence_and_interrupted_import_are_separate(self):
        write=ready_host(self.root);self.assertTrue(resource_status(self.root)['home_launch_ready'])
        write('build/launcher/assets.json',b'{"status":"preparing"}')
        status=resource_status(self.root);self.assertTrue(status['assets_present']);self.assertFalse(status['home_launch_ready'])
        (self.root/'recovered/gar/data/vehicles/fixture.runtime.meshes').unlink()
        self.assertFalse(resource_status(self.root)['assets_present'])

    def test_preparation_reuses_existing_sources_and_records_hashes(self):
        self.setup_prepared();commands=[]
        with patch('launcher_assets.shutil.which',return_value='authored-pwsh'),redirect_stdout(io.StringIO()):
            report=prepare_assets(self.root,self.game,commands.append)
        self.assertEqual(report['status'],'complete');self.assertEqual(len(commands),3)
        self.assertTrue(report['resource_status']['home_launch_ready'])
        self.assertFalse(report['installation_modified'])
        self.assertTrue(report['files']);self.assertEqual(report['files']['data/maps/map01/map01_dem.png']['sha256'],sha(self.game/'data/maps/map01/map01_dem.png'))
        self.assertFalse(any('extract_gar.py' in str(command) for command in commands))

    def test_missing_native_image_reports_the_real_blocker(self):
        self.setup_prepared();(self.root/'recovered/lua/dataS/scripts/fixture.lua').write_bytes(b'changed source')
        with patch('launcher_assets.shutil.which',return_value='authored-pwsh'),redirect_stdout(io.StringIO()):
            with self.assertRaisesRegex(ValueError,'native analysis image'):prepare_assets(self.root,self.game,lambda _:self.fail('No game capture or process should start'))
        report=json.loads((self.root/'build/launcher/assets.json').read_text())
        self.assertEqual(report['status'],'failed');self.assertFalse(resource_status(self.root)['home_launch_ready'])

    def test_failed_step_cannot_leave_a_complete_import_marker(self):
        self.setup_prepared()
        def fail(command):raise subprocess.CalledProcessError(2,command)
        with patch('launcher_assets.shutil.which',return_value='authored-pwsh'),redirect_stdout(io.StringIO()):
            with self.assertRaises(subprocess.CalledProcessError):prepare_assets(self.root,self.game,fail)
        self.assertEqual(json.loads((self.root/'build/launcher/assets.json').read_text())['status'],'failed')


if __name__=='__main__':unittest.main()
