"""Only authored bytes are used to test oracle resource path/hash selection."""
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
import local_reference_assets as assets

class ReferenceAssets(unittest.TestCase):
    def setUp(self):
        fixtures=Path(__file__).resolve().parents[1]/'build/test-fixtures/reference-assets';fixtures.mkdir(parents=True,exist_ok=True)
        self.temp=tempfile.TemporaryDirectory(dir=fixtures);self.addCleanup(self.temp.cleanup);self.root=Path(self.temp.name)
        self.patch=patch.object(assets,'ROOT',self.root);self.patch.start();self.addCleanup(self.patch.stop)
        (self.root/'analysis').mkdir();self.game=self.root/'original';(self.root/'analysis/inventory.json').write_text(json.dumps({'game_root':str(self.game)}))
        self.relative='data/fixture.i3d';self.cached=self.root/'build/ge6-local'/self.relative;self.cached.parent.mkdir(parents=True);self.cached.write_bytes(b'authored fixture')
        self.pins=patch.dict(assets.PINNED_ASSETS,{self.relative:hashlib.sha256(self.cached.read_bytes()).hexdigest()},clear=True);self.pins.start();self.addCleanup(self.pins.stop)
    def test_existing_original_is_preferred(self):
        original=self.game/self.relative;original.parent.mkdir(parents=True);original.write_bytes(b'authored original fixture')
        self.assertEqual(assets.reference_asset(self.relative),original)
    def test_fallback_requires_matching_hash(self):
        self.assertEqual(assets.reference_asset(self.relative),self.cached)
        self.cached.write_bytes(b'changed fixture')
        with self.assertRaisesRegex(ValueError,'differs'):assets.reference_asset(self.relative)
    def test_unpinned_cached_file_is_rejected(self):
        with patch.dict(assets.PINNED_ASSETS,{},clear=True):
            with self.assertRaisesRegex(ValueError,'pin'):assets.reference_asset(self.relative)
    def test_relative_resource_cannot_escape(self):
        for relative in ['../secret','/absolute','C:/absolute','data\\outside']:
            with self.subTest(relative=relative),self.assertRaises(ValueError):assets.reference_asset(relative)

if __name__=='__main__':unittest.main()
