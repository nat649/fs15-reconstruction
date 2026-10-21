"""Private-VM bounded execution controller; no original game code or resources."""
import argparse
from pathlib import Path
import subprocess
import time


def run(timeout, command):
    started=time.monotonic()
    process=subprocess.Popen(command)
    try:
        result=process.wait(timeout=timeout)
    except subprocess.TimeoutExpired:
        print(f'GE6_GRAPHICS_BOUNDED_TIMEOUT seconds={timeout} elapsed={time.monotonic()-started:.3f}',flush=True)
        # Diagnose waits without pretending that a living process is a working game.
        for task in sorted(Path(f'/proc/{process.pid}/task').glob('*')):
            try:
                name=(task/'comm').read_text().strip()
                wait=(task/'wchan').read_text().strip()
                syscall=(task/'syscall').read_text().strip()
                print(f'GE6_PRIVATE_THREAD_SNAPSHOT tid={task.name} name={name!r} wait={wait} syscall={syscall}',flush=True)
            except OSError:
                pass
        snapshot=Path('/work/FS15GE6X11Snapshot')
        if snapshot.is_file():
            try:
                captured=subprocess.run([str(snapshot),'/work/ge6-native-final-screen.png'],timeout=15)
                print(f'GE6_GRAPHICS_FINAL_CAPTURE_RESULT={captured.returncode}',flush=True)
            except (OSError,subprocess.TimeoutExpired):
                print('GE6_GRAPHICS_FINAL_CAPTURE_RESULT=unavailable',flush=True)
        process.terminate()
        try:
            result=process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            result=process.wait(timeout=5)
    print(f'GE6_GRAPHICS_RESULT={result if result>=0 else 128-result}',flush=True)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--timeout',type=int,default=90)
    parser.add_argument('command',nargs=argparse.REMAINDER)
    args=parser.parse_args()
    if args.timeout<1 or not args.command:
        parser.error('A positive timeout and an executable command are required')
    run(args.timeout,args.command)
