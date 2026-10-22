"""Authored log fixtures verify evidence boundaries, not game execution."""
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from verify_ge6_x11_boot import observations,confirm_visual

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
    def test_slow_swaps_remain_measured_progress_not_automatic_playability(self):
        result=observations(LOG+'GE6_NATIVE_SWAP frame=30 draws=824 dc=0x30 context=0x40 caller=0x14000c267 duration_us=4340681\n')
        self.assertEqual(result['original_buffer_swaps'][0]['frame'],30)
        self.assertEqual(result['original_buffer_swaps'][0]['duration_us'],4340681)
        self.assertFalse(result['menu_verified'])
        self.assertFalse(result['playable_game_verified'])

    def test_menu_needs_application_loop_not_only_loading_draws(self):
        calls='GE6_NATIVE_GL_CALL name=glDrawArrays caller=0x14006560d context=0x40\n'
        swap='GE6_NATIVE_SWAP frame=1 draws=2 dc=0x30 context=0x40 caller=0x14000922e\n'
        result=observations(LOG+calls+swap)
        confirm_visual(result,'loading-screen')
        with self.assertRaisesRegex(ValueError,'application-loop'):
            confirm_visual(result,'main-menu')
        result=observations(LOG+calls+swap.replace('0x14000922e','0x14000c267'))
        confirm_visual(result,'main-menu')
        self.assertTrue(result['menu_verified'])
        self.assertFalse(result['menu_interaction_verified'])
        self.assertFalse(result['playable_game_verified'])

    def test_career_navigation_needs_pointer_input_and_visual_confirmation(self):
        calls='GE6_NATIVE_GL_CALL name=glDrawArrays caller=0x14006560d context=0x40\nGE6_NATIVE_SWAP frame=10 draws=32 dc=0x30 context=0x40 caller=0x14000c267\n'
        pointer='GE6_X11_MOUSE_EVENT hwnd=0x10 message=0x201 keys=0x1 root_x=300 root_y=280 time=1234\n'
        for extra in ('',pointer.replace('hwnd=0x10','hwnd=0x99')):
            with self.assertRaisesRegex(ValueError,'pointer input'):
                confirm_visual(observations(LOG+calls+extra),'career-screen')
        result=observations(LOG+calls+pointer)
        self.assertFalse(result['menu_verified'])
        confirm_visual(result,'career-screen')
        self.assertTrue(result['menu_interaction_verified'])
        self.assertFalse(result['playable_game_verified'])

    def test_explicit_x11_display_is_recorded(self):
        self.assertEqual(observations(LOG.replace('DISPLAY=:99','DISPLAY=:98'))['x11_display'],':98')

    def test_map_selection_and_loading_screen_do_not_establish_a_loaded_world(self):
        drawing='GE6_NATIVE_GL_CALL name=glDrawArrays caller=0x14006560d context=0x40\nGE6_NATIVE_SWAP frame=30 draws=200 dc=0x30 context=0x40 caller=0x14000c267\n'
        pointer='GE6_X11_MOUSE_EVENT hwnd=0x10 message=0x201 keys=0x1 root_x=865 root_y=603 time=1234\n'
        for frame in ('difficulty-screen','map-selection-screen','mission-loading-screen'):
            with self.subTest(frame=frame):
                with self.assertRaisesRegex(ValueError,'pointer input'):
                    confirm_visual(observations(LOG+drawing),frame)
                result=observations(LOG+drawing+pointer)
                confirm_visual(result,frame)
                self.assertTrue(result['menu_interaction_verified'])
                self.assertFalse(result['original_renderer_initialized'])
                self.assertFalse(result['full_engine_initialization_verified'])
                self.assertFalse(result['playable_game_verified'])
                self.assertEqual(result.get('original_mission_loading_screen_verified',False),frame=='mission-loading-screen')

    def test_bounded_run_is_not_complete_startup(self):
        text=LOG.replace('GE6_BLOCKED_IMPORT GDI32.dll!GetDeviceGammaRamp caller=0x140382f02 os_calls=2000\n','').replace('GE6_GRAPHICS_RESULT=78','GE6_GRAPHICS_BOUNDED_TIMEOUT seconds=90 elapsed=90.100\nGE6_GRAPHICS_RESULT=143')
        result=observations(text)
        self.assertEqual(result['stopped_at']['kind'],'controlled-test-timeout')
        self.assertFalse(result['stopped_at']['startup_completion_verified'])
        self.assertFalse(result['menu_verified'])
        for bad in (text.replace('elapsed=90.100','elapsed=2.000'),text.replace('GE6_GRAPHICS_BOUNDED_TIMEOUT','absent-marker'),text+'GE6_NATIVE_FAULT signal=8\n'):
            with self.assertRaises(ValueError):
                observations(bad)

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

    def test_draw_and_swap_use_the_original_main_context(self):
        drawing='GE6_NATIVE_GL_CALL name=glDrawArrays caller=0x14006560d context=0x40\n'
        swap='GE6_NATIVE_SWAP frame=1 draws=2 dc=0x30 context=0x40 caller=0x14000922e\n'
        result=observations(LOG+drawing+swap)
        self.assertEqual(result['original_gl_calls'][0]['name'],'glDrawArrays')
        self.assertEqual(result['original_buffer_swaps'][0]['draw_calls'],2)
        self.assertFalse(result['original_frame_rendered']) # Call logs alone do not confirm pixels.
        self.assertFalse(observations(LOG+swap.replace('dc=0x30','dc=0x99'))['original_buffer_swaps'])

    def test_authored_host_drawing_is_not_original_execution(self):
        for extra in ('GE6_NATIVE_GL_CALL name=glDrawArrays caller=0x1234 context=0x40\n',
                      'GE6_NATIVE_SWAP frame=1 draws=2 dc=0x30 context=0x40 caller=0x1234\n'):
            with self.assertRaisesRegex(ValueError,'outside original image'):
                observations(LOG+extra)
