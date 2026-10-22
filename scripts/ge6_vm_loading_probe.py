"""Read task counters from the pinned private Local GE6 process inside the VM.

Authored diagnostics for the observed x64 image layout. No game function is
called, no process memory is changed and no original payload bytes are emitted.
Counts describe scheduling progress, not a rendered or playable world.
"""
import argparse
import json
import os
from pathlib import Path
import struct
import time

TASK_QUEUE = 0x14162C330


def vector_count(first, last, capacity):
    if not first <= last <= capacity or (last-first) % 8 or (last-first)//8 > 100000:
        raise ValueError('Unstable or unexpected native vector header')
    return (last-first)//8


def snapshot(read):
    header=read(TASK_QUEUE,40)
    first,last,capacity,_,current=struct.unpack('<5Q',header)
    result=dict(queued_records=vector_count(first,last,capacity),current_record=hex(current))
    if current:
        group=struct.unpack('<Q',read(current,8))[0]
        if group:
            data=read(group,0x38)
            first,last,capacity=struct.unpack_from('<3Q',data,8)
            count=vector_count(first,last,capacity)
            index,limit=struct.unpack_from('<2I',data,0x28)
            if index>count or limit>count or data[0x30]>1:
                raise ValueError('Unstable or unexpected task-group counters')
            result.update(task_group=hex(group),task_count=count,task_index=index,
                          ready_limit=limit,producer_finished=bool(data[0x30]))
    if read(TASK_QUEUE,40)!=header:
        raise ValueError('Native queue changed while observing it')
    return result


def find_process():
    found=[]
    for path in Path('/proc').iterdir():
        if not path.name.isdigit():
            continue
        try:
            if os.readlink(path/'exe')=='/work/FS15GE6LinuxBoot':
                found.append(int(path.name))
        except OSError:
            pass
    if len(found)!=1:
        raise RuntimeError('Exactly one private VM GE6 process is required')
    return found[0]


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pid',type=int)
    parser.add_argument('--samples',type=int,default=4)
    parser.add_argument('--interval',type=float,default=30)
    args=parser.parse_args()
    if not 1<=args.samples<=20 or not 0<=args.interval<=60 or (args.pid is not None and args.pid<1):
        parser.error('Invalid observation bounds')
    pid=args.pid if args.pid is not None else find_process()
    with open(f'/proc/{pid}/mem','rb',buffering=0) as memory:
        def read(address,size):
            data=os.pread(memory.fileno(),size,address)
            if len(data)!=size:
                raise ValueError('Incomplete process-memory read')
            return data
        for index in range(args.samples):
            result=dict(monotonic_seconds=round(time.monotonic(),3),pid=pid)
            try:
                result.update(snapshot(read))
            except (OSError,ValueError,struct.error) as error:
                result['observation_unavailable']=str(error)
            print(json.dumps(result),flush=True)
            if index+1<args.samples:
                time.sleep(args.interval)


if __name__=='__main__':
    main()
