"""End the one private VM GE6 diagnostic after capturing its actual last frame."""
import argparse
import os
from pathlib import Path
import signal
import subprocess
import time

from ge6_vm_loading_probe import find_process


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--display',default=':98')
    parser.add_argument('--reason',choices=('world-frame-observed','diagnostic-complete'),required=True)
    args=parser.parse_args()
    pid=find_process()
    # Pin this process, rather than risk signalling a subsequently reused PID.
    descriptor=os.pidfd_open(pid)
    try:
        if os.readlink(f'/proc/{pid}/exe')!='/work/FS15GE6LinuxBoot':
            raise RuntimeError('The diagnostic process changed before capture')
        with Path('/work/boot.log').open('ab',buffering=0) as log:
            captured=subprocess.run(['/work/FS15GE6X11Snapshot','/work/ge6-native-final-screen.png'],
                                    env=dict(os.environ,DISPLAY=args.display),stdout=log,stderr=log,timeout=20)
            log.write(f'\nGE6_GRAPHICS_FINAL_CAPTURE_RESULT={captured.returncode}\n'.encode())
            signal.pidfd_send_signal(descriptor,signal.SIGTERM)
            log.write(f'\nGE6_GRAPHICS_OPERATOR_STOP pid={pid} monotonic={time.monotonic():.3f} reason={args.reason}\n'.encode())
    finally:
        os.close(descriptor)


if __name__=='__main__':
    main()
