"""Resolve locally owned oracle inputs, with hash-pinned package fallback.

The inventory remains the record of the original installation. If that drive is
unavailable, only these previously measured original resources may be read from
the existing local package. This never prepares/downloads or publishes assets.
"""
import hashlib
import json
from pathlib import Path,PurePosixPath
from inventory import ROOT

PINNED_ASSETS={
    'data/vehicles/steerable/deutz/deutzAgrostar661.i3d':'8538c582023bc310868e8748da380a81d9ff5c83f329b8f532940af408161b31',
    'data/vehicles/steerable/newHolland/newHollandT8320.i3d':'01cdbe80f9d274db6f2af4b839067093e6c829df8a364093ce1c299ddc573877',
    'data/vehicles/tools/krone/kroneSwadro2000.i3d':'6b1ba51b6f63e1bf2252b08cff45e0eae3364544451445d987878047cafddcf8',
    'data/vehicles/tools/kuhn/kuhnGA8521.i3d':'bbf51911b7f7b45e56e28fbbb2caf782f36f4ecc9afdf8cfad4e1a8526243378',
    'data/vehicles/tools/poettinger/poettingerTop1252.i3d':'5b2c621fb5f80bdd0facf6c856d440d7b35273fe6bd19d460196fbfc4e9852a9',
    'data/vehicles/tools/poettinger/poettingerTop1252.i3d.anim':'08aaf88f93f49775083624b141ca53e8c56464d6595dcf8adef13de120aa1b75',
}

def reference_asset(relative):
    key=PurePosixPath(relative)
    if not key.parts or key.is_absolute() or '..' in key.parts or '\\' in relative or ':' in relative:raise ValueError('Expected a contained game-relative resource path')
    inventory=ROOT/'analysis/inventory.json'
    directory=Path(json.loads(inventory.read_text(encoding='utf-8'))['game_root']) if inventory.is_file() else ROOT/'inputs/unavailable-game'
    original=directory/key
    if original.is_file():return original
    package=ROOT/'build/ge6-local';cached=package/key
    if not cached.is_file():return original
    expected=PINNED_ASSETS.get(key.as_posix())
    if not expected or not cached.resolve().is_relative_to(package.resolve()):raise ValueError('Cached reference resource has no contained version pin')
    if hashlib.sha256(cached.read_bytes()).hexdigest()!=expected:raise ValueError('Cached reference resource differs from the recorded original hash')
    return cached
