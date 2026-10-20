"""Authored log fixtures verify evidence boundaries, not game execution."""
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from verify_ge6_x11_boot import observations

LOG='''Linux fixture 6.0 x86_64 Linux
GE6_GRAPHICS_ENV=X11 DISPLAY=:99 LIBGL_ALWAYS_SOFTWARE=1 ALSOFT_DRIVERS=null
GE6_IMAGE_KIND=local-platform
GE6_IMAGE_MAPPED base=0x140000000 entry=0x1403fc96c supported_imports=200 blocked_imports=224
GE6_NATIVE_NT_TIB_READY
GE6_ORIGINAL_CRT_ENTER
GE6_ORIGINAL_MAIN_REACHED
GE6_X11_WINDOW_CREATE hwnd=0x10 xid=0x20 width=1280 height=720 mapped=0 procedure=0x14037cf00 caller=0x1403833ce
GE6_X11_DC window=0x10 dc=0x30 drawable=0x20
GE6_X11_WINDOW_SHOW hwnd=0x10 visible=1 caller=0x14035a374
GE6_GLX_CONTEXT_CREATE handle=0x40 dc=0x30 caller=0x1403820dc
GE6_GLX_CURRENT handle=0x40 renderer=Authored fixture version=0.0 fixture caller=0x140381e2b
GE6_BLOCKED_IMPORT GDI32.dll!GetDeviceGammaRamp caller=0x140382f02 os_calls=2000
GE6_X11_WINDOW_SNAPSHOT hwnd=0x10 xid=0x20 width=1280 height=720 viewable=1 procedure=0x14037cf00
GE6_X11_CAPTURE_SAVED width=1280 height=720 path=/work/fixture.png
GE6_GRAPHICS_RESULT=78
'''


class X11EvidenceTests(unittest.TestCase):
    def test_visible_context_is_not_rendering_or_playability(self):
        result=observations(LOG)
        self.assertTrue(result['original_main_window']['viewable'])
        self.assertEqual(result['original_main_window_gl_context']['dc'],'0x30')
        for field in ('original_frame_rendered','original_renderer_initialized','menu_verified','playable_game_verified'):
            self.assertFalse(result[field])

    def test_hidden_window_is_rejected(self):
        with self.assertRaisesRegex(ValueError,'visible window'):
            observations(LOG.replace('viewable=1','viewable=0'))

    def test_probe_context_is_rejected(self):
        with self.assertRaisesRegex(ValueError,'Probe contexts'):
            observations(LOG.replace('caller=0x1403820dc','caller=0x140381837'))

    def test_unrelated_dc_is_rejected(self):
        with self.assertRaisesRegex(ValueError,'Probe contexts'):
            observations(LOG.replace('window=0x10 dc=0x30','window=0x99 dc=0x30'))

    def test_fault_and_reference_provenance_are_rejected(self):
        for log in (LOG+'GE6_NATIVE_FAULT signal=11\n',LOG.replace('local-platform','reference')):
            with self.assertRaisesRegex(ValueError,'faulted or wrong-provenance'):
                observations(log)
