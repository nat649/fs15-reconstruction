"""Exercise the authored local adapter, never the original Steam SDK library."""
import argparse
import ctypes as C
import hashlib
import json
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from inventory import PE,ROOT
from prepare_local_engine import EXPORTS


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--dll',type=Path,required=True);args=parser.parse_args()
    data=args.dll.read_bytes();pe=PE(data)
    assert EXPORTS<={row['name'] for row in pe.exports()}
    imported=[row['dll'].lower() for row in pe.imports()]
    assert not any('steam' in name or name in ('ws2_32.dll','winhttp.dll','wininet.dll') for name in imported)
    dll=C.CDLL(str(args.dll.resolve()))
    for name in ('SteamClient','SteamApps','SteamUser','SteamUserStats'):
        fn=getattr(dll,name);fn.argtypes=[];fn.restype=C.c_void_p;assert fn() is None,name
    dll.SteamAPI_Init.argtypes=[];dll.SteamAPI_Init.restype=C.c_bool;assert not dll.SteamAPI_Init()
    dll.SteamAPI_RestartAppIfNecessary.argtypes=[C.c_uint32];dll.SteamAPI_RestartAppIfNecessary.restype=C.c_bool
    assert not dll.SteamAPI_RestartAppIfNecessary(313160)
    dll.SteamUtils.argtypes=[];dll.SteamUtils.restype=C.c_void_p;utils=dll.SteamUtils()
    table=C.c_void_p.from_address(utils).value
    app_id=C.WINFUNCTYPE(C.c_uint32,C.c_void_p)(C.c_void_p.from_address(table+9*8).value)
    assert app_id(utils)==313160
    report=dict(status='passed',adapter_sha256=hashlib.sha256(data).hexdigest(),
                exports_verified=len(EXPORTS),client_user_apps_stats_unavailable=True,init_success=False,
                restart_requested=False,network_or_steam_imports=False,app_identity=313160,
                scope='Authored local adapter contract, not Steam service/entitlement emulation or a full GE6 reconstruction')
    (ROOT/'build/ge6-local/adapter-validation.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))


if __name__=='__main__':main()
