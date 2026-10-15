"""Authored PE fixtures for local-runtime preparation. No original code executes."""
import hashlib
from pathlib import Path
import struct
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from inventory import PE
from prepare_local_engine import local_image


def fixture():
    raw=bytearray(0x4400)
    raw[:2]=b'MZ';struct.pack_into('<I',raw,0x3c,0x80);raw[0x80:0x84]=b'PE\0\0'
    struct.pack_into('<HHIIIHH',raw,0x84,0x8664,4,0,0,0,0xf0,0x22)
    opt=0x98;struct.pack_into('<H',raw,opt,0x20b);struct.pack_into('<I',raw,opt+16,0x1694310)
    struct.pack_into('<Q',raw,opt+24,0x140000000);struct.pack_into('<I',raw,opt+60,0x400)
    struct.pack_into('<H',raw,opt+68,2);struct.pack_into('<I',raw,opt+108,16)
    struct.pack_into('<II',raw,opt+112+8,0x472100,40)
    for i,(name,rva,offset) in enumerate(((b'.text',0xc000,0x400),(b'.crt',0x3fc000,0x1400),
                                         (b'.idata',0x472000,0x2400),(b'.bind',0x1694000,0x3400))):
        header=opt+0xf0+i*40;raw[header:header+8]=name.ljust(8,b'\0')
        struct.pack_into('<IIII',raw,header+8,0x1000,rva,0x1000,offset)
        struct.pack_into('<I',raw,header+36,0x60000020)
    raw[0xc10:0xc15]=bytes.fromhex('48896c2418') # Match only the bounded signature, not a native function.
    struct.pack_into('<IIIII',raw,0x2500,0x472400,0,0,0x472200,0x472420)
    name=b'steam_api64.dll\0';raw[0x2600:0x2600+len(name)]=name
    struct.pack_into('<Q',raw,0x2800,0x472440);struct.pack_into('<Q',raw,0x2820,0x472440)
    symbol=b'\0\0SteamAPI_RunCallbacks\0';raw[0x2840:0x2840+len(symbol)]=symbol
    return bytes(raw)


class LocalEngineImage(unittest.TestCase):
    def patch(self,data):return local_image(data,hashlib.sha256(data).hexdigest())

    def test_only_explicit_bootstrap_fields_are_changed(self):
        source=fixture();result=self.patch(source)
        self.assertEqual(len(source),len(result));self.assertEqual(PE(result).entry_rva,0x3fc96c)
        self.assertEqual(PE(result).imports()[0]['dll'],'fs15_local.dll')
        self.assertEqual(result[0xc10],0xc3)
        allowed=set(range(0x98+16,0x98+20))|{0xc10}|set(range(0x2600,0x2600+16))
        self.assertTrue(all(a==b or i in allowed for i,(a,b) in enumerate(zip(source,result))))
        self.assertEqual(PE(source).entry_rva,0x1694310)

    def test_unpinned_image_is_rejected_before_parsing(self):
        with self.assertRaisesRegex(ValueError,'pinned'):local_image(fixture())

    def test_changed_native_signature_is_rejected(self):
        source=bytearray(fixture());source[0xc10]=0
        with self.assertRaisesRegex(ValueError,'signature'):self.patch(source)

    def test_unknown_import_cannot_reach_the_bounded_adapter(self):
        source=bytearray(fixture());source[0x2842]=ord('X')
        with self.assertRaisesRegex(ValueError,'import interface'):self.patch(source)

    def test_ambiguous_library_name_is_rejected(self):
        with self.assertRaisesRegex(ValueError,'ambiguous'):self.patch(fixture()+b'steam_api64.dll\0')

    def test_unexpected_tls_or_entry_layout_is_rejected(self):
        source=bytearray(fixture());struct.pack_into('<II',source,0x98+112+9*8,0x1694000,40)
        with self.assertRaisesRegex(ValueError,'TLS layout'):self.patch(source)
        source=bytearray(fixture());struct.pack_into('<I',source,0x98+16,0xc810)
        with self.assertRaisesRegex(ValueError,'layout'):self.patch(source)


if __name__=='__main__':unittest.main()
