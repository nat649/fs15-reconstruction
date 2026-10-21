"""Publish bounded, source-pinned observations from a PRIVATE GE6 X11 VM run."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess

from ge6_linux_images import LOCAL_IMAGE

ROOT = Path(__file__).resolve().parents[1]


def observations(text):
    required=('GE6_IMAGE_KIND=local-platform','GE6_NATIVE_NT_TIB_READY',
              'GE6_ORIGINAL_CRT_ENTER','GE6_ORIGINAL_MAIN_REACHED',
              'GE6_GRAPHICS_ENV=X11 DISPLAY=:99 LIBGL_ALWAYS_SOFTWARE=1 ALSOFT_DRIVERS=null',
              'GE6_GRAPHICS_RESULT=78')
    if any(marker not in text for marker in required) or 'GE6_NATIVE_FAULT' in text:
        raise ValueError('Incomplete, faulted or wrong-provenance X11 run')
    window=re.search(r'GE6_X11_WINDOW_CREATE hwnd=(0x\w+) xid=(0x\w+) width=(\d+) height=(\d+) mapped=\d procedure=(0x\w+) caller=0x1403833ce\b',text)
    if not window or window[5]!='0x14037cf00':
        raise ValueError('No original main-window creation/procedure')
    handle,xid,width,height,procedure=window.groups()
    snapshot=f'GE6_X11_WINDOW_SNAPSHOT hwnd={handle} xid={xid} width={width} height={height} viewable=1 procedure={procedure}'
    if snapshot not in text or not re.search(rf'GE6_X11_WINDOW_SHOW hwnd={handle} visible=1 caller=0x140[0-9a-f]+',text):
        raise ValueError('Window creation alone does not establish a visible window')
    dcs={dc for owner,dc,drawable in re.findall(r'GE6_X11_DC window=(0x\w+) dc=(0x\w+) drawable=(0x\w+)',text) if owner==handle and drawable==xid}
    contexts=[(context,dc) for context,dc in re.findall(r'GE6_GLX_CONTEXT_CREATE handle=(0x\w+) dc=(0x\w+) caller=0x1403820dc\b',text) if dc in dcs]
    active=None
    for context,dc in contexts:
        match=re.search(rf'GE6_GLX_CURRENT handle={context} renderer=(.+?) version=(.+?) caller=0x140381e2b\b',text)
        if match:
            active=dict(handle=context,dc=dc,renderer=match[1],version=match[2])
    if not active:
        raise ValueError('Probe contexts do not establish a main-window context')
    blocked=re.search(r'GE6_BLOCKED_IMPORT ([^!\s]+)!([^\s]+) caller=(0x[0-9a-f]+) os_calls=(\d+)',text)
    mapped=re.search(r'GE6_IMAGE_MAPPED base=0x140000000 entry=0x1403fc96c supported_imports=(\d+) blocked_imports=(\d+)',text)
    kernel=re.search(r'^Linux .+$',text,re.MULTILINE)
    capture=re.search(r'GE6_X11_CAPTURE_SAVED width=(\d+) height=(\d+) path=\S+',text)
    if not all((blocked,mapped,kernel,capture)):
        raise ValueError('Missing capture, kernel, loader or stop boundary')
    native_calls=[]
    for name,caller,context in re.findall(r'GE6_NATIVE_GL_CALL name=(\w+) caller=(0x[0-9a-f]+) context=(0x[0-9a-f]+)',text):
        if not 0x140000000<=int(caller,16)<0x140480000:
            raise ValueError('GL call is outside original image')
        if context==active['handle']:
            native_calls.append(dict(name=name,caller=caller,context=context))
    swaps=[]
    for frame,draws,dc,context,caller in re.findall(r'GE6_NATIVE_SWAP frame=(\d+) draws=(\d+) dc=(0x[0-9a-f]+) context=(0x[0-9a-f]+) caller=(0x[0-9a-f]+)',text):
        if not 0x140000000<=int(caller,16)<0x140480000 or int(frame)<1:
            raise ValueError('Swap is outside original image or has invalid count')
        if dc==active['dc'] and context==active['handle']:
            swaps.append(dict(frame=int(frame),draw_calls=int(draws),caller=caller))
    return dict(scope='Original GE6 main window and GL context on Linux; blank startup surface, not a playable game',
                kernel=kernel[0].strip(),image_kind='local-platform',original_image_sha256=LOCAL_IMAGE,
                existing_local_platform_bootstrap_modified=True,linux_loader_patches_instructions=False,
                wine_or_proton_used=False,replacement_engine_linked=False,
                original_main_window=dict(handle=handle,xid=xid,width=int(width),height=int(height),procedure=procedure,viewable=True),
                original_main_window_gl_context=active,
                original_gl_calls=native_calls,original_buffer_swaps=swaps,
                captured_root_dimensions=[int(capture[1]),int(capture[2])],
                bound_import_names=int(mapped[1]),stop_trampoline_names=int(mapped[2]),
                stopped_at=dict(library=blocked[1],symbol=blocked[2],caller=blocked[3],os_calls=int(blocked[4])),
                original_frame_rendered=False,original_renderer_initialized=False,menu_verified=False,
                full_engine_initialization_verified=False,playable_game_verified=False,
                vm_audio_backend='null',audible_audio_verified=False)


def verify(directory,executable,source_commit,run_id,verified_frame=None):
    raw=(directory/'boot.log').read_bytes();result=observations(raw.decode(errors='replace'))
    elf_digest=hashlib.sha256(executable.read_bytes()).hexdigest()
    if f'{elf_digest}  /work/FS15GE6LinuxBoot' not in raw.decode(errors='replace'):
        raise ValueError('VM-executed ELF differs from supplied artifact')
    screenshot=(directory/'ge6-native-screen.png').read_bytes()
    if screenshot[:8]!=b'\x89PNG\r\n\x1a\n' or list(struct.unpack('>II',screenshot[16:24]))!=result['captured_root_dimensions']:
        raise ValueError('Captured PNG dimensions differ')
    manifest=json.loads((directory/'staged-inputs.json').read_text())
    if manifest['x64/FarmingSimulator2015Game.exe']['sha256']!=LOCAL_IMAGE:
        raise ValueError('Staged original image differs')
    if verified_frame:
        if verified_frame!='loading-screen' or not any(item['name'] in ('glDrawArrays','glDrawElements') for item in result['original_gl_calls']) or not any(item['draw_calls']>0 for item in result['original_buffer_swaps']):
            raise ValueError('Visual confirmation needs matching original drawing and swaps')
        result.update(original_frame_rendered=True,visually_verified_content=verified_frame,
                      scope='Original GE6 loading screen rendered on Linux; native shader/texture/draw/swap path, not a playable game')
    journal=(directory/'original-log.txt').read_bytes()
    result['original_opengl_initialization_reported']=b'OpenGL initialization successful' in journal
    commit=subprocess.check_output(['git','rev-parse',source_commit+'^{commit}'],cwd=ROOT,text=True).strip()
    sources=subprocess.check_output(['git','ls-tree','-r','--name-only',commit,'src/ge6_linux'],cwd=ROOT,text=True).splitlines()
    sources+=['scripts/ge6_graphics_vm.py','scripts/setup_ge6_graphics_vm.py','scripts/verify_ge6_x11_boot.py','tests/check_ge6_x11.cpp','tests/test_ge6_x11_evidence.py','.github/workflows/ge6-linux-native-boot.yml']
    result.update(source_commit=commit,ci_run=f'https://github.com/nat649/fs15-reconstruction/actions/runs/{int(run_id)}',
                  committed_source_sha256={name:hashlib.sha256(subprocess.check_output(['git','show',commit+':'+name],cwd=ROOT)).hexdigest() for name in sources},
                  linux_elf_sha256=elf_digest,
                  private_boot_log_sha256=hashlib.sha256(raw).hexdigest(),
                  private_original_journal_sha256=hashlib.sha256(journal).hexdigest(),
                  captured_root_png_sha256=hashlib.sha256(screenshot).hexdigest(),
                  private_staged_inputs=manifest)
    return result


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--directory',type=Path,required=True)
    parser.add_argument('--exe',type=Path,required=True)
    parser.add_argument('--source-commit',required=True)
    parser.add_argument('--ci-run',type=int,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--verified-frame',choices=('loading-screen',))
    args=parser.parse_args()
    args.output.write_text(json.dumps(verify(args.directory,args.exe,args.source_commit,args.ci_run,args.verified_frame),indent=2)+'\n')
